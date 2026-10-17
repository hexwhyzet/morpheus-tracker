export class FPSCounter {
    private lastTime = performance.now();
    private frames = 0;
    private fps = 0;
    private name: string;

    constructor(name: string) {
        this.name = name;
    }

    tick(): void {
        this.frames++;
        const now = performance.now();

        if (now - this.lastTime >= 1000) {
            this.fps = this.frames;
            this.frames = 0;
            this.lastTime = now;
            // console.log(`${this.name} FPS:`, this.fps);
        }
    }

    getFPS(): number {
        return this.fps;
    }

    logTime(duration: number): void {
        console.log(`${this.name} time:`, duration);
    }

    updateDisplay(elementId: string): void {
        const element = document.getElementById(elementId);
        if (element) {
            element.textContent = `${this.name} FPS: ${this.fps}`;
        }
    }
}