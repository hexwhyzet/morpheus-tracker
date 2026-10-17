#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>

#include "esm.hpp"
#include "image_tracking/common/logger.hpp"

using namespace std;
using namespace cv;

Mat computeGradX(const Mat &img) {
    Mat gx;
    Sobel(img, gx, CV_64F, 1, 0, 3);
    return gx;
}

Mat computeGradY(const Mat &img) {
    Mat gy;
    Sobel(img, gy, CV_64F, 0, 1, 3);
    return gy;
}

static Mat buildHomography(const Mat &p) {
    CV_Assert(p.rows == 8 && p.cols == 1);

    Mat H = Mat::eye(3, 3, CV_64F);

    H.at<double>(0, 0) = 1.0 + p.at<double>(0);
    H.at<double>(0, 1) = p.at<double>(1);
    H.at<double>(0, 2) = p.at<double>(2);

    H.at<double>(1, 0) = p.at<double>(3);
    H.at<double>(1, 1) = 1.0 + p.at<double>(4);
    H.at<double>(1, 2) = p.at<double>(5);

    H.at<double>(2, 0) = p.at<double>(6);
    H.at<double>(2, 1) = p.at<double>(7);
    // (2,2) = 1

    return H;
}

static cv::Mat homographyToParams(const cv::Mat &H) {
    CV_Assert(H.rows == 3 && H.cols == 3 && H.type() == CV_64F);

    // Optional check/normalization: if H(2,2) is not exactly 1, you might do:
    double h22 = H.at<double>(2, 2);
    if (std::abs(h22 - 1.0) > 1e-12) {
        cv::Mat Hnorm = H.clone();
        Hnorm /= h22;
        return homographyToParams(Hnorm);
    }
    // Or just rely on the input already having H(2,2) = 1.

    cv::Mat p = cv::Mat::zeros(8, 1, CV_64F);

    p.at<double>(0) = H.at<double>(0, 0) - 1.0; // p0
    p.at<double>(1) = H.at<double>(0, 1);       // p1
    p.at<double>(2) = H.at<double>(0, 2);       // p2

    p.at<double>(3) = H.at<double>(1, 0);       // p3
    p.at<double>(4) = H.at<double>(1, 1) - 1.0; // p4
    p.at<double>(5) = H.at<double>(1, 2);       // p5

    p.at<double>(6) = H.at<double>(2, 0);       // p6
    p.at<double>(7) = H.at<double>(2, 1);       // p7

    return p;
}

static void warpWithParams(const Mat &src, Mat &dst, Size sz, const Mat &p) {
    cv::Mat H = buildHomography(p);
    warpPerspective(src, dst, H, sz, INTER_LINEAR);
}

static cv::Mat computeJacobian(const cv::Mat &gxT, const cv::Mat &gyT,
                               const cv::Mat &gxW, const cv::Mat &gyW,
                               const cv::Mat &p, cv::Size sz) {
    cv::Mat J(sz.area(), 8, CV_64F);  // each pixel => 1 row x 8 columns

    // Rebuild the homography
    cv::Mat H = buildHomography(p);

    int idx = 0;
    for (int y = 0; y < sz.height; y++) {
        for (int x = 0; x < sz.width; x++, idx++) {
            double denom = H.at<double>(2, 0) * x + H.at<double>(2, 1) * y + 1.0;

            // sum of gradients from template & warped
            double Ix = gxT.at<double>(y, x) + gxW.at<double>(y, x);
            double Iy = gyT.at<double>(y, x) + gyW.at<double>(y, x);

            // We'll define Nx = numerator_x = (1+p0)*x + p1*y + p2
            //                Ny = numerator_y = p3*x + (1+p4)*y + p5
            //                D  = denominator = p6*x + p7*y + 1
            // We can also do partial derivatives directly with chain rule.

            double denom2 = denom * denom;
            // We'll do them parameter by parameter:

            // p0 => in Nx => Nx' = x
            // so partial u/partial p0 = [ x / denom ] - [ Nx*( d(denom)/dp0 ) / denom^2 ]
            // but denom doesn't depend on p0 => d(denom)/dp0=0 => simpler:
            double dudp0 = (x / denom);
            double dvdp0 = 0.0; // because p3..p5 are separate

            double dudp1 = (y / denom);
            double dvdp1 = 0.0;

            double dudp2 = 1.0 / denom;
            double dvdp2 = 0.0;

            double dudp3 = 0.0;
            double dvdp3 = (x / denom);

            double dudp4 = 0.0;
            double dvdp4 = (y / denom);

            double dudp5 = 0.0;
            double dvdp5 = 1.0 / denom;

            // p6, p7 => appear in denominator => chain rule
            //  u = Nx / D => du/dp6 = [ 0 - Nx*(dD/dp6) ] / D^2
            // but Nx doesn't involve p6 => so partial Nx / partial p6=0
            // dD/dp6 = x
            // => du/dp6 = - (Nx*x) / denom^2
            double Nx = (1.0 + p.at<double>(0)) * x + p.at<double>(1) * y + p.at<double>(2);
            double Ny = p.at<double>(3) * x + (1.0 + p.at<double>(4)) * y + p.at<double>(5);
            double dDdp6 = x;
            double dDdp7 = y;

            double dudp6 = -(Nx * dDdp6) / denom2;
            double dvdp6 = -(Ny * dDdp6) / denom2;

            double dudp7 = -(Nx * dDdp7) / denom2;
            double dvdp7 = -(Ny * dDdp7) / denom2;

            // now fill row in J => Ix * dudp_i + Iy * dvdp_i
            // for i=0..7
            J.at<double>(idx, 0) = Ix * dudp0 + Iy * dvdp0; // p0
            J.at<double>(idx, 1) = Ix * dudp1 + Iy * dvdp1; // p1
            J.at<double>(idx, 2) = Ix * dudp2 + Iy * dvdp2; // p2
            J.at<double>(idx, 3) = Ix * dudp3 + Iy * dvdp3; // p3
            J.at<double>(idx, 4) = Ix * dudp4 + Iy * dvdp4; // p4
            J.at<double>(idx, 5) = Ix * dudp5 + Iy * dvdp5; // p5
            J.at<double>(idx, 6) = Ix * dudp6 + Iy * dvdp6; // p6
            J.at<double>(idx, 7) = Ix * dudp7 + Iy * dvdp7; // p7
        }
    }

    return J;
}

cv::Mat computeResiduals(const cv::Mat &templ, const cv::Mat &warped) {
    cv::Mat diff = templ - warped;  // float or double
    diff.convertTo(diff, CV_64F);
    // Reshape to [N x 1]
    return diff.reshape(1, diff.total());
}

Mat computeUpdate(const Mat &J, const Mat &residuals) {
    Mat H = J.t() * J;
    Mat b = J.t() * residuals;
    return -H.inv(DECOMP_SVD) * b;
}

void esmRefine(const Mat &templateImg, const Mat &currentImg,
               Mat &H, int maxIter, double minNorm) {
    Mat gxT = computeGradX(templateImg);
    Mat gyT = computeGradY(templateImg);

    Mat p = homographyToParams(H);

    for (int iter = 0; iter < maxIter; iter++) {
        Mat warped;
        warpWithParams(currentImg, warped, templateImg.size(), p);

        Mat gxW = computeGradX(warped);
        Mat gyW = computeGradY(warped);

        Mat J = computeJacobian(gxT, gyT, gxW, gyW, p, templateImg.size());

        Mat residual = computeResiduals(templateImg, warped);

        Mat dp = computeUpdate(J, residual);

        p += dp;

        double nd = norm(dp);

        LOG_DEBUG("ESM", "nd: ", nd);

        if (nd < minNorm) {
            cout << "[ESM] Iteration " << iter << " converged, ||dp||=" << nd << endl;
            break;
        }
    }

    H = buildHomography(p);
}

void esmRefinePyr(const Mat &templateImg, const Mat &image, Mat &H, int maxIterPerLevel, double minNorm, int maxLevel) {
    std::vector<Mat> templatePyr, currentPyr;
    buildPyramid(templateImg, templatePyr, maxLevel);
    buildPyramid(image, currentPyr, maxLevel);

    // Start at the coarsest level and refine downwards
    for (int level = maxLevel; level >= 0; level--) {
        double scale = 1.0 / (1 << level);

        LOG_DEBUG("ESM", "Scale: ", scale);

        Mat H_scaled = H.clone();

        // Bring the homography into this level's scale: pre-scale * H * post-scale
        Mat S = (Mat_<double>(3, 3)
                <<
                scale, 0, 0,
                0, scale, 0,
                0, 0, 1);
        Mat Sinv = (Mat_<double>(3, 3)
                <<
                1.0 / scale, 0, 0,
                0, 1.0 / scale, 0,
                0, 0, 1);

        H_scaled = S * H_scaled * Sinv;

        esmRefine(templatePyr[level], currentPyr[level], H_scaled, maxIterPerLevel, minNorm);

        // Scale back out for the next, finer level
        H = Sinv * H_scaled * S;
    }
}
