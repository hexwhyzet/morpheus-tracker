#include "opencv2/core/mat.hpp"
#include "../image_tracking/common/homography_result.hpp"

void DrawResult(cv::Mat &templateImg, cv::Mat &frame, HomographyResult &result, const std::string &name);

void DrawMatWithPoints(cv::Mat &img, std::vector <cv::Point2f> &points, const std::string &);

void DrawMatWithPointsTransform(cv::Mat &img, std::vector <cv::Point2f> &pointsA, std::vector <cv::Point2f> &pointsB,
                                const std::string &name);

void DrawSideBySideWithPoints(
    const cv::Mat &templateImg,
    const cv::Mat &image,
    const std::vector <cv::Point2f> &pointsTemplate,
    const std::vector <cv::Point2f> &pointsImage,
    const std::string &windowName
);

cv::Mat DrawResult(cv::Mat &image, std::vector <cv::Point2f> &corners, bool clone = false);