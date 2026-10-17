#include "utils.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <cmath>
#include "logger.hpp"

const int N = 10;

std::string TrackingQuality::getFailureReasonString() const {
    switch (failureReason) {
        case TrackingFailureReason::NONE:
            return "No failure";
        case TrackingFailureReason::DETERMINANT_TOO_SMALL:
            return "Determinant too small (degenerate transformation)";
        case TrackingFailureReason::DETERMINANT_TOO_LARGE:
            return "Determinant too large (excessive scaling)";
        case TrackingFailureReason::CONDITION_NUMBER_TOO_HIGH:
            return "Condition number too high (ill-conditioned matrix)";
        case TrackingFailureReason::SKEW_TOO_HIGH:
            return "Skew angle too high (non-affine distortion)";
        case TrackingFailureReason::PERSPECTIVE_DISTORTION:
            return "Excessive perspective distortion";
        case TrackingFailureReason::TOO_FEW_POINTS:
            return "Too few matching points";
        case TrackingFailureReason::EMPTY_HOMOGRAPHY:
            return "Empty or null homography matrix";
        default:
            return "Unknown failure reason";
    }
}

TrackingQuality EvaluateHomographyQuality(const cv::Mat& H, int numPoints) {
    TrackingQuality quality;
    quality.numPoints = numPoints;

    // Check if homography is empty or invalid
    if (H.empty() || H.rows != 3 || H.cols != 3) {
        quality.failureReason = TrackingFailureReason::EMPTY_HOMOGRAPHY;
        LOG_WARN("TrackingQuality", "Homography matrix is empty or invalid size");
        return quality;
    }

    // Check if we have enough points
    if (numPoints > 0 && numPoints < 4) {
        quality.failureReason = TrackingFailureReason::TOO_FEW_POINTS;
        LOG_WARN("TrackingQuality", "Too few points for homography: ", numPoints);
        return quality;
    }

    // Normalize homography
    cv::Mat H_normalized = H.clone();
    if (H.type() == CV_64F) {
        double h33 = H.at<double>(2, 2);
        if (std::abs(h33) > 1e-10) {
            H_normalized /= h33;
        }
    } else if (H.type() == CV_32F) {
        float h33 = H.at<float>(2, 2);
        if (std::abs(h33) > 1e-10f) {
            H_normalized /= h33;
        }
    }

    // Extract 2x2 affine part for analysis
    cv::Mat A;
    if (H_normalized.type() == CV_64F) {
        A = (cv::Mat_<double>(2, 2) <<
             H_normalized.at<double>(0, 0), H_normalized.at<double>(0, 1),
             H_normalized.at<double>(1, 0), H_normalized.at<double>(1, 1));
    } else {
        cv::Mat H_64F;
        H_normalized.convertTo(H_64F, CV_64F);
        A = (cv::Mat_<double>(2, 2) <<
             H_64F.at<double>(0, 0), H_64F.at<double>(0, 1),
             H_64F.at<double>(1, 0), H_64F.at<double>(1, 1));
    }

    // 1. Check determinant (scale preservation)
    quality.determinant = cv::determinant(A);

    const double MIN_DET = 1.0 / N;
    const double MAX_DET = N;

    if (std::abs(quality.determinant) < MIN_DET) {
        quality.failureReason = TrackingFailureReason::DETERMINANT_TOO_SMALL;
        LOG_WARN("TrackingQuality", "Determinant too small: ", quality.determinant);
        return quality;
    }

    if (std::abs(quality.determinant) > MAX_DET) {
        quality.failureReason = TrackingFailureReason::DETERMINANT_TOO_LARGE;
        LOG_WARN("TrackingQuality", "Determinant too large: ", quality.determinant);
        return quality;
    }

    // 2. Check condition number (numerical stability)
    cv::Mat w, u, vt;
    cv::SVD::compute(A, w, u, vt);

    if (w.rows >= 2 && std::abs(w.at<double>(1, 0)) > 1e-10) {
        quality.conditionNumber = w.at<double>(0, 0) / w.at<double>(1, 0);
    } else {
        quality.conditionNumber = std::numeric_limits<double>::infinity();
    }

    const double MAX_CONDITION = 100.0;
    if (quality.conditionNumber > MAX_CONDITION) {
        quality.failureReason = TrackingFailureReason::CONDITION_NUMBER_TOO_HIGH;
        LOG_WARN("TrackingQuality", "Condition number too high: ", quality.conditionNumber);
        return quality;
    }

    // 3. Check perspective distortion
    double h31 = H_normalized.type() == CV_64F ?
                 H_normalized.at<double>(2, 0) : H_normalized.at<float>(2, 0);
    double h32 = H_normalized.type() == CV_64F ?
                 H_normalized.at<double>(2, 1) : H_normalized.at<float>(2, 1);

    const double MAX_PERSPECTIVE = 0.002;
    if (std::abs(h31) > MAX_PERSPECTIVE || std::abs(h32) > MAX_PERSPECTIVE) {
        quality.failureReason = TrackingFailureReason::PERSPECTIVE_DISTORTION;
        LOG_WARN("TrackingQuality", "Excessive perspective distortion: h31=", h31, " h32=", h32);
        return quality;
    }

    // 4. Check skew (should be close to orthogonal)
    double a = A.at<double>(0, 0);
    double b = A.at<double>(0, 1);
    double c = A.at<double>(1, 0);
    double d = A.at<double>(1, 1);

    double dot = a * c + b * d;
    double mag1 = std::sqrt(a * a + b * b);
    double mag2 = std::sqrt(c * c + d * d);

//    if (mag1 > 1e-10 && mag2 > 1e-10) {
//        double cosAngle = dot / (mag1 * mag2);
//        quality.skewAngle = std::acos(std::max(-1.0, std::min(1.0, cosAngle))) * 180.0 / M_PI;
//
//        const double MAX_SKEW = 30.0; // degrees
//        if (quality.skewAngle < (90.0 - MAX_SKEW) || quality.skewAngle > (90.0 + MAX_SKEW)) {
//            quality.failureReason = TrackingFailureReason::SKEW_TOO_HIGH;
//            LOG_WARN("TrackingQuality", "Skew angle too high: ", quality.skewAngle);
//            return quality;
//        }
//    }

    // All checks passed
    quality.isValid = true;
    quality.failureReason = TrackingFailureReason::NONE;
    LOG_DEBUG("TrackingQuality", "Homography valid - det:", quality.determinant,
              " cond:", quality.conditionNumber, " skew:", quality.skewAngle);

    return quality;
}

TrackingQuality EvaluateHomographyQuality(const cv::Mat& H, int numPoints,
                                          double minDet, double maxDet,
                                          double maxCondition, double maxPerspective) {
    TrackingQuality quality;
    quality.numPoints = numPoints;

    if (H.empty() || H.rows != 3 || H.cols != 3) {
        quality.failureReason = TrackingFailureReason::EMPTY_HOMOGRAPHY;
        LOG_WARN("TrackingQuality", "Homography matrix is empty or invalid size");
        return quality;
    }

    if (numPoints > 0 && numPoints < 4) {
        quality.failureReason = TrackingFailureReason::TOO_FEW_POINTS;
        return quality;
    }

    cv::Mat H_normalized = H.clone();
    if (H.type() == CV_64F) {
        double h33 = H.at<double>(2, 2);
        if (std::abs(h33) > 1e-10) H_normalized /= h33;
    } else if (H.type() == CV_32F) {
        float h33 = H.at<float>(2, 2);
        if (std::abs(h33) > 1e-10f) H_normalized /= h33;
    }

    cv::Mat A;
    if (H_normalized.type() == CV_64F) {
        A = (cv::Mat_<double>(2, 2) <<
             H_normalized.at<double>(0, 0), H_normalized.at<double>(0, 1),
             H_normalized.at<double>(1, 0), H_normalized.at<double>(1, 1));
    } else {
        cv::Mat H_64F;
        H_normalized.convertTo(H_64F, CV_64F);
        A = (cv::Mat_<double>(2, 2) <<
             H_64F.at<double>(0, 0), H_64F.at<double>(0, 1),
             H_64F.at<double>(1, 0), H_64F.at<double>(1, 1));
    }

    quality.determinant = cv::determinant(A);

    if (std::abs(quality.determinant) < minDet) {
        quality.failureReason = TrackingFailureReason::DETERMINANT_TOO_SMALL;
        LOG_WARN("TrackingQuality", "Determinant too small: ", quality.determinant);
        return quality;
    }
    if (std::abs(quality.determinant) > maxDet) {
        quality.failureReason = TrackingFailureReason::DETERMINANT_TOO_LARGE;
        LOG_WARN("TrackingQuality", "Determinant too large: ", quality.determinant);
        return quality;
    }

    cv::Mat w, u, vt;
    cv::SVD::compute(A, w, u, vt);
    if (w.rows >= 2 && std::abs(w.at<double>(1, 0)) > 1e-10)
        quality.conditionNumber = w.at<double>(0, 0) / w.at<double>(1, 0);
    else
        quality.conditionNumber = std::numeric_limits<double>::infinity();

    if (quality.conditionNumber > maxCondition) {
        quality.failureReason = TrackingFailureReason::CONDITION_NUMBER_TOO_HIGH;
        LOG_WARN("TrackingQuality", "Condition number too high: ", quality.conditionNumber);
        return quality;
    }

    double h31 = H_normalized.type() == CV_64F ?
                 H_normalized.at<double>(2, 0) : H_normalized.at<float>(2, 0);
    double h32 = H_normalized.type() == CV_64F ?
                 H_normalized.at<double>(2, 1) : H_normalized.at<float>(2, 1);

    if (std::abs(h31) > maxPerspective || std::abs(h32) > maxPerspective) {
        quality.failureReason = TrackingFailureReason::PERSPECTIVE_DISTORTION;
        LOG_WARN("TrackingQuality", "Excessive perspective distortion: h31=", h31, " h32=", h32);
        return quality;
    }

    quality.isValid = true;
    quality.failureReason = TrackingFailureReason::NONE;
    return quality;
}

bool IsHomographyValid(cv::Mat H) {
    if (H.empty() || H.rows != 3 || H.cols != 3) {
        return false;
    }

    const double det = H.at<double>(0, 0) * H.at<double>(1, 1) -
                       H.at<double>(1, 0) * H.at<double>(0, 1);
    return 1 / N < fabs(det) && fabs(det) < N;
}

std::string getMatType(const cv::Mat& mat) {
    int type = mat.type();
    int depth = CV_MAT_DEPTH(type);
    int channels = CV_MAT_CN(type);

    std::string r;

    switch (depth) {
        case CV_8U:
            r = "8U";
            break;
        case CV_8S:
            r = "8S";
            break;
        case CV_16U:
            r = "16U";
            break;
        case CV_16S:
            r = "16S";
            break;
        case CV_32S:
            r = "32S";
            break;
        case CV_32F:
            r = "32F";
            break;
        case CV_64F:
            r = "64F";
            break;
        default:
            r = "User";
            break;
    }

    r += "C" + std::to_string(channels);
    return r;
}

cv::Mat parseMat(uchar imageData[], size_t cols, size_t rows, bool isGrey,
                 size_t padding, uchar borderValue)
{
    cv::Mat gray;

    if (isGrey) {
        gray = cv::Mat((int)rows, (int)cols, CV_8UC1, imageData);
    } else {
        cv::cvtColor(cv::Mat((int)rows, (int)cols, CV_8UC4, imageData),
                     gray, cv::COLOR_RGBA2GRAY);
    }

    cv::Mat padded;
    if (padding == 0) {
        padded = gray;
    } else {
        cv::copyMakeBorder(gray, padded,
                           (int)padding, (int)padding, (int)padding, (int)padding,
                           cv::BORDER_CONSTANT, cv::Scalar(borderValue));
    }

//    cv::Mat blurred;
//    cv::GaussianBlur(padded, blurred, cv::Size(5,5), 3, 3, cv::BORDER_DEFAULT);
    return padded;
}

// void DrawKeypointsOnCanvas(std::vector<cv::KeyPoint> keyPts, const char
// *canvasId, const char *color) {
//     for (int i = 0; i < keyPts.size(); i++) {
//         EM_ASM({
//                        const canvasId = UTF8ToString($0);
//                        const color = UTF8ToString($1);
//                        const canvas = document.getElementById(canvasId);
//                        const ctx = canvas.getContext("2d");
//                        ctx.fillStyle = color;
//                        ctx.fillRect($2, $3, 1, 1);
//                }, canvasId, color, keyPts[i].pt.x, keyPts[i].pt.y);
//     }
// }

cv::Mat computeHomographyUV(const cv::Mat &H_pix, size_t frameWidth,
                            size_t frameHeight, size_t dstWidth,
                            size_t dstHeight) {
    CV_Assert(H_pix.rows == 3 && H_pix.cols == 3 && H_pix.type() == CV_32F);

    auto w_src = static_cast<float>(frameWidth),
         h_src = static_cast<float>(frameHeight);
    auto w_dst = static_cast<float>(dstWidth),
         h_dst = static_cast<float>(dstHeight);

    cv::Mat Tsrc = (cv::Mat_<float>(3, 3) << w_src, 0.0, 0.0, 0.0, h_src, 0.0,
                    0.0, 0.0, 1.0);
    cv::Mat Tdst = (cv::Mat_<float>(3, 3) << w_dst, 0.0, 0.0, 0.0, h_dst, 0.0,
                    0.0, 0.0, 1.0);

    cv::Mat H_uv = Tdst.inv() * H_pix * Tsrc;  // CV_32F, 3×3

    return H_uv;
}
