#include "ORB.hpp"

#ifdef EMSCRIPTEN
#include <malloc.h>
#endif

#include <algorithm>
#include <cmath>
#include <random>
#include <string.h>

#include <debug/drawer.hpp>
#include <vector>

#include "common/homography_result.hpp"
#include "common/logger.hpp"
#include "common/timer.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/core.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/core/types_c.h"
#include "opencv2/features2d.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"

// Pyramid counter suppression:
// For each level L (0=coarsest .. maxDepth=finest), divide bounds into 2^L x 2^L cells.
// Each cell at level L has a limit of K^(maxDepth-L) points.
// A point is accepted only if ALL level counters are below their limits.
// Shuffle beforehand so QR-code (high response) doesn't monopolize slots.
static std::vector<cv::Point2f> pyramidCounterSample(const std::vector<cv::Point2f>& pts,
                                                      const cv::Rect& bounds,
                                                      int maxDepth, int leafMax, float k) {
    // Build counter arrays per level: level L has (2^L)^2 cells
    int numLevels = maxDepth + 1;
    std::vector<std::vector<int>> counters(numLevels);
    std::vector<int> limits(numLevels);
    for (int L = 0; L < numLevels; L++) {
        int cells = (1 << L) * (1 << L);
        counters[L].assign(cells, 0);
        limits[L] = (int)std::ceil(leafMax * std::pow(k, maxDepth - L));
    }

    // Shuffle so high-response clusters don't fill slots first
    std::vector<cv::Point2f> shuffled = pts;
    std::mt19937 rng(42);
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    std::vector<cv::Point2f> result;
    for (const auto& p : shuffled) {
        float nx = (p.x - bounds.x) / (float)bounds.width;
        float ny = (p.y - bounds.y) / (float)bounds.height;
        nx = std::max(0.f, std::min(0.9999f, nx));
        ny = std::max(0.f, std::min(0.9999f, ny));

        // Check all levels
        bool accepted = true;
        for (int L = 0; L < numLevels; L++) {
            int gridL = (1 << L);
            int col = (int)(nx * gridL);
            int row = (int)(ny * gridL);
            int idx = row * gridL + col;
            if (counters[L][idx] >= limits[L]) { accepted = false; break; }
        }

        if (accepted) {
            // Increment counters at all levels
            for (int L = 0; L < numLevels; L++) {
                int gridL = (1 << L);
                int col = (int)(nx * gridL);
                int row = (int)(ny * gridL);
                counters[L][row * gridL + col]++;
            }
            result.push_back(p);
        }
    }
    return result;
}

static std::vector<cv::Point2f> gridSample(const std::vector<cv::Point2f>& pts,
                                            const cv::Rect& bounds, int gridSize) {
    if (pts.empty()) return pts;
    int maxDepth = (int)std::round(std::log2(gridSize));
    return pyramidCounterSample(pts, bounds, maxDepth, 4, 4.0f);
}

void ORBTracker::initTemplate(uchar templateGrayData[], size_t templateCols,
                              size_t templateRows, bool isGrey) {
    padding = 0;
    recognized_ = false;
    hasPendingDetection_ = false;

    templateGray_ = parseMat(templateGrayData, templateCols, templateRows,
                             isGrey, padding, 255);

    IF_DEBUG(debugData.addImage("templateGray_", templateGray_));

    templateKeyPts_.clear();
    features_detector_->detect(templateGray_.clone(), templateKeyPts_,
                               cv::noArray());

    features_descriptor_->compute(templateGray_.clone(), templateKeyPts_,
                                  templateKeyDescriptors_);

    LOG_INFO("ORB", "initTemplate: templateKeyPts_=", templateKeyPts_.size(), " descriptors=", templateKeyDescriptors_.rows);

    IF_DEBUG({
        auto templateGrayClone = templateGray_.clone();
        for (const auto &pt : templateKeyPts_)
            cv::circle(templateGrayClone, pt.pt, 3, cv::Scalar(0, 255, 0), -1);
        debugData.addImage("detected keypoints on template", templateGrayClone);
    });

    for (const auto &kp : templateKeyPts_) {
        templatePts_.push_back(kp.pt);
    }

    Timer timer;
    timer.create_timestamp("buildOpticalFlowPyramid create");
    cv::buildOpticalFlowPyramid(
        templateGray_, templatePyr_,
        cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
        config_.opticalFlowMaxLevel, true);
    timer.create_timestamp("buildOpticalFlowPyramid END");

    corners_ = {cvPoint(0, 0),
                cvPoint(static_cast<int>(templateGray_.size[1]), 0),
                cvPoint(static_cast<int>(templateGray_.size[1]),
                        static_cast<int>(templateGray_.size[0])),
                cvPoint(0, static_cast<int>(templateGray_.size[0]))};

    // Detect Shi-Tomasi corners on the template once; shared by tracking and OBD correction
    if (config_.useOBDCorrection || config_.useShiTomasi) {
        std::vector<cv::Point2f> rawTmplPts;
        cv::goodFeaturesToTrack(templateGray_, rawTmplPts,
                                config_.shiTomasiMaxCorners,
                                config_.shiTomasiQuality,
                                config_.shiTomasiMinDist);
        cv::Rect tmplBounds(0, 0, templateGray_.cols, templateGray_.rows);
        shiTemplatePts_ = gridSample(rawTmplPts, tmplBounds, config_.obdCorrectionGridSize);
        LOG_INFO("ORB", "ShiTomasi on template: ", shiTemplatePts_.size(),
                 " points (raw: ", rawTmplPts.size(), ")");
    }

    if (config_.useESM) {
        esmTemplate_ = templateGray_.clone();
        esmPts_.clear();
        int step = config_.esmPointStep;
        for (int y = step; y < templateGray_.rows - step; y += step)
            for (int x = step; x < templateGray_.cols - step; x += step)
                esmPts_.push_back({(float)x, (float)y});
    }

    initialized_ = true;
}

void ORBTracker::processFrame(uchar imageData[], size_t cols, size_t rows,
                              bool isGrey) {
    if (!initialized_) {
        throw std::runtime_error(
            "Tracking algorithm not initialized with template");
    }

    if (!recognized_) {
        startTracking(imageData, cols, rows, isGrey);
        return;
    }

    Timer timer;

    currentImage = parseMat(imageData, cols, rows, isGrey, 0u);

    timer.create_timestamp("parseMat");

    cv::buildOpticalFlowPyramid(
        currentImage, currentPyr_,
        cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
        config_.opticalFlowMaxLevel, true);

    timer.create_timestamp("buildPyramid");

    if (!config_.useShiTomasi) {
        cv::perspectiveTransform(matchedPts_, trackFramePts, homography_);
    }
    // useShiTomasi: trackFramePts are fixed — set once in startTracking or after OBD correction

    timer.create_timestamp("perspectiveTransform");

    const std::vector<cv::Point2f> originPts = trackFramePts;
    std::vector<cv::Point2f> currentPts = trackFramePts;

    bool iterSuccess = false;
    std::vector<cv::Point2f> goodOriginPts, goodFinalPts;

    if (previousPyr_.empty() || currentPyr_.empty()) {
        recognized_ = false;
        incrementTrackingFailures();
        return;
    }

    for (int iter = 0; iter < config_.opticalFlowIterations; ++iter) {
        std::vector<float> err;
        std::vector<uchar> status;
        std::vector<cv::Point2f> nextPts;

        cv::calcOpticalFlowPyrLK(
            previousPyr_, currentPyr_, currentPts, nextPts, status,
            err, cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
            config_.opticalFlowMaxLevel);

        // Forward-backward check: track nextPts back and compare with currentPts
        std::vector<uchar> fbStatus;
        std::vector<cv::Point2f> backPts;
        if (config_.useFBCheck) {
            std::vector<float> fbErr;
            cv::calcOpticalFlowPyrLK(
                currentPyr_, previousPyr_, nextPts, backPts, fbStatus,
                fbErr, cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
                config_.opticalFlowMaxLevel);
        }

        // Filter by status, keeping each survivor paired with its origin point
        std::vector<cv::Point2f> newOriginPts, newFinalPts, newCurrentPts;
        const std::vector<cv::Point2f>& originRef = (iter == 0) ? originPts : goodOriginPts;
        for (size_t i = 0; i < currentPts.size(); ++i) {
            if (!status[i]) continue;
            if (config_.useFBCheck && fbStatus.size() > i && fbStatus[i]) {
                float dx = currentPts[i].x - backPts[i].x;
                float dy = currentPts[i].y - backPts[i].y;
                if (std::sqrt(dx*dx + dy*dy) > config_.fbCheckThreshold) continue;
            }
            newOriginPts.push_back(originRef[i]);
            newFinalPts.push_back(nextPts[i]);
            newCurrentPts.push_back(nextPts[i]);
        }

        if (newOriginPts.size() < 4) {
            LOG_WARN("ORB", "Too few optical flow matches on iter ", iter, ": ", newOriginPts.size());
            break;
        }

        // Check the quality of the intermediate transform (iter -> iter+1)
        cv::Mat transform = findHomography(newOriginPts, newFinalPts, cv::RANSAC, config_.ransacReprojThreshold);
        TrackingQuality quality = EvaluateHomographyQuality(transform, newOriginPts.size(),
            config_.minDeterminant, config_.maxDeterminant,
            config_.maxConditionNumber, config_.maxPerspectiveDistortion);
        if (!quality.isValid) {
            LOG_WARN("ORB", "Transform quality check failed on iter ", iter, ": ", quality.getFailureReasonString());
            break;
        }

        goodOriginPts = newOriginPts;
        goodFinalPts  = newFinalPts;
        currentPts    = newCurrentPts;
        iterSuccess   = true;

        LOG_DEBUG("ORB", "Optical flow iter ", iter, " OK, points: ", goodOriginPts.size());
    }

    timer.create_timestamp("calcOpticalFlowPyrLK iterations");

    const size_t minTracked = std::max<size_t>(
        4, (size_t)(originPts.size() * config_.minTrackedRatio));
    if (iterSuccess && goodOriginPts.size() < minTracked) {
        LOG_INFO("ORB", "Lost target: ", goodOriginPts.size(), " of ",
                 originPts.size(), " points survived");
        iterSuccess = false;
    }

    if (iterSuccess) {
        // Final homography: origin points to their positions after all iterations
        cv::Mat transform = findHomography(goodOriginPts, goodFinalPts, cv::RANSAC, config_.ransacReprojThreshold);
        cv::Mat candidateH = transform * homography_;
        TrackingQuality finalQuality = EvaluateHomographyQuality(candidateH, goodOriginPts.size(),
            config_.minDeterminant, config_.maxDeterminant,
            config_.maxConditionNumber, config_.maxPerspectiveDistortion);

        if (finalQuality.isValid) {
            homography_ = candidateH;
            frameCount_++;

            // Periodic OBD-style correction via Shi-Tomasi in warped frame
            lastMatchSrc_.clear();
            lastMatchDst_.clear();
            if (config_.useOBDCorrection && !shiTemplatePts_.empty() &&
                frameCount_ % config_.obdCorrectionInterval == 0) {
                ScopedTimer obdTimer("OBD correction");
                ManualTimer obdDetail("OBD");

                // Warp current frame into template space
                cv::Mat warpedFrame;
                cv::warpPerspective(currentImage, warpedFrame, homography_.inv(),
                                    templateGray_.size());
                obdDetail.checkpoint("warpPerspective");

                // CRITICAL: normalize brightness of warpedFrame to match templateGray_.
                // Without this, optical flow fails badly when lighting differs from template capture.
                cv::Mat warpedNorm;
                double tmplMean = cv::mean(templateGray_)[0];
                double warpedMean = cv::mean(warpedFrame)[0];
                if (warpedMean > 0)
                    warpedFrame.convertTo(warpedNorm, -1, tmplMean / warpedMean, 0);
                else
                    warpedNorm = warpedFrame;
                obdDetail.checkpoint("normalize");

                // Track shiTemplatePts_ from templatePyr_ (pre-built at initTemplate) into warpedFrame via optical flow
                std::vector<cv::Mat> warpedNormPyr;
                cv::buildOpticalFlowPyramid(warpedNorm, warpedNormPyr,
                    cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
                    config_.opticalFlowMaxLevel, true);
                obdDetail.checkpoint("buildPyr");

                std::vector<cv::Point2f> warpedPts = shiTemplatePts_;
                std::vector<uchar> ofStatus;
                std::vector<float> ofErr;
                cv::calcOpticalFlowPyrLK(
                    templatePyr_, warpedNormPyr, shiTemplatePts_, warpedPts,
                    ofStatus, ofErr,
                    cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
                    config_.opticalFlowMaxLevel,
                    cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01),
                    cv::OPTFLOW_USE_INITIAL_FLOW);
                obdDetail.checkpoint("calcOpticalFlow");

                std::vector<cv::Point2f> srcPts, dstPts;
                for (size_t i = 0; i < shiTemplatePts_.size(); i++) {
                    if (ofStatus[i])  {
                        srcPts.push_back(shiTemplatePts_[i]);
                        dstPts.push_back(warpedPts[i]);
                    }
                }

                IF_DEBUG({
                    cv::cvtColor(warpedNorm, debugImage_, cv::COLOR_GRAY2BGR);
                    for (size_t i = 0; i < shiTemplatePts_.size(); i++) {
                        if (i < warpedPts.size() && ofStatus[i]) {
                            cv::circle(debugImage_, warpedPts[i], 9, {0, 0, 255}, -1);
                            cv::line(debugImage_, shiTemplatePts_[i], warpedPts[i], {0, 200, 200}, 1);
                        }
                    }
                    for (size_t i = 0; i < shiTemplatePts_.size(); i++)
                        cv::circle(debugImage_, shiTemplatePts_[i], 3, {0, 255, 0}, -1);
                });

                if (srcPts.size() >= 4) {
                    // deltaH in template space: template pts → warped pts
                        cv::Mat inlierMask;
                        cv::Mat deltaH = cv::findHomography(srcPts, dstPts, cv::RANSAC, 10.0, inlierMask);

                        // Save inlier matches in frame space for debug visualization
                        lastMatchSrc_.clear();
                        lastMatchDst_.clear();
                        // Show all OF pairs as debug (inlier mask unreliable for small displacements)
                        cv::perspectiveTransform(srcPts, lastMatchSrc_, homography_);
                        cv::perspectiveTransform(dstPts, lastMatchDst_, homography_);
                        if (!deltaH.empty()) {
                            // correctedH = homography_ * deltaH maps template → frame
                            cv::Mat correctedH = homography_ * deltaH;
                            TrackingQuality cq = EvaluateHomographyQuality(correctedH, (int)srcPts.size(),
                                config_.minDeterminant, config_.maxDeterminant,
                                config_.maxConditionNumber, config_.maxPerspectiveDistortion);

                            if (cq.isValid) {
                                std::vector<cv::Point2f> oldCorners(4), newCorners(4);
                                cv::perspectiveTransform(
                                    std::vector<cv::Point2f>{corners_.begin(), corners_.end()},
                                    oldCorners, homography_);
                                cv::perspectiveTransform(
                                    std::vector<cv::Point2f>{corners_.begin(), corners_.end()},
                                    newCorners, correctedH);
                                double dist = 0;
                                for (int i = 0; i < 4; i++) {
                                    float dx = oldCorners[i].x - newCorners[i].x;
                                    float dy = oldCorners[i].y - newCorners[i].y;
                                    dist += std::sqrt(dx*dx + dy*dy);
                                }
                                dist /= 4.0;

                                if (dist <= config_.obdCorrectionThreshold) {
                                    // Smooth correction: blend corners, recompute homography
                                    const double alpha = config_.obdCorrectionAlpha;
                                    std::vector<cv::Point2f> blendedCorners(4);
                                    for (int i = 0; i < 4; i++) {
                                        blendedCorners[i].x = (1.0 - alpha) * oldCorners[i].x + alpha * newCorners[i].x;
                                        blendedCorners[i].y = (1.0 - alpha) * oldCorners[i].y + alpha * newCorners[i].y;
                                    }
                                    std::vector<cv::Point2f> srcCorners(corners_.begin(), corners_.end());
                                    homography_ = cv::getPerspectiveTransform(srcCorners, blendedCorners);
                                    if (config_.obdUpdateTrackingPts)
                                        cv::perspectiveTransform(shiTemplatePts_, trackFramePts, homography_);
                                    LOG_INFO("ORB", "OBD correction applied, dist=", dist, " pts=", srcPts.size());
                                } else {
                                    LOG_WARN("ORB", "OBD correction rejected, dist=", dist);
                                }
                            }
                        }
                }
            }

            // ESM refinement on top of the optical flow result
            if (config_.useESM && !esmTemplate_.empty() && !homography_.empty()) {
                cv::Mat H64;
                homography_.convertTo(H64, CV_64F);
                esmCorrect(esmTemplate_, esmPts_, currentImage, H64,
                           config_.esmMaxIter, config_.esmEps);
                int origType = homography_.type();
                H64.convertTo(homography_, origType);
            }

            currentImage.copyTo(previousImage);
            std::swap(previousPyr_, currentPyr_);
            if (config_.useShiTomasi && !shiTemplatePts_.empty()) {
                cv::perspectiveTransform(shiTemplatePts_, trackFramePts, homography_);
            } else {
                trackFramePts = goodFinalPts;
            }


            // Idea: Shi-Tomasi re-anchor — snap trackFramePts to freshly detected corners
            // on currentImage each frame to reduce drift. Disabled: causes instability.

            resetTrackingFailures();

            if (config_.useKalman) {
                // Derive corners from the homography and smooth them with Kalman
                std::vector<cv::Point2f> warped(4);
                cv::perspectiveTransform(
                    std::vector<cv::Point2f>{corners_.begin(), corners_.end()},
                    warped, homography_);
                if (!kfInitialized_) {
                    initKalman(warped);
                } else {
                    updateKalman(warped);
                }
            }

            LOG_DEBUG("ORB", "Optical flow tracking successful, points: ", goodOriginPts.size());
        } else {
            LOG_WARN("ORB", "Final homography invalid: ", finalQuality.getFailureReasonString());
            recognized_ = false;
            incrementTrackingFailures();
        }
    } else {
        recognized_ = false;
        kfInitialized_ = false;
        incrementTrackingFailures();
    }

    // Reset tracking if too many consecutive failures
    if (shouldResetTracking() && !recognized_) {
        LOG_WARN("ORB", "Too many consecutive failures (", consecutiveFailures_, "), resetting to detection mode");
    }

    if (frameCount_ % 30 == 1) {
        PerformanceLogger::logReport();
    }
}

HomographyResult ORBTracker::getHomography() {
    HomographyResult homographyResult{};
    homographyResult.success = recognized_;

    if (!recognized_) {
        return homographyResult;
    }

    if (config_.useKalman && kfInitialized_) {
        // Rebuild the homography from the Kalman-smoothed corners
        std::vector<cv::Point2f> srcPts{corners_.begin(), corners_.end()};
        std::vector<cv::Point2f> dstPts = {
            {(float)kfCorners_[0], (float)kfCorners_[1]},
            {(float)kfCorners_[2], (float)kfCorners_[3]},
            {(float)kfCorners_[4], (float)kfCorners_[5]},
            {(float)kfCorners_[6], (float)kfCorners_[7]},
        };
        cv::Mat H = cv::getPerspectiveTransform(srcPts, dstPts);
        FillHomographyResult(homographyResult, recognized_, H, dstPts, padding);
        return homographyResult;
    }

    std::vector<cv::Point2f> warped(4);
    perspectiveTransform(this->corners_, warped, homography_);

    FillHomographyResult(homographyResult, recognized_, homography_, warped,
                         padding);

    IF_DEBUG(debugData.addImage("result", DrawResult(previousImage, warped, false)));

    return homographyResult;
}

void ORBTracker::startTracking(uchar imageData[], size_t cols, size_t rows,
                               bool isGrey) {
    Timer stTimer;
    currentImage = parseMat(imageData, cols, rows, isGrey, 0u);
    stTimer.create_timestamp("search::parseMat");

    IF_DEBUG(debugData.addImage("startTracking currentImage", currentImage));

    cv::Mat frameDescriptors;
    std::vector<cv::KeyPoint> frameKeyPts;
    features_detector_->detect(currentImage, frameKeyPts, cv::noArray());
    stTimer.create_timestamp("search::detect");
    features_descriptor_->compute(currentImage, frameKeyPts, frameDescriptors);
    stTimer.create_timestamp("search::compute");

    IF_DEBUG({
        auto currentImageClone = currentImage.clone();
        for (const auto &pt : frameKeyPts)
            cv::circle(currentImageClone, pt.pt, 3, cv::Scalar(0, 255, 0), -1);
        debugData.addImage("detected keypoints on first frame", currentImageClone);
    });

    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher_->knnMatch(frameDescriptors, templateKeyDescriptors_, knnMatches,
                       2);
    stTimer.create_timestamp("search::knnMatch");

    framePts.clear();
    std::vector<cv::Point2f> refPts;

    matchedKeyPts_.clear();
    matchedPts_.clear();
    matchedDescriptors_.release();

    LOG_DEBUG("ORB", "knnMatches.size(): ", knnMatches.size());

    for (auto &knnMatch : knnMatches) {
        if (knnMatch.size() < 2) continue;
        if (knnMatch[0].distance < GOOD_MATCH_RATIO * knnMatch[1].distance) {
            framePts.push_back(frameKeyPts[knnMatch[0].queryIdx].pt);
            refPts.push_back(templateKeyPts_[knnMatch[0].trainIdx].pt);

            matchedKeyPts_.push_back(templateKeyPts_[knnMatch[0].trainIdx]);
            matchedPts_.push_back(templateKeyPts_[knnMatch[0].trainIdx].pt);
            matchedDescriptors_.push_back(
                templateKeyDescriptors_.row(knnMatch[0].trainIdx));
        }
    }

    LOG_DEBUG("ORB", "framePts.size(): ", framePts.size());

    if ((int)framePts.size() >= config_.minMatchesForRecognition) {
        homography_ = findHomography(refPts, framePts, cv::USAC_MAGSAC);

        // Evaluate homography quality
        TrackingQuality quality = EvaluateHomographyQuality(homography_, framePts.size(),
            config_.minDeterminant, config_.maxDeterminant,
            config_.maxConditionNumber, config_.maxPerspectiveDistortion);

        if (quality.isValid) {
            std::vector<cv::Point2f> warpedVec(4);
            cv::perspectiveTransform(std::vector<cv::Point2f>{corners_.begin(), corners_.end()},
                                     warpedVec, homography_);
            std::array<cv::Point2f, 4> currentCorners;
            for (int ci = 0; ci < 4; ci++) currentCorners[ci] = warpedVec[ci];

            if (confirmDetection(currentCorners, framePts.size())) {
                if (config_.useShiTomasi && !shiTemplatePts_.empty()) {
                    cv::perspectiveTransform(shiTemplatePts_, trackFramePts, homography_);
                }
            }
        } else {
            hasPendingDetection_ = false;
            LOG_WARN("ORB", "Homography validation failed: ", quality.getFailureReasonString());
            incrementTrackingFailures();
        }
    } else {
        LOG_DEBUG("ORB", "Insufficient matches for recognition: ", framePts.size());
        incrementTrackingFailures();
    }

    if (!config_.useShiTomasi) {
        trackFrameKeyPts = frameKeyPts;
        trackFramePts = framePts;
    }
}

bool ORBTracker::confirmDetection(const std::array<cv::Point2f, 4>& corners, size_t numMatches) {
    static constexpr float kThreshold = 30.f;

    if (hasPendingDetection_) {
        float maxDist = 0.f;
        for (int i = 0; i < 4; i++) {
            float dx = corners[i].x - pendingCorners_[i].x;
            float dy = corners[i].y - pendingCorners_[i].y;
            maxDist = std::max(maxDist, std::sqrt(dx*dx + dy*dy));
        }
        if (maxDist <= kThreshold) {
            currentImage.copyTo(previousImage);
            cv::buildOpticalFlowPyramid(
                currentImage, previousPyr_,
                cv::Size(config_.opticalFlowWinSize, config_.opticalFlowWinSize),
                config_.opticalFlowMaxLevel, true);
            recognized_ = true;
            hasPendingDetection_ = false;
            resetTrackingFailures();
            LOG_INFO("ORB", "Target confirmed, matches=", numMatches, " maxCornerDist=", maxDist);
            return true;
        } else {
            LOG_INFO("ORB", "Detection unstable, maxCornerDist=", maxDist);
        }
    }

    pendingCorners_ = corners;
    pendingHomography_ = homography_.clone();
    hasPendingDetection_ = true;
    LOG_INFO("ORB", "Pending detection, matches=", numMatches);
    return false;
}

void ORBTracker::initKalman(const std::vector<cv::Point2f>& corners) {
    kf_.init(16, 8, 0, CV_32F);

    // Transition: x_k = x_{k-1} + v_{k-1}
    cv::setIdentity(kf_.transitionMatrix);
    for (int i = 0; i < 8; i++)
        kf_.transitionMatrix.at<float>(i, i + 8) = 1.0f;

    // Measurement: only the coordinates (first 8) are observed
    kf_.measurementMatrix = cv::Mat::zeros(8, 16, CV_32F);
    for (int i = 0; i < 8; i++)
        kf_.measurementMatrix.at<float>(i, i) = 1.0f;

    cv::setIdentity(kf_.processNoiseCov, cv::Scalar(config_.kalmanProcessNoise));
    for (int i = 8; i < 16; i++)
        kf_.processNoiseCov.at<float>(i, i) = config_.kalmanProcessNoise * 0.1f;

    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar(config_.kalmanMeasNoise));
    cv::setIdentity(kf_.errorCovPre,  cv::Scalar(100.f));
    cv::setIdentity(kf_.errorCovPost, cv::Scalar(100.f));

    for (int i = 0; i < 4; i++) {
        kf_.statePre.at<float>(i*2)     = corners[i].x;
        kf_.statePre.at<float>(i*2 + 1) = corners[i].y;
        kf_.statePost.at<float>(i*2)    = corners[i].x;
        kf_.statePost.at<float>(i*2 + 1) = corners[i].y;
        kfCorners_[i*2]     = corners[i].x;
        kfCorners_[i*2 + 1] = corners[i].y;
    }

    kfInitialized_ = true;
}

void ORBTracker::updateKalman(const std::vector<cv::Point2f>& corners) {
    kf_.predict();

    cv::Mat meas(8, 1, CV_32F);
    for (int i = 0; i < 4; i++) {
        meas.at<float>(i*2)     = corners[i].x;
        meas.at<float>(i*2 + 1) = corners[i].y;
    }
    kf_.correct(meas);

    const cv::Mat& state = kf_.statePost;
    for (int i = 0; i < 8; i++)
        kfCorners_[i] = state.at<float>(i);
}