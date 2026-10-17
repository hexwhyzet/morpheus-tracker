#include "ShiTomasi.hpp"

#include <cmath>
#include "common/logger.hpp"
#include "common/homography_result.hpp"

void ShiTomasiTracker::initTemplate(uchar templateData[], size_t templateCols,
                                    size_t templateRows, bool isGrey) {
    padding_ = 0;
    templateGray_ = parseMat(templateData, templateCols, templateRows, isGrey, padding_);

    detector_   = cv::ORB::create(config_.nFeatures);
    descriptor_ = cv::ORB::create(config_.nDescriptorFeatures);
    matcher_    = cv::BFMatcher::create(cv::NORM_HAMMING);

    detector_->detect(templateGray_, templateKeyPts_);
    descriptor_->compute(templateGray_, templateKeyPts_, templateKeyDescriptors_);

    for (const auto& kp : templateKeyPts_)
        templatePts_.push_back(kp.pt);

    corners_ = {
        {0.f, 0.f},
        {(float)templateGray_.cols, 0.f},
        {(float)templateGray_.cols, (float)templateGray_.rows},
        {0.f, (float)templateGray_.rows},
    };

    initialized_ = true;
    LOG_INFO("ShiTomasi", "Template initialized: ", templateCols, "x", templateRows,
             " keypoints: ", templateKeyPts_.size());
}

void ShiTomasiTracker::processFrame(uchar imageData[], size_t cols, size_t rows, bool isGrey) {
    if (!initialized_)
        throw std::runtime_error("ShiTomasiTracker not initialized");

    if (!recognized_) {
        startTracking(imageData, cols, rows, isGrey);
        return;
    }

    currentImage_ = parseMat(imageData, cols, rows, isGrey, 0u);

    // Project object corners into current frame to build mask
    std::vector<cv::Point2f> objCorners(4);
    cv::perspectiveTransform(corners_, objCorners, homography_);

    cv::Mat mask = cv::Mat::zeros(currentImage_.size(), CV_8UC1);
    std::vector<cv::Point> poly(objCorners.begin(), objCorners.end());
    cv::fillConvexPoly(mask, poly, cv::Scalar(255));

    // Detect Shi-Tomasi on current frame within object mask
    std::vector<cv::Point2f> currPts;
    cv::goodFeaturesToTrack(currentImage_, currPts,
                            config_.shiTomasiMaxCorners,
                            config_.shiTomasiQuality,
                            config_.shiTomasiMinDist,
                            mask);

    if (currPts.size() < 4) {
        // Fallback: detect without mask
        cv::goodFeaturesToTrack(currentImage_, currPts,
                                config_.shiTomasiMaxCorners,
                                config_.shiTomasiQuality,
                                config_.shiTomasiMinDist);
    }

    // Previous frame points: back-project via H^-1 to get template-space coords,
    // then project via H to get their expected position in previousImage.
    // Instead we detect on current frame and match to template-space via H^-1.

    // Detect Shi-Tomasi on template-warped region: use objCorners as "previous"
    // anchor. Back-project currPts to template space.
    if (currPts.size() < 4) {
        LOG_WARN("ShiTomasi", "Too few corners detected: ", currPts.size());
        recognized_ = false;
        incrementTrackingFailures();
        return;
    }

    // Back-project detected points to template space
    std::vector<cv::Point2f> currInTemplate;
    cv::perspectiveTransform(currPts, currInTemplate, homography_.inv());

    // Filter: keep only points that land inside template bounds
    std::vector<cv::Point2f> validTemplate, validFrame;
    for (size_t i = 0; i < currInTemplate.size(); i++) {
        const auto& tp = currInTemplate[i];
        if (tp.x >= 0 && tp.x < templateGray_.cols &&
            tp.y >= 0 && tp.y < templateGray_.rows) {
            validTemplate.push_back(tp);
            validFrame.push_back(currPts[i]);
        }
    }

    if (validTemplate.size() < 4) {
        LOG_WARN("ShiTomasi", "Too few valid back-projected points: ", validTemplate.size());
        recognized_ = false;
        incrementTrackingFailures();
        return;
    }

    // RANSAC homography: template -> frame
    cv::Mat candidateH = cv::findHomography(validTemplate, validFrame, cv::RANSAC);
    TrackingQuality quality = EvaluateHomographyQuality(candidateH, (int)validTemplate.size(),
        config_.minDeterminant, config_.maxDeterminant,
        config_.maxConditionNumber, config_.maxPerspectiveDistortion);

    if (quality.isValid) {
        homography_ = candidateH;
        resetTrackingFailures();
        LOG_DEBUG("ShiTomasi", "Tracking OK, points: ", validTemplate.size());
    } else {
        LOG_WARN("ShiTomasi", "Homography invalid: ", quality.getFailureReasonString());
        recognized_ = false;
        incrementTrackingFailures();
    }
}

HomographyResult ShiTomasiTracker::getHomography() {
    HomographyResult result{};
    result.success = recognized_;
    if (!recognized_) return result;

    std::vector<cv::Point2f> warped(4);
    cv::perspectiveTransform(corners_, warped, homography_);
    FillHomographyResult(result, recognized_, homography_, warped, padding_);
    return result;
}

void ShiTomasiTracker::startTracking(uchar imageData[], size_t cols, size_t rows, bool isGrey) {
    currentImage_ = parseMat(imageData, cols, rows, isGrey, 0u);

    std::vector<cv::KeyPoint> frameKeyPts;
    cv::Mat frameDescriptors;
    detector_->detect(currentImage_, frameKeyPts);
    descriptor_->compute(currentImage_, frameKeyPts, frameDescriptors);

    if (frameKeyPts.empty() || frameDescriptors.empty()) {
        incrementTrackingFailures();
        return;
    }

    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher_->knnMatch(frameDescriptors, templateKeyDescriptors_, knnMatches, 2);

    std::vector<cv::Point2f> framePts, refPts;
    for (auto& m : knnMatches) {
        if (m.size() < 2) continue;
        if (m[0].distance < GOOD_MATCH_RATIO * m[1].distance) {
            framePts.push_back(frameKeyPts[m[0].queryIdx].pt);
            refPts.push_back(templateKeyPts_[m[0].trainIdx].pt);
        }
    }

    if (framePts.size() < (size_t)config_.minMatchesForRecognition) {
        LOG_DEBUG("ShiTomasi", "Insufficient matches: ", framePts.size());
        incrementTrackingFailures();
        return;
    }

    cv::Mat H = cv::findHomography(refPts, framePts, cv::RANSAC);
    TrackingQuality quality = EvaluateHomographyQuality(H, (int)framePts.size(),
        config_.minDeterminant, config_.maxDeterminant,
        config_.maxConditionNumber, config_.maxPerspectiveDistortion);

    if (quality.isValid) {
        homography_ = H;
        recognized_ = true;
        resetTrackingFailures();
        LOG_INFO("ShiTomasi", "Target recognized with ", framePts.size(), " matches");
    } else {
        LOG_WARN("ShiTomasi", "Recognition homography invalid: ", quality.getFailureReasonString());
        incrementTrackingFailures();
    }
}
