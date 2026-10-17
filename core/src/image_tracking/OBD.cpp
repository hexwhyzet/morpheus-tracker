#include "OBD.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common/homography_result.hpp"
#include "common/logger.hpp"
#include "common/timer.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/video/tracking.hpp"

void OBDTracker::processFrame(uchar imageData[], size_t cols, size_t rows,
                              bool isGrey) {
    Timer timer;
    Timer timer2;
    for (int i = 0; i < config_.opticalFlowIterations; ++i) {
        if (!initialized_) {
            throw std::runtime_error(
                "Tracking algorithm not initialized with template");
        }

        if (!recognized_) {
            startTracking(imageData, cols, rows, isGrey);
            return;
        }

        timer.create_timestamp("started processFrame");

        currentImage = parseMat(imageData, cols, rows, isGrey, 0u);

        timer2.create_timestamp("parseMat");

        auto &sourceKeyPts = matchedKeyPts_;
        auto &sourcePts = matchedPts_;

        cv::Mat warpedCurrentImage;

        cv::warpPerspective(currentImage, warpedCurrentImage, homography_.inv(),
                            templateGray_.size());

        if (_debug) {
            debugData.addImage("template", templateGray_);
            debugData.addImage("warpedCurrentImage", warpedCurrentImage);
        }

        timer2.create_timestamp("warpPerspective");

        std::vector<cv::Point2f> warpedTrackedPts;
        std::vector<uchar> status;
        std::vector<float> err;


        std::vector<cv::Mat> warpedCurrentImagePyr;
        cv::buildOpticalFlowPyramid(warpedCurrentImage, warpedCurrentImagePyr, cv::Size(OPTICAL_FLOW_WIN_SIZE, OPTICAL_FLOW_WIN_SIZE), OPTICAL_FLOW_MAX_LEVEL, true);
        timer2.create_timestamp("buildOpticalFlowPyramid");

        LOG_DEBUG("OBD", "sourcePts.size(): ", sourcePts.size());

        cv::calcOpticalFlowPyrLK(templatePyr_, warpedCurrentImagePyr, sourcePts,
                                 warpedTrackedPts, status, err, cv::Size(OPTICAL_FLOW_WIN_SIZE, OPTICAL_FLOW_WIN_SIZE), OPTICAL_FLOW_MAX_LEVEL);

        LOG_DEBUG("OBD", "warpedTrackedPts.size(): ", warpedTrackedPts.size());

        timer2.create_timestamp("calcOpticalFlowPyrLK");

        std::vector<cv::Point2f> validTemplatePts;
        std::vector<cv::KeyPoint> validWarpedKeyPts;

        cv::Mat templateDescriptors;

        std::vector<cv::Point2f> temp;

        for (size_t i = 0; i < status.size(); ++i) {
            if (!status[i]) continue;

            validTemplatePts.push_back(sourcePts[i]);

            cv::KeyPoint kp = sourceKeyPts[i];
            kp.pt = warpedTrackedPts[i];
            validWarpedKeyPts.push_back(kp);

            temp.push_back(kp.pt);

            templateDescriptors.push_back(
                matchedDescriptors_.row(static_cast<int>(i)));
        }

        if (validWarpedKeyPts.size() < 4) {
            LOG_WARN("OBD", "TOO FEW warpedKeypoints: ", validWarpedKeyPts.size());
            recognized_ = false;
            incrementTrackingFailures();
            return;
        }

        for (int i = 0; i < validWarpedKeyPts.size(); ++i) {
            validWarpedKeyPts[i].class_id = i;
        }
        timer2.create_timestamp("features_descriptor_->compute PREPARE");

        cv::Mat reorderedWarpedDescriptors;
        std::vector<cv::KeyPoint> reorderedKeypoints = validWarpedKeyPts;
        features_descriptor_->compute(warpedCurrentImage, reorderedKeypoints,
                                      reorderedWarpedDescriptors);
        timer2.create_timestamp("features_descriptor_->compute");

        cv::Mat warpedDescriptors(validWarpedKeyPts.size(),
                                  reorderedWarpedDescriptors.cols,
                                  reorderedWarpedDescriptors.type());
        for (size_t i = 0; i < reorderedKeypoints.size(); ++i) {
            int id = reorderedKeypoints[i].class_id;
            if (id >= 0) {
                reorderedWarpedDescriptors.row(static_cast<int>(i))
                    .copyTo(warpedDescriptors.row(id));
            }
        }

        assert(warpedDescriptors.rows == (int)validWarpedKeyPts.size());

        if (warpedDescriptors.empty() || templateDescriptors.empty() ||
            warpedDescriptors.cols != templateDescriptors.cols) {
            LOG_WARN("OBD", "Descriptor size mismatch, skipping frame");
            recognized_ = false;
            incrementTrackingFailures();
            return;
        }

        std::vector<cv::Point2f> finalTemplatePts;
        std::vector<cv::Point2f> finalWarpedPts;
        std::vector<std::pair<double, size_t>> dists;
        for (size_t i = 0; i < validWarpedKeyPts.size(); ++i) {
            double dist = cv::norm(templateDescriptors.row(static_cast<int>(i)),
                                   warpedDescriptors.row(static_cast<int>(i)),
                                   cv::NORM_HAMMING);

            dists.emplace_back(dist, i);
        }

        std::sort(dists.begin(), dists.end());

//        for (int i = 0; dists[i].first < 50 && i < dists.size(); ++i) {
        for (int i = 0; i < 50 && i < dists.size(); ++i) {
            finalTemplatePts.push_back(validTemplatePts[dists[i].second]);
            finalWarpedPts.push_back(validWarpedKeyPts[dists[i].second].pt);
        }

        if (_debug) {
            LOG_DEBUG("OBD", "Points after sort: ", finalWarpedPts.size());
        }

        timer2.create_timestamp("cv::USAC_MAGSAC PREPARE");
        if (finalTemplatePts.size() >= 4) {
            cv::Mat transform = cv::findHomography(
                finalTemplatePts, finalWarpedPts, cv::USAC_MAGSAC);

            // Evaluate transform quality
            TrackingQuality quality = EvaluateHomographyQuality(transform, finalTemplatePts.size());

            if (quality.isValid) {
                homography_ = homography_ * transform;

                // Validate final homography
                TrackingQuality finalQuality = EvaluateHomographyQuality(homography_, finalTemplatePts.size());

                if (finalQuality.isValid) {
                    resetTrackingFailures();
                    LOG_INFO("OBD", "Tracking successful with ", finalTemplatePts.size(), " points");
                } else {
                    LOG_WARN("OBD", "Final homography invalid: ", finalQuality.getFailureReasonString());
                    recognized_ = false;
                    incrementTrackingFailures();
                }
            } else {
                LOG_WARN("OBD", "Transform quality check failed: ", quality.getFailureReasonString());
                recognized_ = false;
                incrementTrackingFailures();
            }
        } else {
            LOG_WARN("OBD", "Too few final points: ", finalTemplatePts.size());
            recognized_ = false;
            incrementTrackingFailures();
        }

        // Reset tracking if too many consecutive failures
        if (shouldResetTracking() && recognized_ == false) {
            LOG_WARN("OBD", "Too many consecutive failures (", consecutiveFailures_, "), will restart detection");
            break; // Exit retry loop
        }

        if (_debug) {
            previousImage = currentImage;
        }

        timer2.create_timestamp("cv::USAC_MAGSAC");
        timer.create_timestamp("ended processFrame");
    }
}
