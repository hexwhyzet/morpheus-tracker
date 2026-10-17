import { computeTransform, type ScaleMode } from './transform';

export function strokeQuad(
    ctx: CanvasRenderingContext2D,
    pts: Float64Array | number[],
    frameWidth: number,
    frameHeight: number,
    mode: ScaleMode
) {
    if (!ctx || !pts || pts.length < 8) {
        console.warn('[strokeQuad] Invalid parameters', { ctx: !!ctx, pts: pts?.length });
        return;
    }

    const cw = ctx.canvas.width, ch = ctx.canvas.height;
    const {s, sx, sy, dx, dy} = computeTransform(cw, ch, frameWidth, frameHeight, mode);
    const X = (i: number) => (pts[i] - sx) * s + dx;
    const Y = (i: number) => (pts[i + 1] - sy) * s + dy;

    ctx.save();
    ctx.strokeStyle = 'lime';
    ctx.lineWidth = 2;

    ctx.beginPath();
    ctx.moveTo(X(0), Y(0));
    ctx.lineTo(X(2), Y(2));
    ctx.lineTo(X(4), Y(4));
    ctx.lineTo(X(6), Y(6));
    ctx.closePath();
    ctx.stroke();

    ctx.restore();
}

export function drawDebugSquare(ctx: CanvasRenderingContext2D, frameIndex: number) {
    const cw = ctx.canvas.width;
    const ch = ctx.canvas.height;

    const size = 100;
    const x = (cw - size) / 2;
    const y = (ch - size) / 2;

    ctx.save();
    ctx.fillStyle = 'red';
    ctx.fillRect(x, y, size, size);

    // Frame number, so individual frames can be told apart
    ctx.fillStyle = 'white';
    ctx.font = '20px monospace';
    ctx.textAlign = 'center';
    ctx.fillText(`${frameIndex}`, x + size/2, y + size/2 + 7);

    ctx.restore();
}