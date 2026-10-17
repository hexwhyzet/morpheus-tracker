import { FrameState, getFrameStateName, isValidFrameState } from './frame-buffer-manager';

/**
 * Wrapper around Atomics operations on frame states.
 * Gives synchronization a type-safe, convenient API.
 */
export class AtomicFlag {
    private buffer: Int32Array;
    private index: number;

    constructor(sharedArrayBuffer: SharedArrayBuffer, byteOffset: number, index: number = 0) {
        this.buffer = new Int32Array(sharedArrayBuffer, byteOffset, 1);
        this.index = index;
    }

    /**
     * Builds an AtomicFlag over an existing Int32Array
     */
    static fromInt32Array(buffer: Int32Array, index: number = 0): AtomicFlag {
        const flag = Object.create(AtomicFlag.prototype);
        flag.buffer = buffer;
        flag.index = index;
        return flag;
    }

    /**
     * Reads the current state without blocking
     */
    get state(): FrameState {
        return Atomics.load(this.buffer, this.index) as FrameState;
    }

    set state(newState: FrameState) {
        Atomics.store(this.buffer, this.index, newState);
    }

    /**
     * Atomically stores a new state and wakes any waiting threads
     */
    setState(newState: FrameState): void {
        Atomics.store(this.buffer, this.index, newState);
        Atomics.notify(this.buffer, this.index);
    }

    /**
     * Blocks until the state equals the expected one
     */
    waitUntil(expectedState: number): void {
        for (;;) {
            if (Atomics.load(this.buffer, this.index) === expectedState) {
                return;
            }

            const v = Atomics.load(this.buffer, this.index);

            Atomics.wait(this.buffer, this.index, v);
        }
    }

    /**
     * Tries to atomically move the state from one value to another.
     * Returns true on success, false if the current state was not the expected one.
     */
    compareAndSwap(expectedState: FrameState, newState: FrameState): boolean {
        const result = Atomics.compareExchange(this.buffer, this.index, expectedState, newState);
        if (result === expectedState) {
            Atomics.notify(this.buffer, this.index);
            return true;
        }
        return false;
    }

    /**
     * Checks whether the state is one of the given ones
     */
    isInState(...states: FrameState[]): boolean {
        return states.includes(this.state);
    }

    /**
     * State transition with validation of the current state
     */
    transitionTo(newState: FrameState, allowedCurrentStates?: FrameState[]): boolean {
        if (allowedCurrentStates) {
            const currentState = this.state;
            if (!allowedCurrentStates.includes(currentState)) {
                console.warn(`Invalid transition: cannot go from ${getFrameStateName(currentState)} to ${getFrameStateName(newState)}`);
                return false;
            }
        }

        this.setState(newState);
        return true;
    }

    /**
     * Safe transitions between states
     */
    markReady(): boolean {
        return this.compareAndSwap(FrameState.IDLE, FrameState.READY);
    }

    markProcessing(): boolean {
        return this.compareAndSwap(FrameState.READY, FrameState.PROCESSING);
    }

    markCompleted(): boolean {
        return this.compareAndSwap(FrameState.PROCESSING, FrameState.COMPLETED);
    }

    markDisplayed(): boolean {
        return this.compareAndSwap(FrameState.COMPLETED, FrameState.DISPLAYED);
    }

    markIdle(): boolean {
        return this.transitionTo(FrameState.IDLE, [FrameState.DISPLAYED, FrameState.COMPLETED]);
    }

    toString(): string {
        const stateName = getFrameStateName(this.state);
        return `AtomicFlag(${stateName})`;
    }

    getDebugInfo() {
        return {
            state: this.state,
            stateName: getFrameStateName(this.state),
            bufferLength: this.buffer.length,
            index: this.index,
            isValidState: isValidFrameState(this.state)
        };
    }

    /**
     * Clones the flag; the clone points at the same SharedArrayBuffer
     */
    clone(): AtomicFlag {
        return new AtomicFlag(this.buffer.buffer as SharedArrayBuffer,
                             this.buffer.byteOffset + this.index * 4, 0);
    }
}