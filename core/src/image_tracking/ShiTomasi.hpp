#pragma once

#include <cmath>
#include <vector>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>

#include "ORB_config.hpp"
#include "common/tracker_base.hpp"
#include "common/utils.hpp"
#include "common/homography_result.hpp"

// Tracks by detecting Shi-Tomasi corners on both previous and current frame,
// matching them via nearest-neighbor, and computing homography with RANSAC.
// No optical flow — each frame is independent, so drift cannot accumulate.
class ShiTomasiTracker : public TrackerBase {
public:
    explicit ShiTomasiTracker(bool debug) : TrackerBase(debug) {}
    ShiTomasiTracker(bool debug, const ORBConfig& config)
        : TrackerBase(debug), config_(config) {}

    void initTemplate(uchar templateData[], size_t templateCols,
                      size_t templateRows, bool isGrey) override;

    void processFrame(uchar imageData[], size_t cols, size_t rows,
                      bool isGrey) override;

    HomographyResult getHomography() override;

private:
    void startTracking(uchar imageData[], size_t cols, size_t rows, bool isGrey);

    ORBConfig config_;

    cv::Mat templateGray_;
    cv::Ptr<cv::ORB> detector_;
    cv::Ptr<cv::ORB> descriptor_;
    cv::Ptr<cv::BFMatcher> matcher_;

    std::vector<cv::KeyPoint> templateKeyPts_;
    cv::Mat templateKeyDescriptors_;
    std::vector<cv::Point2f> templatePts_;   // matched template points
    std::vector<cv::Point2f> corners_;       // template corner coords

    cv::Mat currentImage_;
    cv::Mat homography_;

    bool initialized_ = false;
    bool recognized_  = false;
    size_t padding_   = 0;

    float GOOD_MATCH_RATIO = 0.8f;
};
