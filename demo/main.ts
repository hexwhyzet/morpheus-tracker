import {TrackingLoop} from '@morpheus/tracker'

/**
 * The target the demo loads on startup; any file can be swapped in from the UI.
 * Jan Davidsz. de Heem, "Still Life with Flowers in a Glass Vase",
 * Rijksmuseum SK-C-214, public domain —
 * see public/ATTRIBUTION.md.
 */
const DEFAULT_TARGET = '/default-target.jpg'

const canvas = document.getElementById('view') as HTMLCanvasElement
const targetImg = document.getElementById('target') as HTMLImageElement
const fileInput = document.getElementById('file') as HTMLInputElement
const threadSel = document.getElementById('threadSel') as HTMLSelectElement
const fpsSel = document.getElementById('fpsSel') as HTMLSelectElement
const errorBox = document.getElementById('error') as HTMLDivElement

const el = {
    status: document.getElementById('status')!,
    fps: document.getElementById('fps')!,
    threads: document.getElementById('threads')!,
}

function fail(html: string): never {
    errorBox.innerHTML = html
    errorBox.style.display = 'grid'
    throw new Error(errorBox.textContent ?? 'failed')
}

/**
 * Template pixels as RGBA — exactly what setTemplate expects (it calls
 * initTemplate(..., isGrey = false), and WASM does the grayscale conversion).
 */
function templatePixels(img: HTMLImageElement): Uint8Array {
    const w = img.naturalWidth
    const h = img.naturalHeight
    const c = document.createElement('canvas')
    c.width = w
    c.height = h
    const ctx = c.getContext('2d', {willReadFrequently: true})!
    ctx.drawImage(img, 0, 0)
    return new Uint8Array(ctx.getImageData(0, 0, w, h).data.buffer)
}

function startSpinner(label: string): () => void {
    let dots = 0
    const tick = () => {
        el.status.textContent = label + '.'.repeat(dots % 4)
        dots++
    }
    tick()
    const id = setInterval(tick, 400)
    return () => clearInterval(id)
}

async function main(): Promise<void> {
    if (typeof SharedArrayBuffer === 'undefined') {
        fail(
            'SharedArrayBuffer is unavailable — this page is not cross-origin isolated.<br><br>' +
            'Serve it with <code>Cross-Origin-Opener-Policy: same-origin</code> and ' +
            '<code>Cross-Origin-Embedder-Policy: require-corp</code>.',
        )
    }

    // The camera frame rate is fixed when the stream opens and cannot be
    // changed on the fly, so it goes through the URL plus a reload: ?fps=60
    const frameRate = Number(new URLSearchParams(location.search).get('fps')) || 60
    fpsSel.value = String(frameRate)
    fpsSel.addEventListener('change', () => {
        location.search = `?fps=${fpsSel.value}`
    })

    const loop = new TrackingLoop({
        canvas,
        drawDetectionQuad: true,
        debug: true,
        threads: 2,
        frameRate,
        onPose: (pose) => {
            el.status.textContent = pose.valid ? 'target locked' : 'no target'
            el.status.className = pose.valid ? '' : 'lost'
        },
        onStats: (stats) => {
            el.fps.textContent = `${stats.fps} capture / ${stats.displayFps} display`
        },
    })

    let stopSpinner = startSpinner('starting')
    try {
        await loop.init()
    } catch (e) {
        stopSpinner()
        fail(`Camera unavailable.<br><br>${e instanceof Error ? e.message : String(e)}`)
    }

    el.threads.textContent = String(await loop.getOpenCVThreads())
    stopSpinner()
    stopSpinner = startSpinner('loading target')

    let started = false

    async function useTarget(src: HTMLImageElement): Promise<void> {
        await loop.setTemplate(templatePixels(src), src.naturalWidth, src.naturalHeight)
        if (!started) {
            started = true
            await loop.start()
        }
        el.status.textContent = 'no target'
    }

    document.getElementById('pick')!.addEventListener('click', () => fileInput.click())
    fileInput.addEventListener('change', () => {
        const file = fileInput.files?.[0]
        if (!file) return
        targetImg.src = URL.createObjectURL(file)
        targetImg.addEventListener('load', () => void useTarget(targetImg), {once: true})
    })

    threadSel.addEventListener('change', async () => {
        await loop.setOpenCVThreads(Number(threadSel.value))
        el.threads.textContent = String(await loop.getOpenCVThreads())
    })

    // Default target, so the demo works right away without picking a file.
    targetImg.addEventListener('load', () => void useTarget(targetImg), {once: true})
    targetImg.addEventListener(
        'error',
        () => {
            el.status.textContent = 'pick a target'
        },
        {once: true},
    )
    targetImg.src = DEFAULT_TARGET
}

void main()
