#pragma once

#include <vector>
#include <string>

#include "opencv2/core/mat.hpp"

enum class TrackingFailureReason {
    NONE = 0,
    DETERMINANT_TOO_SMALL,
    DETERMINANT_TOO_LARGE,
    CONDITION_NUMBER_TOO_HIGH,
    SKEW_TOO_HIGH,
    PERSPECTIVE_DISTORTION,
    TOO_FEW_POINTS,
    EMPTY_HOMOGRAPHY
};

struct TrackingQuality {
    bool isValid = false;
    TrackingFailureReason failureReason = TrackingFailureReason::NONE;
    double determinant = 0.0;
    double conditionNumber = 0.0;
    double skewAngle = 0.0;
    int numPoints = 0;

    std::string getFailureReasonString() const;
};

bool IsHomographyValid(cv::Mat H);

TrackingQuality EvaluateHomographyQuality(const cv::Mat& H, int numPoints = 0);

// Overload with explicit thresholds, for differing template/frame scales
TrackingQuality EvaluateHomographyQuality(const cv::Mat& H, int numPoints,
                                          double minDet, double maxDet,
                                          double maxCondition, double maxPerspective);

std::string getMatType(const cv::Mat& mat);

cv::Mat parseMat(uchar imageData[], size_t cols, size_t rows, bool isGrey, size_t padding, uchar borderValue = 0);

cv::Mat computeHomographyUV(const cv::Mat &H_pix, size_t frameWidth,
                            size_t frameHeight, size_t dstWidth,
                            size_t dstHeight);