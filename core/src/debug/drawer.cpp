#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "drawer.hpp"
#include <iostream>

void DrawResult(cv::Mat &templateImg, cv::Mat &frame, HomographyResult &result, const std::string &name) {
    if (result.success) {
        // Project the template corners onto the frame
        std::vector <cv::Point2f> projected;
        std::vector <cv::Point2f> corners = {
            cv::Point(0, 0),
            cv::Point(templateImg.cols, 0),
            cv::Point(templateImg.cols, templateImg.rows),
            cv::Point(0, templateImg.rows)
        };
        cv::Mat H_mat(3, 3, CV_64F, result.H);
        cv::perspectiveTransform(corners, projected, H_mat);

        // Grayscale to BGR so the outline can be drawn in color
        cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);

        for (size_t i = 0; i < 4; ++i) {
            cv::line(frame, projected[i], projected[(i + 1) % 4], cv::Scalar(0, 255, 0), 1);
        }

        cv::imshow(name, frame);
    }
}

void DrawMatWithPoints(cv::Mat &img, std::vector <cv::Point2f> &points, const std::string &name) {
    auto clonedImg = img.clone();
    cv::cvtColor(clonedImg, clonedImg, cv::COLOR_GRAY2BGR);
    for (const auto &point: points) {
        cv::circle(clonedImg, point, 3, cv::Scalar(0, 255, 0), -1);
    }
    cv::imshow(name, clonedImg);
}

void DrawMatWithPointsTransform(cv::Mat &img, std::vector <cv::Point2f> &pointsA, std::vector <cv::Point2f> &pointsB,
                                const std::string &name) {
    assert(pointsA.size() == pointsB.size());
    auto clonedImg = img.clone();
    cv::cvtColor(clonedImg, clonedImg, cv::COLOR_GRAY2BGR);
    for (size_t i = 0; i < pointsA.size(); ++i) {
        cv::circle(clonedImg, pointsA[i], 3, cv::Scalar(0, 255, 0), -1);
        cv::circle(clonedImg, pointsB[i], 3, cv::Scalar(0, 0, 255), -1);
        cv::line(clonedImg, pointsA[i], pointsB[i], cv::Scalar(0, 255, 0), 2);
    }
    cv::imshow(name, clonedImg);
}

void DrawSideBySideWithPoints(
    const cv::Mat &templateImg,
    const cv::Mat &image,
    const std::vector <cv::Point2f> &pointsTemplate,
    const std::vector <cv::Point2f> &pointsImage,
    const std::string &windowName
) {
    assert(pointsTemplate.size() == pointsImage.size());

    // Convert to BGR if grayscale
    cv::Mat templateBGR, imageBGR;
    if (templateImg.channels() == 1)
        cv::cvtColor(templateImg, templateBGR, cv::COLOR_GRAY2BGR);
    else
        templateImg.copyTo(templateBGR);

    if (image.channels() == 1)
        cv::cvtColor(image, imageBGR, cv::COLOR_GRAY2BGR);
    else
        image.copyTo(imageBGR);

    // Make heights equal by padding the shorter image
    int height = std::max(templateBGR.rows, imageBGR.rows);
    int width = templateBGR.cols + imageBGR.cols;

    cv::Mat combined(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    templateBGR.copyTo(combined(cv::Rect(0, 0, templateBGR.cols, templateBGR.rows)));
    imageBGR.copyTo(combined(cv::Rect(templateBGR.cols, 0, imageBGR.cols, imageBGR.rows)));

    // Draw points and lines
    for (size_t i = 0; i < pointsTemplate.size(); ++i) {
        cv::Point2f ptA = pointsTemplate[i];
        cv::Point2f ptB = pointsImage[i] + cv::Point2f(static_cast<float>(templateBGR.cols), 0); // shift right

        cv::circle(combined, ptA, 3, cv::Scalar(0, 255, 0), -1);  // green
        cv::circle(combined, ptB, 3, cv::Scalar(0, 0, 255), -1);  // red
        cv::line(combined, ptA, ptB, cv::Scalar(255, 0, 0), 1);   // blue line
    }

    cv::imshow(windowName, combined);
}

cv::Mat Colorize(cv::Mat &image, bool clone) {
    cv::Mat cloned = image.clone();
    if (clone) {
        cv::Mat result;
        result = image.clone();
        if (image.channels() == 1) {
            cv::cvtColor(cloned, result, cv::COLOR_GRAY2RGBA);
        }
        return result;
    } else {
        if (image.channels() == 1) {
            cv::cvtColor(cloned, image, cv::COLOR_GRAY2RGBA);
        }
        return image;
    }
}

cv::Mat DrawResult(cv::Mat &image, std::vector <cv::Point2f>& corners, bool clone) {
    cv::Mat result;
    if (clone) {
        result = image.clone();
    } else {
        result = image;
    }

    std::vector<cv::Point> int_corners;
    for (const auto& pt : corners) {
        int_corners.emplace_back(cv::Point(cvRound(pt.x), cvRound(pt.y)));
    }

    Colorize(result, false);

    const cv::Point *pts[1] = {int_corners.data()};
    int npts[] = {static_cast<int>(int_corners.size())};

    cv::polylines(
        result,
        pts,
        npts,
        1,
        true,
        cv::Scalar(0, 255, 0, 255),
        5
    );
    return result;
}