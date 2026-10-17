from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class MorpheusTrackerConan(ConanFile):
    name = "morpheus_tracker"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("opencv/4.13.0")
        if self.settings.os != "Emscripten":
            self.requires("nlohmann_json/3.11.3")
            self.requires("gtest/1.14.0")

    def tool_requirements(self):
        if self.settings.os == "Emscripten":
            self.tool_requires("emsdk/3.1.50")

    def configure(self):
        # Keep only the OpenCV modules the tracker actually uses:
        # core, imgproc, imgcodecs, features2d, video, calib3d, xfeatures2d.
        # xfeatures2d is mandatory - it provides the BEBLID descriptor.
        opts = self.options["opencv"]
        is_wasm = self.settings.os == "Emscripten"
        opts.xfeatures2d = True

        # No need for video codecs: frames arrive from JS as a raw buffer.
        opts.with_ffmpeg = not is_wasm
        opts.with_libvpx = False
        opts.with_libx264 = False
        opts.with_libx265 = False
        opts.with_libaom = False
        opts.with_libdav1d = False
        opts.with_openh264 = False
        opts.with_libfdk_aac = False
        opts.with_libmp3lame = False
        opts.with_opus = False
        opts.with_vorbis = False
        opts.with_soxr = False

        # Image formats are unnecessary for the same reason.
        opts.with_webp = False
        opts.with_openexr = False
        opts.with_jpeg2000 = False
        opts.with_avif = False
        opts.with_imgcodec_hdr = False
        opts.with_imgcodec_pfm = False
        opts.with_imgcodec_pxm = False
        opts.with_imgcodec_sunraster = False

        opts.dnn = False
        opts.with_protobuf = False
        opts.with_flatbuffers = False
        opts.with_freetype = False
        opts.with_quirc = False
        opts.with_gdal = False
        opts.with_opencl = False
        opts.with_tiff = False
        opts.with_lzma = False

        if is_wasm:
            # `parallel` selects a backend (False / tbb / openmp), it is NOT a bool flag.
            # Leaving it False: threading comes from WITH_PTHREADS_PF, which OpenCV
            # enables by default and activates when HAVE_PTHREAD is set - supplied by
            # the -pthread in profiles/emscripten-base. We don't pull TBB into wasm.
            # The thread count is chosen at runtime via cv::setNumThreads().
            opts.parallel = False

    def layout(self):
        cmake_layout(self)
