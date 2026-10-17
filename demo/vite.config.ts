import {defineConfig} from 'vite'
import basicSsl from '@vitejs/plugin-basic-ssl'

/**
 * SharedArrayBuffer requires cross-origin isolation, and COEP: require-corp
 * requires CORP on every subresource — including emscripten's ?worker_file.
 * server.headers does not cover every dev-server response, so we set the
 * headers from middleware instead.
 */
const crossOriginIsolation = {
    name: 'cross-origin-isolation',
    configureServer(server: any) {
        server.middlewares.use((_req: any, res: any, next: any) => {
            res.setHeader('Cross-Origin-Opener-Policy', 'same-origin')
            res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp')
            res.setHeader('Cross-Origin-Resource-Policy', 'same-origin')
            next()
        })
    },
}

/**
 * Emscripten's pthread worker loads the module through a dynamic import() that
 * Vite cannot analyze statically — silence the warning.
 */
const patchEmscriptenWorker = {
    name: 'patch-emscripten-worker',
    enforce: 'pre' as const,
    transform(code: string, id: string) {
        if (id.includes('/wasm/img_tracker_wasm.js')) {
            return {
                code: code.replace(/,\s*workerOptions\);/g, ', /* @vite-ignore */ workerOptions);'),
                map: null,
            }
        }
        if (id.includes('/wasm/img_tracker_wasm.worker.js')) {
            return {
                code: code.replace(
                    /import\((e\.data\.urlOrBlob|"\.\/img_tracker_wasm\.js")\)/g,
                    'import(/* @vite-ignore */ $1)',
                ),
                map: null,
            }
        }
        return null
    },
}

export default defineConfig({
    plugins: [patchEmscriptenWorker, crossOriginIsolation, basicSsl()],
    server: {
        port: 5180,
        host: '0.0.0.0',
        headers: {
            'Cross-Origin-Opener-Policy': 'same-origin',
            'Cross-Origin-Embedder-Policy': 'require-corp',
        },
    },
    optimizeDeps: {
        // Excluded, otherwise Vite tries to pre-bundle the emscripten glue
        exclude: ['@morpheus/tracker'],
    },
    worker: {
        // The emscripten glue is an ES module that code-splits, which rules
        // out Vite's default iife worker format.
        format: 'es',
    },
    build: {
        target: 'es2022',
    },
})
