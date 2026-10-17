import * as Comlink from 'comlink';

import {AtomicFlag} from '../atomic-flag';
import {FrameState} from '../frame-buffer-manager';
import {SharedTrackResult} from '../image-tracker';

import WasmWorker from './wasm.worker?worker';

import type {TrackerWorkerInterface, FrameBufferData, FlagData, SharedBufferInfo} from '../types/tracker-worker';
import type {WasmWorkerInterface} from '../types/wasm-worker';


class TrackerWorker implements TrackerWorkerInterface {
    private wasmWorker: WasmWorkerInterface;
    private flags: AtomicFlag[] = [];
    private frameBuffers: FrameBufferData[] = [];
    private results: SharedTrackResult[] = [];
    private resultBuffers: FrameBufferData[] = [];
    private searchBuffer!: FrameBufferData;
    private searchResultBuffer!: FrameBufferData;
    private sab!: SharedArrayBuffer; // Keep a reference to the SharedArrayBuffer
    private isProcessing = false;
    private bufferCount!: number;
    private isLastFrameValid: boolean = false;
    private isTemplatePassed: boolean = false;
    private isSearching: boolean = false;

    constructor() {
        const worker = new WasmWorker();
        this.wasmWorker = Comlink.wrap(worker);
    }

    async create(debug: boolean = false): Promise<void> {
        await this.wasmWorker.create(debug);
    }

    async setOpenCVThreads(n: number): Promise<void> {
        await this.wasmWorker.setOpenCVThreads(n);
    }

    async getOpenCVThreads(): Promise<number> {
        return this.wasmWorker.getOpenCVThreads();
    }

    async init(
        frameWidth: number,
        frameHeight: number,
        bufferCount: number,
    ): Promise<SharedBufferInfo> {
        await this.wasmWorker.init(frameWidth, frameHeight);

        return await this.initBuffers(frameWidth, frameHeight, bufferCount);
    }

    private async initBuffers(
        frameWidth: number,
        frameHeight: number,
        bufferCount: number
    ): Promise<SharedBufferInfo> {
        this.bufferCount = Math.max(bufferCount, 2);

        const frameSize = frameWidth * frameHeight;
        const sab = await this.wasmWorker.getSab();
        this.sab = sab;

        this.frameBuffers = [];
        this.flags = [];
        this.results = [];
        this.resultBuffers = [];
        const flagsData: FlagData[] = [];

        const resultSize = 8 + 9 * 8 + 8 * 8; // 8 bytes (bool + padding) + 72 bytes (H) + 64 bytes (corners)

        const sizes: number[] = [];
        for (let i = 0; i < this.bufferCount; i++) sizes.push(frameSize, 4, resultSize);
        sizes.push(frameSize, resultSize);

        const allocated = await Promise.all(
            sizes.map(s => this.wasmWorker.allocateSharedFrame(s)),
        );

        for (let i = 0; i < this.bufferCount; i++) {
            const frame = allocated[i * 3];
            const flagBuf = allocated[i * 3 + 1];
            const result = allocated[i * 3 + 2];

            this.frameBuffers.push({framePtr: frame.framePtr, size: frame.size});
            flagsData.push({flagPtr: flagBuf.framePtr, size: flagBuf.size});

            const flag = new AtomicFlag(sab, flagBuf.framePtr);
            flag.state = FrameState.IDLE;
            this.flags.push(flag);

            this.resultBuffers.push({framePtr: result.framePtr, size: result.size});
            this.results.push(new SharedTrackResult(sab, result.framePtr));
        }

        // An extra buffer dedicated to the search pass
        const search = allocated[this.bufferCount * 3];
        const searchResult = allocated[this.bufferCount * 3 + 1];
        this.searchBuffer = {framePtr: search.framePtr, size: search.size};
        this.searchResultBuffer = {framePtr: searchResult.framePtr, size: searchResult.size};

        return {
            sab,
            frameBuffers: this.frameBuffers,
            flags: flagsData,
            results: this.resultBuffers,
        };
    }

    async initTemplate(
        templateData: Uint8Array,
        templateWidth: number,
        templateHeight: number,
        isGrey: boolean,
    ): Promise<void> {
        this.isLastFrameValid = false;
        this.isTemplatePassed = true;
        this.isSearching = false;
        await this.wasmWorker.initTemplate(templateData, templateWidth, templateHeight, isGrey);
    }

    async startProcessing(): Promise<void> {
        if (this.isProcessing) {
            console.warn('Processing already started');
            return;
        }

        this.isProcessing = true;

        while (this.isProcessing) {
            for (let i = 0; i < this.bufferCount; i++) {
                if (!this.isProcessing) break;

                const flag = this.flags[i];
                const buffer = this.frameBuffers[i];
                const result = this.results[i];
                const resultBuffer = this.resultBuffers[i];

                flag.waitUntil(FrameState.READY);

                flag.markProcessing();

                if (!this.isLastFrameValid) {
                    result.valid = false;
                    flag.markCompleted();

                    if (!this.isTemplatePassed) {
                        continue;
                    }

                    if (this.isSearching) {
                        continue;
                    }

                    // Copy the frame into the search buffer
                    const sourceFrame = new Uint8Array(this.sab, buffer.framePtr, buffer.size);
                    const searchFrame = new Uint8Array(this.sab, this.searchBuffer.framePtr, this.searchBuffer.size);
                    searchFrame.set(sourceFrame);

                    // Kick off the search without awaiting it
                    this.isSearching = true;
                    this.wasmWorker.processFrameResultPtr(this.searchBuffer.framePtr, this.searchBuffer.size, this.searchResultBuffer.framePtr).then(() => {
                        const searchResult = new SharedTrackResult(this.sab, this.searchResultBuffer.framePtr);
                        this.isLastFrameValid = searchResult.valid;
                        this.isSearching = false;
                    });
                    continue;
                }

                // Already locked on: process the frame without the search detour
                await this.wasmWorker.processFrameResultPtr(buffer.framePtr, buffer.size, resultBuffer.framePtr);
                this.isLastFrameValid = result.valid;
                flag.markCompleted();
            }

            // The loop never ends, so yield periodically to let other calls into the worker
            await new Promise(resolve => setTimeout(resolve, 0));
        }
    }

    async stopProcessing(): Promise<void> {
        this.isProcessing = false;
    }
}

const trackerWorker = new TrackerWorker();
Comlink.expose(trackerWorker);
