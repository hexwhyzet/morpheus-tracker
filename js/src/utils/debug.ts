export interface DebugImage {
    name: string;
    width: number;
    height: number;
    channels: number;
    base64: string;
}

export interface DebugInfo {
    info: Record<string, string>;
    images: DebugImage[];
}