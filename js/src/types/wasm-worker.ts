import type {TrackResult} from '../image-tracker';

export interface WasmWorkerInterface {
    create(debug?: boolean): Promise<void>;

    init(frameWidth: number, frameHeight: number): Promise<void>;

    initTemplate(
        templateData: Uint8Array,
        templateWidth: number,
        templateHeight: number,
        isGrey: boolean,
    ): Promise<void>;

    processFrame(
        framePtr: number,
        size: number
    ): Promise<TrackResult>;

    processFrameResultPtr(
        framePtr: number,
        size: number,
        resultPtr: number
    ): Promise<void>;

    setOpenCVThreads(n: number): Promise<void>;

    getOpenCVThreads(): Promise<number>;

    getSab(): Promise<SharedArrayBuffer>;

    allocateSharedFrame(size: number): Promise<{framePtr: number, size: number}>;
}