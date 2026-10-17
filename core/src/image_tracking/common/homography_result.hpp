#pragma once

#include <cmath>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>


struct HomographyResult {
    bool success;
    double H[9];
    double corners[8];
};

struct LastFrameData {
    uchar* data;   // pointer into WASM heap (not owned)
    size_t width;
    size_t height;
};

static inline void FillHomographyResult(HomographyResult &homographyResult,
                                        bool success,
                                        const cv::Mat &H,
                                        const std::vector<cv::Point2f> &warped,
                                        size_t padding = 0) {
    homographyResult.success = success;

    if (!success) {
        return;
    }

    homographyResult.H[0] = H.at<double>(0, 0);
    homographyResult.H[1] = H.at<double>(0, 1);
    homographyResult.H[2] = H.at<double>(0, 2);
    homographyResult.H[3] = H.at<double>(1, 0);
    homographyResult.H[4] = H.at<double>(1, 1);
    homographyResult.H[5] = H.at<double>(1, 2);
    homographyResult.H[6] = H.at<double>(2, 0);
    homographyResult.H[7] = H.at<double>(2, 1);
    homographyResult.H[8] = H.at<double>(2, 2);

    homographyResult.corners[0] = warped[0].x + padding;
    homographyResult.corners[1] = warped[0].y + padding;
    homographyResult.corners[2] = warped[1].x - padding;
    homographyResult.corners[3] = warped[1].y + padding;
    homographyResult.corners[4] = warped[2].x - padding;
    homographyResult.corners[5] = warped[2].y - padding;
    homographyResult.corners[6] = warped[3].x + padding;
    homographyResult.corners[7] = warped[3].y - padding;
}

// Mean corner-to-corner distance between two results, averaged over the 4 corners.
// Handy for checking how far a new result has moved from the current one.
static inline double cornerDistance(const HomographyResult& a, const HomographyResult& b) {
    double sum = 0.0;
    for (int i = 0; i < 4; i++) {
        double dx = a.corners[i*2]     - b.corners[i*2];
        double dy = a.corners[i*2 + 1] - b.corners[i*2 + 1];
        sum += std::sqrt(dx*dx + dy*dy);
    }
    return sum / 4.0;
}