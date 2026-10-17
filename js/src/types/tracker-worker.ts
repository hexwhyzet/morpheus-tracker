export interface FrameBufferData {
    framePtr: number;
    size: number;
}

export interface FlagData {
    flagPtr: number;
    size: number;
}

export interface SharedBufferInfo {
    sab: SharedArrayBuffer;
    frameBuffers: FrameBufferData[];
    flags: FlagData[];
    results: FrameBufferData[];
}

export interface TrackerWorkerInterface {
    create(debug?: boolean): Promise<void>;

    init(
        frameWidth: number,
        frameHeight: number,
        bufferCount?: number
    ): Promise<SharedBufferInfo>;

    initTemplate(
        templateData: Uint8Array,
        templateWidth: number,
        templateHeight: number,
        isGrey: boolean,
    ): Promise<void>;

    startProcessing(): Promise<void>;

    stopProcessing(): Promise<void>;

    setOpenCVThreads(n: number): Promise<void>;

    getOpenCVThreads(): Promise<number>;
}