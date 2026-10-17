import * as Comlink from 'comlink';

import {ImageTracker, type TrackResult} from '../image-tracker';

import type {WasmWorkerInterface} from '../types/wasm-worker';

class WasmWorker implements WasmWorkerInterface {
    private tracker!: ImageTracker;

    async create(debug: boolean = false): Promise<void> {
        this.tracker = await ImageTracker.create(debug);
    }

    async init(frameWidth: number, frameHeight: number): Promise<void> {
        this.tracker.init(frameWidth, frameHeight);
    }

    async initTemplate(
        templateData: Uint8Array,
        templateWidth: number,
        templateHeight: number,
        isGrey: boolean
    ): Promise<void> {
        this.tracker.initTemplate(templateData, templateWidth, templateHeight, isGrey);
    }

    async processFrame(
        framePtr: number,
        size: number
    ): Promise<TrackResult> {
        return this.tracker.processFrame(framePtr, size);
    }

    async processFrameResultPtr(
        framePtr: number,
        size: number,
        resultPtr: number
    ): Promise<void> {
        this.tracker.processFrameResultPtr(framePtr, size, resultPtr);
    }

    async setOpenCVThreads(n: number): Promise<void> {
        this.tracker.setOpenCVThreads(n);
    }

    async getOpenCVThreads(): Promise<number> {
        return this.tracker.getOpenCVThreads();
    }

    async getSab(): Promise<SharedArrayBuffer> {
        return this.tracker.getSab();
    }

    async allocateSharedFrame(size: number): Promise<{ framePtr: number, size: number }> {
        return this.tracker.allocateSharedFrame(size);
    }
}

const wasmWorker = new WasmWorker();
Comlink.expose(wasmWorker);