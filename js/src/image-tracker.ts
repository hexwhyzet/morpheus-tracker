import ImageTrackerWASM from "./wasm/img_tracker_wasm.js";

import type {DebugInfo} from "./utils/debug.ts";
// @ts-ignore
import type EmscriptenModule from 'emscripten';

export interface TrackResult {
    valid: boolean;
    H: Float64Array;
    corners: Float64Array;
}

export class SharedTrackResult {
    private readonly validArr: Uint8Array;
    public H: Float64Array;
    public corners: Float64Array;

    constructor(sab: SharedArrayBuffer, ptr: number) {
        this.validArr = new Uint8Array(sab, ptr, 1);
        this.H = new Float64Array(sab, ptr + 8, 9);
        this.corners = new Float64Array(sab, ptr + 8 + 72, 8);
    }

    get valid(): boolean {
        return this.validArr[0] === 1;
    }

    set valid(value: boolean) {
        this.validArr[0] = value ? 1 : 0;
    }
}

export class ImageTracker {
    private _frameWidth!: number;
    private _frameHeight!: number;
    private _Module!: EmscriptenModule;
    private _adoptContext!: (handle: number) => void;
    private _init!: (debug: boolean) => void;
    private _initTemplate!: (templatePtr: number, width: number, height: number, isGrey: boolean) => void;
    private _processFrame!: (imageData: number, width: number, height: number) => number;
    private _processFrameInplaceResult!: (imageData: number, width: number, height: number, resultPtr: number) => void;
    private _processFrameTexture!: (frameTextureId: number, width: number, height: number) => number;
    private _collectDebugData!: () => number;
    private _setOpenCVThreads!: (n: number) => void;
    private _getOpenCVThreads!: () => number;
    private frameImagePtr!: number;
    private templateImagePtr?: number;
    private readonly _debug: boolean;

    private constructor(debug: boolean = false) {
        this._debug = debug;
    }

    private frameLength(): number {
        return this._frameWidth * this._frameHeight;
    }

    public static async create(debug: boolean = false) {
        const tracker = new ImageTracker(debug);
        const Module: EmscriptenModule = await ImageTrackerWASM();
        tracker.onWasmInit(Module);
        if (tracker._debug) console.log("WASM module loaded.");
        return tracker;
    }

    private onWasmInit(Module: EmscriptenModule): void {
        this._Module = Module;

        this._adoptContext = this._Module.cwrap("adoptContext", null, ["number"]);
        this._init = this._Module.cwrap("init", null, ["boolean"]);
        this._initTemplate = this._Module.cwrap("initTemplate", null, ["number", "number", "number", "boolean"]);
        this._processFrame = this._Module.cwrap("processFrame", "number", ["number", "number", "number"]);
        this._processFrameInplaceResult = this._Module.cwrap("processFrameInplaceResult", null, ["number", "number", "number", "number"]);
        this._processFrameTexture = this._Module.cwrap("processFrameTexture", "number", ["number", "number", "number"]);
        this._collectDebugData = this._Module.cwrap("collectDebugData", "number", []);
        this._setOpenCVThreads = this._Module.cwrap("setOpenCVThreads", null, ["number"]);
        this._getOpenCVThreads = this._Module.cwrap("getOpenCVThreads", "number", []);
    }

    /** OpenCV thread count. Set it once after init, not per frame. */
    public setOpenCVThreads(n: number): void {
        this.ensureModuleReady();
        this._setOpenCVThreads(n);
    }

    public getOpenCVThreads(): number {
        this.ensureModuleReady();
        return this._getOpenCVThreads();
    }

    public adoptContext(webGLContextHandle: number) {
        this._adoptContext(webGLContextHandle);
    }

    public registerContext(ctx: WebGL2RenderingContext) {
        this._adoptContext(this._Module.GL.registerContext(ctx))
    }

    public registerNativeTextureId(textureResource: WebGLTexture) {
        const id = this._Module.GL.getNewId(this._Module.GL.textures);
        this._Module.GL.textures[id] = textureResource;

        return id;
    }

    public unregisterNativeTextureId(nativeTextureId: number) {
        const tex = this._Module.GL.textures[nativeTextureId];

        tex.name = 0;
        this._Module.GL.textures[nativeTextureId] = null;

        return tex;
    }

    public init(frameWidth: number, frameHeight: number) {
        this._frameWidth = frameWidth;
        this._frameHeight = frameHeight;

        this._init(this._debug);
    }

    public initTemplate(templateData: Uint8Array, templateWidth: number, templateHeight: number, isGrey: boolean) {
        this.ensureModuleReady();
        if (this._debug) console.log("initTemplate");

        if (this.templateImagePtr) {
            this._Module._free(this.templateImagePtr);
        }
        this.templateImagePtr = this._Module._malloc(templateData.length);

        if (this.templateImagePtr != null) {
            this._Module.HEAPU8.set(templateData, this.templateImagePtr);
            this._initTemplate(this.templateImagePtr, templateWidth, templateHeight, isGrey);
        } else {
            throw new Error("Failed to load template image");
        }
    }

    public processFrame(framePtr: number, size: number): TrackResult {
        this.ensureModuleReady();

        if (size !== this.frameLength()) {
            throw new Error(`Input image size ${size} does not match expected ${this.frameLength()}`);
        }
        if (this._debug) console.log("processFrame");

        const resultPtr = this._processFrame(framePtr, this._frameWidth, this._frameHeight);
        if (this._debug) console.log("Track raw result pointer:", resultPtr);

        const result = this.parseResult(resultPtr);
        if (this._debug) {
            console.log("Track result H:", result.H);
            console.log("Track result valid:", result.valid);
        }
        return result;
    }

    public processFrameResultPtr(framePtr: number, size: number, resultPtr: number): void {
        this.ensureModuleReady();

        if (size !== this.frameLength()) {
            throw new Error(`Input image size ${size} does not match expected ${this.frameLength()}`);
        }
        if (this._debug) console.log("processFrame");

        this._processFrameInplaceResult(framePtr, this._frameWidth, this._frameHeight, resultPtr);
        if (this._debug) console.log("Track raw result pointer:", resultPtr);
    }

    public processFrameTexture(frameTextureId: number): TrackResult {
        this.ensureModuleReady();
        if (this._debug) console.log("processFrameTexture");

        const resultPtr = this._processFrameTexture(frameTextureId, this._frameWidth, this._frameHeight);
        if (this._debug) console.log("Track raw result pointer:", resultPtr);

        const result = this.parseResult(resultPtr);
        if (this._debug) {
            console.log("Track result H:", result.H);
            console.log("Track result valid:", result.valid);
        }
        return result;
    }

    public collectDebugData(): DebugInfo {
        console.log("CollectDebugData JS");
        const ptr = this._collectDebugData();
        return JSON.parse(this._Module.UTF8ToString(ptr));
    }

    public destroy(): void {
        this.ensureModuleReady();
        if (this._debug) console.log("destroy");
        if (this.frameImagePtr) this._Module._free(this.frameImagePtr);
        if (this.templateImagePtr) this._Module._free(this.templateImagePtr);
    }

    private parseResult(ptr: number): TrackResult {
        const valid = this._Module.getValue(ptr, "i8") !== 0;

        // Copy out of the WASM heap into JS memory so the views cannot dangle
        const wasmH = new Float64Array(this._Module.HEAPF64.buffer, ptr + 8, 9);
        const wasmCorners = new Float64Array(this._Module.HEAPF64.buffer, ptr + 80, 8);

        const H = new Float64Array(9);
        const corners = new Float64Array(8);

        H.set(wasmH);
        corners.set(wasmCorners);

        return {valid, H, corners};
    }

    private ensureModuleReady(): void {
        if (!this._Module) {
            throw new Error("WASM module is not initialized");
        }
    }

    public getSab() {
        this.ensureModuleReady();

        return (this._Module as any).wasmMemory
            ? (this._Module as any).wasmMemory.buffer as SharedArrayBuffer
            : (this._Module.HEAPU8.buffer as SharedArrayBuffer);
    }

    public allocateSharedFrame(size: number) {
        this.ensureModuleReady();

        const framePtr: number = this._Module._malloc(size);

        return {
            framePtr,
            size,
        };
    }
}
