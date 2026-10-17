export class GrayscaleRenderer {
    private gl: WebGL2RenderingContext;
    private prog!: WebGLProgram;
    private vao!: WebGLVertexArrayObject;
    private uTexLoc!: WebGLUniformLocation;

    private videoTex!: WebGLTexture; // texture backing the <video>
    private fbo!: WebGLFramebuffer;
    private fboTex!: WebGLTexture;

    private canvas: HTMLCanvasElement;
    private readonly width: number;
    private readonly height: number;
    private readonly videWidth: number;
    private readonly videoHeight: number;

    constructor(canvas: HTMLCanvasElement, width: number, height: number, videoWidth: number, videoHeight: number) {
        this.canvas = canvas;
        this.width = width;
        this.height = height;
        this.videWidth = videoWidth;
        this.videoHeight = videoHeight;
        const gl = canvas.getContext('webgl2', {
            alpha: false,
            premultipliedAlpha: false,
            preserveDrawingBuffer: false,
            powerPreference: 'high-performance',
        });
        if (!gl) throw new Error('WebGL2 not available');
        this.gl = gl;

        this.initStaticState();
        this.initResources();
    }

    private initStaticState() {
        const gl = this.gl;

        const vs = `#version 300 es
    const vec2 POS[4] = vec2[4](vec2(-1.,-1.), vec2(1.,-1.), vec2(-1.,1.), vec2(1.,1.));
    const vec2 UV [4] = vec2[4](vec2(0.,1.), vec2(1.,1.), vec2(0.,0.), vec2(1.,0.));
    out vec2 vUv;
    void main() {
      gl_Position = vec4(POS[gl_VertexID], 0., 1.);
      vUv = UV[gl_VertexID];
    }`;

        const fs = `#version 300 es
    precision mediump float;
    in vec2 vUv;
    uniform sampler2D uTex;
    out vec4 outColor;
    void main() {
      vec3 rgb = texture(uTex, vUv).rgb;
      float y = dot(rgb, vec3(0.2126, 0.7152, 0.0722)); // BT.709 luma
      outColor = vec4(y, y, y, 1.0);
    }`;

        const compile = (type: number, src: string) => {
            const s = gl.createShader(type)!;
            gl.shaderSource(s, src);
            gl.compileShader(s);
            if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
                throw new Error(gl.getShaderInfoLog(s) || "shader compile error");
            }
            return s;
        };

        const prog = gl.createProgram()!;
        gl.attachShader(prog, compile(gl.VERTEX_SHADER, vs));
        gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fs));
        gl.linkProgram(prog);
        if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
            throw new Error(gl.getProgramInfoLog(prog) || "link error");
        }
        this.prog = prog;

        this.vao = gl.createVertexArray()!;
        gl.bindVertexArray(this.vao);
        gl.useProgram(this.prog);
        this.uTexLoc = gl.getUniformLocation(this.prog, "uTex")!;
        gl.uniform1i(this.uTexLoc, 0);

        // sticky states
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
        gl.pixelStorei(gl.PACK_ALIGNMENT,   1);

        this.canvas.width = this.width;
        this.canvas.height = this.height;
        gl.viewport(0, 0, this.width, this.height);
    }

    private initResources() {
        const gl = this.gl;

        // video texture
        this.videoTex = gl.createTexture()!;
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, this.videoTex);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, this.videWidth, this.videoHeight, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

        // render target texture and FBO
        this.fboTex = gl.createTexture()!;
        gl.bindTexture(gl.TEXTURE_2D, this.fboTex);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.R8, this.width, this.height, 0, gl.RED, gl.UNSIGNED_BYTE, null);

        this.fbo = gl.createFramebuffer()!;
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.fboTex, 0);
        if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) {
            throw new Error("FBO incomplete");
        }
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    }

    /** Rendering always targets an external CPU buffer, e.g. a Uint8Array over the WASM heap. */
    public render(video: HTMLVideoElement, outCPU: Uint8Array) {
        this.gl.useProgram(this.prog);
        this.gl.bindVertexArray(this.vao);

        // upload the current video frame into the texture
        this.gl.activeTexture(this.gl.TEXTURE0);
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.videoTex);
        this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, this.gl.RGBA, this.gl.UNSIGNED_BYTE, video);

        // draw into the FBO
        this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, this.fbo);
        this.gl.viewport(0, 0, this.width, this.height);
        this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);

        // read back into the external CPU buffer
        this.gl.readPixels(0, 0, this.width, this.height, this.gl.RED, this.gl.UNSIGNED_BYTE, outCPU);

        this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    }

    public dispose() {
        const gl = this.gl;
        gl.deleteTexture(this.videoTex);
        gl.deleteTexture(this.fboTex);
        gl.deleteFramebuffer(this.fbo);
        gl.deleteProgram(this.prog);
        gl.deleteVertexArray(this.vao);
    }
}
