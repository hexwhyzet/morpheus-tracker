#include <gtest/gtest.h>
#include "src/image_tracking/common/utils.hpp"
#include <opencv2/opencv.hpp>

TEST(UtilsTest, IsHomographyValid_Identity) {
    cv::Mat H = cv::Mat::eye(3, 3, CV_64F);
    EXPECT_TRUE(IsHomographyValid(H));
}

TEST(UtilsTest, IsHomographyValid_InvalidDeterminant) {
    cv::Mat H = cv::Mat::zeros(3, 3, CV_64F);
    EXPECT_FALSE(IsHomographyValid(H));
}

TEST(UtilsTest, EvaluateHomographyQuality_Identity) {
    cv::Mat H = cv::Mat::eye(3, 3, CV_64F);
    std::vector<cv::Point2f> points = {
        cv::Point2f(0, 0),
        cv::Point2f(100, 0),
        cv::Point2f(100, 100),
        cv::Point2f(0, 100)
    };

    TrackingQuality quality = EvaluateHomographyQuality(H, (int)points.size());

    EXPECT_TRUE(quality.isValid);
    EXPECT_EQ(quality.failureReason, TrackingFailureReason::NONE);
    EXPECT_EQ(quality.numPoints, 4);
}

TEST(UtilsTest, EvaluateHomographyQuality_TooFewPoints) {
    cv::Mat H = cv::Mat::eye(3, 3, CV_64F);
    std::vector<cv::Point2f> points = {
        cv::Point2f(0, 0),
        cv::Point2f(100, 0)
    };

    TrackingQuality quality = EvaluateHomographyQuality(H, (int)points.size());

    EXPECT_FALSE(quality.isValid);
    EXPECT_EQ(quality.failureReason, TrackingFailureReason::TOO_FEW_POINTS);
}

// TODO: cover more scenarios
