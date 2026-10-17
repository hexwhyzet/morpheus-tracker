#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "debug.hpp"
#include "homography_result.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/core/types.hpp"

struct DebugPoints {
    std::vector<cv::Point2f> trackingPts;    // points used for optical flow (in current frame)
    std::vector<cv::Point2f> correctionPts;  // template pts projected into frame
    std::vector<cv::Point2f> matchSrc;       // nearest-neighbor match: template side (in frame space)
    std::vector<cv::Point2f> matchDst;       // nearest-neighbor match: warped frame side (in frame space)
    cv::Mat debugImage;                      // optional debug image (e.g. warped frame with OF points)
};

class TrackerBase {
   public:
    TrackerBase(bool debug) : _debug(debug) {};

    virtual void initTemplate(uchar templateData[], size_t templateWidth,
                              size_t templateHeight, bool isGrey) = 0;

    virtual void processFrame(uchar imageData[], size_t width, size_t height,
                              bool isGrey) = 0;

    virtual HomographyResult getHomography() = 0;

    virtual DebugPoints getDebugPoints() const { return {}; }

    virtual ~TrackerBase() = default;

    std::string collectDebugData() {
        std::string debugDataJson = debugData.buildJson();
        debugData.clear();
        return debugDataJson;
    }

   protected:
    bool _debug = false;
    DebugData debugData;

    // Tracking quality monitoring
    int consecutiveFailures_ = 0;
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;

    void resetTrackingFailures() {
        consecutiveFailures_ = 0;
    }

    void incrementTrackingFailures() {
        consecutiveFailures_++;
    }

    bool shouldResetTracking() const {
        return consecutiveFailures_ >= MAX_CONSECUTIVE_FAILURES;
    }
};