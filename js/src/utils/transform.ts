export type ScaleMode = 'fit' | 'fill';

export function computeTransform(
    canvasW: number, canvasH: number,
    frameW: number, frameH: number,
    mode: ScaleMode
) {
    const ratioW = canvasW / frameW;
    const ratioH = canvasH / frameH;

    if (mode === "fit") {
        const s = Math.min(ratioW, ratioH);            // scale without cropping (upscaling allowed)
        const dx = (canvasW - frameW * s) * 0.5;       // center it
        const dy = (canvasH - frameH * s) * 0.5;
        return {
            s, sx: 0, sy: 0,
            dx, dy,
            sw: frameW, sh: frameH,                     // src rect (uncropped)
            dw: frameW * s, dh: frameH * s,             // dst rect
        };
    }

    // mode === "fill" (cover): cover the whole canvas, cropping if needed
    const s = Math.max(ratioW, ratioH);
    const sw = canvasW / s;                         // how much source we need horizontally
    const sh = canvasH / s;                         // and vertically
    const sx = (frameW - sw) * 0.5;                 // centered crop
    const sy = (frameH - sh) * 0.5;
    return {
        s, sx, sy,
        dx: 0, dy: 0,
        sw, sh,                                        // src rect (cropped)
        dw: canvasW, dh: canvasH,                      // dst rect (the whole canvas)
    };
}