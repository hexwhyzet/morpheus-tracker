#include <iostream>

#ifdef EMSCRIPTEN

#include <emscripten/emscripten.h>
#include <malloc.h>

#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <memory>

#include <opencv2/core.hpp>

#include "image_tracking/ORB.hpp"
#include "image_tracking/ORB_config.hpp"
#include "image_tracking/common/homography_result.hpp"
#include "image_tracking/common/logger.hpp"

// Default OpenCV thread count. The ceiling is PTHREAD_POOL_SIZE (8) from
// wasm/CMakeLists.txt; past it pthread_create finds no free worker. We stay well
// below: at our frame sizes the synchronization overhead in parallel_for_ grows
// faster than the speedup, and with ALLOW_MEMORY_GROWTH=0 every thread also
// carves its stack out of the fixed 128MB. Adjustable from JS via setOpenCVThreads().
static constexpr int kDefaultCVThreads = 2;

std::unique_ptr<HomographyResult> homographyResult{};

static ORBConfig makeConfig() {
    ORBConfig c;
    c.nFeatures               = 2000;
    c.nDescriptorFeatures     = 3000;
    c.descriptorType          = "BEBLID";
    c.opticalFlowWinSize      = 15;
    c.opticalFlowMaxLevel     = 3;
    c.useShiTomasi            = true;
    c.shiTomasiMaxCorners     = 500;
    c.shiTomasiQuality        = 0.01;
    c.shiTomasiMinDist        = 5.0;
    c.useOBDCorrection        = true;
    c.obdCorrectionGridSize   = 8;
    c.obdCorrectionInterval   = 20;
    c.obdCorrectionThreshold  = 50.0;
    c.obdCorrectionAlpha      = 0.7;
    c.obdUpdateTrackingPts    = true;
    c.useFBCheck              = true;
    c.fbCheckThreshold        = 1.5f;
    c.ransacReprojThreshold   = 3.0;
    c.goodMatchRatio          = 0.75f;
    c.minMatchesForRecognition = 15;
    c.minDeterminant          = 0.001;
    c.maxDeterminant          = 1000.0;
    c.maxConditionNumber      = 1000.0;
    c.maxPerspectiveDistortion = 0.1;
    return c;
}

std::unique_ptr<ORBTracker> tracker;

static std::vector<uchar> lastFrameBuffer;
static LastFrameData lastLastFrameData{};

extern "C" {

EMSCRIPTEN_KEEPALIVE
void init(bool debug) {
    LOG_INFO("WASM", "Entered init");
    // Set once at startup rather than per frame.
    cv::setNumThreads(kDefaultCVThreads);
    LOG_INFO("WASM", "OpenCV threads=", cv::getNumThreads());
    tracker = std::make_unique<ORBTracker>(debug, makeConfig());
    homographyResult = std::make_unique<HomographyResult>();
    struct mallinfo mi = mallinfo();
    LOG_INFO("WASM", "used after init=", mi.uordblks / 1024, "KB free=", mi.fordblks / 1024, "KB");
}

// Exposed to JS so 1/2/4 threads can be compared against a single build.
EMSCRIPTEN_KEEPALIVE
void setOpenCVThreads(int n) {
    cv::setNumThreads(n);
    LOG_INFO("WASM", "OpenCV threads set to ", cv::getNumThreads());
}

EMSCRIPTEN_KEEPALIVE
int getOpenCVThreads() {
    return cv::getNumThreads();
}

EMSCRIPTEN_KEEPALIVE
void initTemplate(uchar templateData[], size_t width, size_t height,
                  bool isGrey) {
    LOG_INFO("WASM", "initTemplate");
    LOG_DEBUG("WASM", "width=", width, " height=", height);
    homographyResult = std::make_unique<HomographyResult>();
    tracker->initTemplate(templateData, width, height, isGrey);
    struct mallinfo mi2 = mallinfo();
    LOG_INFO("WASM", "used after initTemplate=", mi2.uordblks / 1024, "KB free=", mi2.fordblks / 1024, "KB");
}

static void saveLastFrame(uchar imageData[], size_t width, size_t height) {
    lastFrameBuffer.assign(imageData, imageData + width * height);
    lastLastFrameData = {lastFrameBuffer.data(), width, height};
}

static uint32_t frameCounter_ = 0;

EMSCRIPTEN_KEEPALIVE
void processFrameInplaceResult(uchar imageData[], size_t width, size_t height,
                               HomographyResult *result) {
    if (++frameCounter_ % 300 == 0) {
        struct mallinfo mi3 = mallinfo();
        size_t totalMem = __builtin_wasm_memory_size(0) * 65536;
        LOG_INFO("WASM", "used=", mi3.uordblks / 1024, "KB free=", mi3.fordblks / 1024, "KB total=", totalMem / 1024 / 1024, "MB");
    }
    saveLastFrame(imageData, width, height);
    tracker->processFrame(imageData, width, height, true);
    *result = tracker->getHomography();
}

EMSCRIPTEN_KEEPALIVE
LastFrameData *getLastFrame() {
    return lastLastFrameData.data ? &lastLastFrameData : nullptr;
}

EMSCRIPTEN_KEEPALIVE
const char *collectDebugData() {
    static std::string jsonStr;
    jsonStr = tracker->collectDebugData();
    return jsonStr.c_str();
}
}
