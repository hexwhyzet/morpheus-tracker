import { AtomicFlag } from './atomic-flag';

// const assertions instead of an enum, for erasableSyntaxOnly compatibility
export const FrameState = {
    IDLE: 0 as const,           // Buffer is free, ready to be written
    READY: 1 as const,          // Frame written, ready for processing
    PROCESSING: 2 as const,     // Frame is being processed in the worker
    COMPLETED: 3 as const,      // Processing done, ready to be displayed
    DISPLAYED: 4 as const,      // Frame displayed, safe to release
} as const;

// Explicit type declaration, for erasableSyntaxOnly compatibility
export type FrameState = 0 | 1 | 2 | 3 | 4;

// Runtime helpers for working with the states
export const FRAME_STATE_NAMES = {
    [FrameState.IDLE]: 'Idle',
    [FrameState.READY]: 'Ready',
    [FrameState.PROCESSING]: 'Processing',
    [FrameState.COMPLETED]: 'Completed',
    [FrameState.DISPLAYED]: 'Displayed'
} as const;

export function getFrameStateName(state: FrameState): string {
    return FRAME_STATE_NAMES[state] ?? `Unknown(${state})`;
}

export function isValidFrameState(value: number): value is FrameState {
    return value >= 0 && value <= 4 && Number.isInteger(value);
}

export interface FrameBufferSlot {
    framePtr: number;
    size: number;
    flagPtr: number;
    flag: AtomicFlag;
}

export class FrameBufferManager {
    private slots: FrameBufferSlot[] = [];
    private sab!: SharedArrayBuffer;
    private readonly bufferCount;

    constructor(bufferCount: number = 2) {
        if (bufferCount < 2) {
            throw new Error('Buffer count must be at least 2');
        }
        this.bufferCount = bufferCount;
    }

    /**
     * Initializes the buffers from the data handed over by the worker
     */
    initialize(
        sab: SharedArrayBuffer,
        frameBuffers: Array<{ framePtr: number; size: number }>,
        flags: Array<{ flagPtr: number; size: number }>
    ) {
        this.sab = sab;
        this.slots = [];

        for (let i = 0; i < this.bufferCount; i++) {
            const frameBuffer = frameBuffers[i];
            const flagData = flags[i];

            const flag = new AtomicFlag(sab, flagData.flagPtr);
            flag.state = FrameState.IDLE;

            this.slots.push({
                framePtr: frameBuffer.framePtr,
                size: frameBuffer.size,
                flagPtr: flagData.flagPtr,
                flag
            });
        }
    }

    getSlot(index: number): FrameBufferSlot | null {
        return this.slots[index] || null;
    }

    getBufferCount(): number {
        return this.bufferCount;
    }

    /**
     * Counts how many slots are in each state
     */
    getSlotStates(): Record<FrameState, number> {
        const states = {
            [FrameState.IDLE]: 0,
            [FrameState.READY]: 0,
            [FrameState.PROCESSING]: 0,
            [FrameState.COMPLETED]: 0,
            [FrameState.DISPLAYED]: 0,
        };

        this.slots.forEach(slot => {
            states[slot.flag.state]++;
        });

        return states;
    }

    /**
     * Debug snapshot of every slot
     */
    getDebugInfo() {
        return {
            bufferCount: this.bufferCount,
            slotStates: this.getSlotStates(),
            slots: this.slots.map((slot, index) => ({
                index,
                state: getFrameStateName(slot.flag.state)
            }))
        };
    }

    getSab(): SharedArrayBuffer {
        return this.sab;
    }

    cleanup(): void {
        this.slots = [];
    }
}