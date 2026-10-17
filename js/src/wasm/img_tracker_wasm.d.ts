export {};

declare module './img_tracker_wasm.js' {
    const factory: () => Promise<EmscriptenModule>;
    export default factory;
}