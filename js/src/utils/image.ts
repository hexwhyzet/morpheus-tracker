import type {DebugImage} from "./debug.ts"

export async function fetchImageAsUint8(url: string): Promise<{ data: Uint8Array, width: number, height: number }> {
    const img = new Image();
    img.crossOrigin = 'anonymous';
    img.src = url;

    await img.decode();

    const canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height;

    const ctx = canvas.getContext('2d')!;
    ctx.drawImage(img, 0, 0);

    const imageData = ctx.getImageData(0, 0, img.width, img.height);
    const rgba = imageData.data;
    const gray = new Uint8Array(img.width * img.height);

    for (let i = 0, j = 0; i < rgba.length; i += 4, j++) {
        const r = rgba[i];
        const g = rgba[i + 1];
        const b = rgba[i + 2];
        gray[j] = Math.round(0.299 * r + 0.587 * g + 0.114 * b);
    }

    return {
        data: gray,
        width: img.width,
        height: img.height
    };
}

export function convertMatToPngBase64(debugImage: DebugImage): string {
    const byteString = atob(debugImage.base64);
    const raw = new Uint8ClampedArray(byteString.length);

    for (let i = 0; i < byteString.length; i++) {
        raw[i] = byteString.charCodeAt(i);
    }

    const imageData = new ImageData(debugImage.width, debugImage.height);
    for (let i = 0; i < debugImage.width * debugImage.height; i++) {
        if (debugImage.channels === 1) {
            const gray = raw[i];
            imageData.data[4 * i] = gray;
            imageData.data[4 * i + 1] = gray;
            imageData.data[4 * i + 2] = gray;
            imageData.data[4 * i + 3] = 255;
        } else if (debugImage.channels === 2) {
            const gray = Math.abs(raw[2 * i] + raw[2 * i + 1]) / 2; // it's scharr gradient
            imageData.data[4 * i] = gray;
            imageData.data[4 * i + 1] = gray;
            imageData.data[4 * i + 2] = gray;
            imageData.data[4 * i + 3] = 255;
        } else if (debugImage.channels === 3) {
            imageData.data[4 * i] = raw[3 * i];
            imageData.data[4 * i + 1] = raw[3 * i + 1];
            imageData.data[4 * i + 2] = raw[3 * i + 2];
            imageData.data[4 * i + 3] = 255;
        } else if (debugImage.channels === 4) {
            imageData.data[4 * i] = raw[4 * i];
            imageData.data[4 * i + 1] = raw[4 * i + 1];
            imageData.data[4 * i + 2] = raw[4 * i + 2];
            imageData.data[4 * i + 3] = raw[4 * i + 3];
        } else {
            console.warn(`Unsupported channel count: ${debugImage.channels}`);
        }
    }

    const canvas = document.createElement("canvas");
    canvas.width = debugImage.width;
    canvas.height = debugImage.height;
    const ctx = canvas.getContext("2d");
    if (!ctx) {
        throw new Error("Cannot get 2D context from canvas");
    }

    ctx.putImageData(imageData, 0, 0);

    return canvas.toDataURL("image/png").split(",")[1];
}
