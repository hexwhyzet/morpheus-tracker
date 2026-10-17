import * as Comlink from 'comlink';

import {SharedTrackResult} from './image-tracker';

import {FrameBufferManager, FrameState} from "./frame-buffer-manager";
import {GrayscaleRenderer} from "./grayscale";
import {strokeQuad} from "./utils/canvas-drawing";
import {FPSCounter} from "./utils/fps-counter";
import {computeTransform, type ScaleMode} from "./utils/transform";
import TrackerWorker from './workers/tracker.worker?worker';

import type {TrackerWorkerInterface} from "./types/tracker-worker";

/** Target pose for the current frame. The buffers are views into shared memory; copy them if you keep them. */
export interface TrackingPose {
    valid: boolean;
    H: Float64Array;
    corners: Float64Array;
}

export interface TrackingStats {
    fps: number;
    displayFps: number;
}

export interface TrackingLoopOptions {
    /** Canvas that receives the video plus the debug overlay. */
    canvas: HTMLCanvasElement;
    /** An existing video element. If omitted, we create one via getUserMedia. */
    video?: HTMLVideoElement;
    /** How many frame buffers to keep in rotation. At least 2. */
    bufferCount?: number;
    /** Outline the detected target with a green quad. */
    drawDetectionQuad?: boolean;
    /** Verbose WASM logs plus the __cvThreads debug hook on window. */
    debug?: boolean;
    /** OpenCV thread count. Capped by the build's PTHREAD_POOL_SIZE (8). */
    threads?: number;
    /**
     * Camera frame rate, 30 by default. It also caps the tracking FPS:
     * processing is driven from videoFrameCallback, i.e. by camera frames.
     * The requested value is not a guarantee — the device may deliver less.
     */
    frameRate?: number;
    /** Called on every frame with the finished tracking result. */
    onPose?: (pose: TrackingPose) => void;
    /** Called once per frame with the current FPS. */
    onStats?: (stats: TrackingStats) => void;
}


export class TrackingLoop {
    private video!: HTMLVideoElement;
    private canvas!: HTMLCanvasElement;
    private glCanvas!: HTMLCanvasElement;
    private ctx!: CanvasRenderingContext2D;
    private trackerWorker: TrackerWorkerInterface;

    private grayscale!: GrayscaleRenderer;
    private frameBufferManager!: FrameBufferManager;

    private bufferCount = 3;

    private readonly ownsVideo: boolean;
    private readonly drawDetectionQuad: boolean;
    private readonly debug: boolean;
    private readonly threads: number | undefined;
    private readonly frameRate: number;
    private readonly onPose: ((pose: TrackingPose) => void) | undefined;
    private readonly onStats: ((stats: TrackingStats) => void) | undefined;

    private frameWidth!: number;
    private frameHeight!: number;

    // Video and template dimensions. Unused by the loop itself — they are read
    // by code that builds a pose from the homography, which needs K and the
    // physical size of the target.
    public videoWidth!: number;
    public videoHeight!: number;

    public templateWidth!: number;
    public templateHeight!: number;

    // Sequential counters for thread safety - no frame skipping
    private writeSlotIndex = 0;  // Frame writes
    private displaySlotIndex = 0; // Frame display

    // FPS counters for different stages
    private captureFPS = new FPSCounter("Capture");
    private processingFPS = new FPSCounter("Processing");
    private displayFPS = new FPSCounter("Display");

    // Canvas array for frame storage during processing
    private frameCanvases: HTMLCanvasElement[] = [];
    private frameContexts: CanvasRenderingContext2D[] = [];

    // Results from shared memory
    private resultBuffers: SharedTrackResult[] = [];


    private videoReady!: Promise<void>;
    private wasmReady!: Promise<void>;
    private frameBuffersReady!: Promise<void>;
    private resolveVideo!: () => void;
    private resolveWasm!: () => void;
    private resolveFrameBuffers!: () => void;

    private scaleMode: ScaleMode = 'fill';

    constructor(options: TrackingLoopOptions) {
        this.canvas = options.canvas;
        this.ownsVideo = !options.video;
        if (options.video) this.video = options.video;
        this.drawDetectionQuad = options.drawDetectionQuad ?? false;
        this.debug = options.debug ?? false;
        this.threads = options.threads;
        this.frameRate = options.frameRate ?? 30;
        this.onPose = options.onPose;
        this.onStats = options.onStats;

        const worker = new TrackerWorker();

        worker.onerror = (error) => {
            console.error('Worker error:', error);
        };
        worker.onmessageerror = (error) => {
            console.error('Worker message error:', error);
        };

        this.trackerWorker = Comlink.wrap(worker);
        this.bufferCount = Math.max(options.bufferCount ?? 3, 2);

        // Debug hook for changing the OpenCV thread count from the console
        // without a rebuild: __cvThreads(4) sets it, __cvThreads() reads it.
        if (this.debug) {
            (window as any).__cvThreads = async (n?: number) => {
                if (typeof n === 'number') await this.trackerWorker.setOpenCVThreads(n);
                const current = await this.trackerWorker.getOpenCVThreads();
                console.log(`[OpenCV] threads=${current}`);
                return current;
            };
        }

        this.frameBufferManager = new FrameBufferManager(this.bufferCount);

        this.videoReady = new Promise(r => this.resolveVideo = r);
        this.wasmReady = new Promise(r => this.resolveWasm = r);
        this.frameBuffersReady = new Promise(r => this.resolveFrameBuffers = r);
    }

    /**
     * Brings up the canvases, video, worker and frame buffers. Resolves once
     * the frame pipeline is running.
     */
    public async init(): Promise<void> {
        this.setupCanvas();
        if (this.ownsVideo) {
            void this.setupVideo();
        } else {
            if (this.video.readyState >= 1) {
                this.resolveVideo();
            } else {
                this.video.addEventListener('loadedmetadata', () => this.resolveVideo(), {once: true});
            }
        }
        void this.setupWorker();

        await Promise.all([this.wasmReady, this.videoReady]).then(() => {
            // Safari 26: scroll to hide bottom bar, then resize canvas after layout settles
            if (document.documentElement.classList.contains('safari-26')) {
                window.scrollTo(0, 62);
                requestAnimationFrame(() => requestAnimationFrame(() => this.resizeCanvas()));
            }

            this.frameWidth = this.video.videoWidth;
            this.frameHeight = this.video.videoHeight;

            this.videoWidth = this.video.videoWidth;
            this.videoHeight = this.video.videoHeight;

            this.grayscale = new GrayscaleRenderer(
                this.glCanvas,
                this.frameWidth,
                this.frameHeight,
                this.video.videoWidth,
                this.video.videoHeight,
            );

            return this.trackerWorker.init(this.frameWidth, this.frameHeight, this.bufferCount).then((sharedBufferInfo) => {
                this.frameBufferManager.initialize(
                    sharedBufferInfo.sab,
                    sharedBufferInfo.frameBuffers,
                    sharedBufferInfo.flags,
                );

                // Create views over the results in shared memory
                this.resultBuffers = [];
                for (let i = 0; i < this.bufferCount; i++) {
                    const result = sharedBufferInfo.results[i];
                    this.resultBuffers.push(new SharedTrackResult(sharedBufferInfo.sab, result.framePtr));
                }

                this.createFrameCanvases();

                this.video.requestVideoFrameCallback(() => this.videoFrameCallback());

                this.startRenderLoop();


                this.resolveFrameBuffers();
            });
        });
    }

    private async setupWorker() {
        try {
            await this.trackerWorker.create(this.debug);
            if (typeof this.threads === 'number') {
                await this.trackerWorker.setOpenCVThreads(this.threads);
            }
            this.resolveWasm();
        } catch (error) {
            console.error('Failed to initialize worker:', error);
        }
    }

    public switchScaleMode(): void {
        this.scaleMode = this.scaleMode == 'fit' ? 'fill' : 'fit';
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }

    /**
     * The template the tracker will look for. Restarts processing.
     *
     * templateData is RGBA, 4 bytes per pixel (like ImageData.data): below we
     * call initTemplate(..., isGrey = false), and WASM does the grayscale
     * conversion itself. Do not pass an already-grayscale buffer here —
     * parseMat would read it as RGBA and the tracker would search for garbage.
     */
    public async setTemplate(templateData: Uint8Array, templateWidth: number, templateHeight: number): Promise<void> {
        return this.initTemplate(templateData, templateWidth, templateHeight);
    }

    /**
     * Reinitializes the tracker with a new template
     */
    private async initTemplate(templateData: Uint8Array, templateWidth: number, templateHeight: number): Promise<void> {
        Promise.all([this.wasmReady, this.frameBuffersReady])
            .then(async () => {
                try {
                    await this.trackerWorker.stopProcessing();

                    this.templateWidth = templateWidth;
                    this.templateHeight = templateHeight;

                    await this.trackerWorker.initTemplate(
                        templateData,
                        templateWidth,
                        templateHeight,
                        false,
                    );

                    await this.startProcessing();
                } catch (error) {
                    console.error('Failed to reinitialize tracker:', error);
                }
            })
            .catch(error => {
                console.error('Failed to initialize template:', error);
            });
    }

    private resizeCanvas() {
        const isSafari26 = document.documentElement.classList.contains('safari-26');
        let w: number;
        let h: number;
        if (isSafari26) {
            const rect = this.canvas.getBoundingClientRect();
            w = Math.round(rect.width) || window.innerWidth;
            h = Math.round(rect.height) || window.innerHeight;
        } else {
            w = window.innerWidth;
            h = window.innerHeight;
        }
        this.canvas.width = w;
        this.canvas.height = h;
        this.frameCanvases.forEach(c => { c.width = w; c.height = h; });
    }

    private createFrameCanvases(): void {
        this.frameCanvases.forEach(canvas => {
            if (canvas.parentElement) {
                canvas.parentElement.removeChild(canvas);
            }
        });
        this.frameCanvases = [];
        this.frameContexts = [];

        for (let i = 0; i < this.bufferCount; i++) {
            const canvas = document.createElement('canvas');
            canvas.id = `frame-canvas-${i}`;
            canvas.style.position = 'absolute';
            canvas.style.display = 'none';
            canvas.style.top = '0';
            canvas.style.left = '0';
            canvas.style.pointerEvents = 'none';
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;

            canvas.style.willChange = 'auto';
            canvas.style.imageRendering = 'auto';

            const ctx = canvas.getContext('2d', {
                alpha: false,
                desynchronized: true,
            });
            if (!ctx) {
                throw new Error('Failed to get 2D context');
            }

            this.frameCanvases.push(canvas);
            this.frameContexts.push(ctx);

            document.body.appendChild(canvas);
        }
    }

    private setupCanvas() {
        this.glCanvas = document.createElement('canvas');
        this.glCanvas.id = 'gl';
        this.glCanvas.style.display = 'none';
        document.body.appendChild(this.glCanvas);

        this.canvas.style.pointerEvents = 'none';
        this.canvas.style.willChange = 'auto';
        this.canvas.style.imageRendering = 'auto';

        this.ctx = this.canvas.getContext('2d', {
            alpha: false,
            desynchronized: true
        })!;

        this.resizeCanvas();
        window.addEventListener('resize', () => {
            this.resizeCanvas();
        });
    }

    private async setupVideo() {
        this.video = document.createElement('video');
        this.video.autoplay = true;
        this.video.muted = true;
        this.video.playsInline = true;
        this.video.style.display = 'none';
        document.body.appendChild(this.video);

        // The camera clocks the whole pipeline: videoFrameCallback fires once
        // per camera frame, so frameRate here is the tracking FPS ceiling.
        const constraints = {
            video: {
                facingMode: 'environment',
                focusMode: {ideal: 'continuous'},
                // ideal, not max: max only sets a ceiling without requiring
                // the camera to reach it — Safari happily handed back 30 under
                // max: 30. There is also a known WebKit quirk (bug 210186)
                // where asking for frameRate: 60 specifically works more
                // reliably than lower values or none at all.
                frameRate: {ideal: this.frameRate},
                // Above 30 fps the size goes through min rather than ideal:
                // ideal competes with the frame rate and the camera settles on
                // 1080p@30, while omitting the size entirely drops it to the
                // smallest mode, which can certainly do 60. min sets a floor
                // and leaves the upper choice to the camera.
                // All hints, no hard minimum: a `min` the camera cannot meet
                // throws OverconstrainedError, and the retry below costs a
                // second camera start — seconds of it on a phone.
                //
                // 1440x1080 is the mode iPhones expose at 60 fps. It is 4:3,
                // which is why asking for 1280x720 misses it and lands back
                // on 1080p30.
                ...(this.frameRate > 30
                    ? {width: {ideal: 1440}, height: {ideal: 1080}}
                    : {width: {ideal: 1920}, height: {ideal: 1080}}),
            },
            audio: false,
        };

        // Some devices answer with OverconstrainedError when the requested
        // resolution/frame-rate pair does not exist. In that case retry without
        // the size and let the camera pick — any mode beats a black screen.
        navigator.mediaDevices.getUserMedia(constraints as MediaStreamConstraints)
            .catch(err => {
                if (err?.name === 'NotAllowedError' || err?.name === 'SecurityError') throw err;
                console.warn(`Camera rejected the constraints (${err?.name}), retrying without a size`);
                const {width, height, ...video} = constraints.video;
                return navigator.mediaDevices.getUserMedia({video, audio: false} as MediaStreamConstraints);
            })
            .then(stream => {
                this.video.srcObject = stream;

                if (this.debug) {
                    navigator.mediaDevices.enumerateDevices().then(devices => {
                        console.log("devices", devices);
                    });

                    const videoTrack = stream.getVideoTracks()[0];
                    if (videoTrack) {
                        const settings = videoTrack.getSettings();
                        console.log(
                            `Camera: ${settings.width}x${settings.height} @ ${settings.frameRate} fps`,
                        );
                        console.log("Camera settings:", settings);

                        const capabilities = videoTrack.getCapabilities();
                        console.log("Camera capabilities:", capabilities);
                    }
                }

                this.video.onloadedmetadata = () => {
                    this.video.play();
                    this.resolveVideo();
                };
            })
            .catch(err => {
                console.error('getUserMedia error:', err);
                console.error('Error name:', err.name);
                console.error('Error message:', err.message);
            });
    }

    private videoFrameCallback() {
        const slot = this.frameBufferManager.getSlot(this.writeSlotIndex);
        if (slot && slot.flag.state === FrameState.IDLE) {
            const sab = this.frameBufferManager.getSab();
            const frameData = new Uint8Array(sab, slot.framePtr, slot.size);

            this.grayscale.render(this.video, frameData);

            const ctx = this.frameContexts[this.writeSlotIndex];
            if (ctx) {
                drawFrame(ctx, this.video, this.video.videoWidth, this.video.videoHeight, this.scaleMode);
            }

            slot.flag.markReady();

            this.writeSlotIndex = (this.writeSlotIndex + 1) % this.bufferCount;

            this.captureFPS.tick();
        }

        this.video.requestVideoFrameCallback(() => this.videoFrameCallback());
    }

    /** Start the processing loop in the worker. */
    public async start(): Promise<void> {
        await this.frameBuffersReady;
        await this.startProcessing();
    }

    public async stop(): Promise<void> {
        await this.trackerWorker.stopProcessing();
    }

    private async startProcessing() {
        void this.trackerWorker.startProcessing();
    }

    /** OpenCV thread count. Change it rarely, not per frame. */
    public async setOpenCVThreads(n: number): Promise<void> {
        await this.trackerWorker.setOpenCVThreads(n);
    }

    public async getOpenCVThreads(): Promise<number> {
        return this.trackerWorker.getOpenCVThreads();
    }

    private startRenderLoop(): void {
        const render = () => {
            const slot = this.frameBufferManager.getSlot(this.displaySlotIndex);
            if (slot && slot.flag.state === FrameState.COMPLETED) {
                const result = this.resultBuffers[this.displaySlotIndex];

                const frameCtx = this.frameContexts[this.displaySlotIndex];
                if (frameCtx && result.valid && result.corners) {
                    if (this.drawDetectionQuad) {
                        strokeQuad(frameCtx, result.corners, this.frameWidth, this.frameHeight, this.scaleMode);
                    }
                }

                // Fires on every processed frame, lock or no lock, so the
                // consumer learns about a loss as well as a find.
                this.onPose?.({valid: result.valid, H: result.H, corners: result.corners});

                if (this.frameCanvases[this.displaySlotIndex]) {
                    this.ctx.drawImage(this.frameCanvases[this.displaySlotIndex], 0, 0);
                }

                slot.flag.markDisplayed();

                const prevIndex = (this.displaySlotIndex - 1 + this.bufferCount) % this.bufferCount;
                const prevSlot = this.frameBufferManager.getSlot(prevIndex);
                if (prevSlot && prevSlot.flag.state === FrameState.DISPLAYED) {
                    prevSlot.flag.markIdle();
                }

                this.displaySlotIndex = (this.displaySlotIndex + 1) % this.bufferCount;

                this.processingFPS.tick();
                this.displayFPS.tick();

                this.onStats?.({fps: this.captureFPS.getFPS(), displayFps: this.displayFPS.getFPS()});
            }

            requestAnimationFrame(render);
        };

        requestAnimationFrame(render);
    }

}

export function drawFrame(
    ctx: CanvasRenderingContext2D,
    source: CanvasImageSource,
    frameW: number,
    frameH: number,
    mode: ScaleMode,
    clear: boolean = true
) {
    const cw = ctx.canvas.width, ch = ctx.canvas.height;
    const t = computeTransform(cw, ch, frameW, frameH, mode);

    if (clear) {
        ctx.clearRect(0, 0, cw, ch);
    }

    ctx.drawImage(source, t.sx, t.sy, t.sw, t.sh, t.dx, t.dy, t.dw, t.dh);
}
