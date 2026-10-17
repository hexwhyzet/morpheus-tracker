#pragma once

#include <memory>
#include <opencv2/features2d.hpp>
#include <opencv2/video/tracking.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <vector>
#include <opencv2/calib3d.hpp>

#include "ORB_config.hpp"
#include "common/esm_correction.hpp"
#include "common/tracker_base.hpp"
#include "common/utils.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/core/types_c.h"

class ORBTracker : public TrackerBase {
   public:
    explicit ORBTracker(bool debug) : TrackerBase(debug) { initDetectors(); }
    ORBTracker(bool debug, const ORBConfig& config) : TrackerBase(debug), config_(config) { initDetectors(); }

    void initTemplate(uchar templateGrayData[], size_t templateCols,
                      size_t templateRows, bool isGrey) override;

    void processFrame(uchar imageGrayData[], size_t cols, size_t rows,
                      bool isGrey) override;

    HomographyResult getHomography() override;

    DebugPoints getDebugPoints() const override {
        DebugPoints dp;
        dp.trackingPts = trackFramePts;
        if (recognized_ && !homography_.empty() && !shiTemplatePts_.empty())
            cv::perspectiveTransform(shiTemplatePts_, dp.correctionPts, homography_);
        dp.matchSrc = lastMatchSrc_;
        dp.matchDst = lastMatchDst_;
        dp.debugImage = debugImage_;
        return dp;
    }

    // Reset the internal homography to an externally supplied one to correct drift
    void resetHomography(const HomographyResult& result, const cv::Mat& frame) {
        if (!result.success) return;
        homography_ = cv::Mat(3, 3, CV_64F);
        for (int i = 0; i < 9; i++)
            homography_.at<double>(i / 3, i % 3) = result.H[i];
        // Both images must match in size for the next optical flow pass
        currentImage  = frame.clone();
        previousImage = frame.clone();
        recognized_ = true;
        kfInitialized_ = false;
    }

   protected:
    virtual void startTracking(uchar imageGrayData[], size_t cols, size_t rows,
                               bool isGrey);

    cv::Ptr<cv::ORB> features_detector_;
    cv::Ptr<cv::Feature2D> features_descriptor_;
    cv::Ptr<cv::BFMatcher> matcher_;
    cv::Mat templateGray_;

    std::vector<cv::Mat> templatePyr_;
    std::vector<cv::Mat> previousPyr_;
    std::vector<cv::Mat> currentPyr_;
    static constexpr int OPTICAL_FLOW_WIN_SIZE = 15;
    static constexpr int OPTICAL_FLOW_MAX_LEVEL = 5;
    int opticalFlowFlags = 0;

    ORBConfig config_;

    std::vector<cv::KeyPoint> templateKeyPts_;
    std::vector<cv::Point2f> templatePts_;
    cv::Mat templateKeyDescriptors_;

    std::vector<cv::KeyPoint> matchedKeyPts_;
    std::vector<cv::Point2f> matchedPts_;
    cv::Mat matchedDescriptors_;

    std::vector<cv::Point2f> corners_;

    bool initialized_ = false;
    bool recognized_ = false;

    // Two-frame confirmation: corners from previous successful detection
    std::array<cv::Point2f, 4> pendingCorners_{};
    cv::Mat pendingHomography_;
    bool hasPendingDetection_ = false;

    size_t padding = 0;

    cv::Mat currentImage;
    cv::Mat previousImage;

    float GOOD_MATCH_RATIO = 0.8;

    std::vector<cv::Point2f> framePts;

    cv::Mat homography_;

    std::vector<cv::KeyPoint> trackFrameKeyPts;
    std::vector<cv::Point2f> trackFramePts;

    // Kalman filter (state=16: 8 corners + 8 velocities, measurement=8)
    cv::KalmanFilter kf_;
    bool kfInitialized_ = false;
    double kfCorners_[8] = {};

    // OBD-style periodic correction
    int frameCount_ = 0;
    std::vector<cv::Point2f> shiTemplatePts_;  // Shi-Tomasi detected once on template
    std::vector<cv::Point2f> lastMatchSrc_;    // last correction match: template side (frame space)
    std::vector<cv::Point2f> lastMatchDst_;    // last correction match: warped side (frame space)

    // ESM correction
    cv::Mat esmTemplate_;
    std::vector<cv::Point2f> esmPts_;

    cv::Mat debugImage_;  // debug visualization of OBD correction step

    void initKalman(const std::vector<cv::Point2f>& corners);
    void updateKalman(const std::vector<cv::Point2f>& corners);
    bool confirmDetection(const std::array<cv::Point2f, 4>& corners, size_t numMatches);

    void initDetectors() {
        features_detector_ = cv::ORB::create(
            config_.nFeatures, config_.orbScaleFactor, config_.orbLevels,
            31, 0, 2, cv::ORB::HARRIS_SCORE, 31, config_.orbFastThreshold);
        if (config_.descriptorType == "BEBLID") {
            features_descriptor_ = cv::xfeatures2d::BEBLID::create(1.0f, cv::xfeatures2d::BEBLID::SIZE_256_BITS);
        } else {
            features_descriptor_ = cv::ORB::create(config_.nDescriptorFeatures);
        }
        matcher_ = cv::BFMatcher::create(cv::NORM_HAMMING);
    }
};