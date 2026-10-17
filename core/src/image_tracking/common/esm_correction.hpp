#pragma once

#include <cmath>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Sparse ESM (Efficient Second-order Minimization) homography refinement.
//
// Minimizes the photometric error between the template and the warped current
// frame over a sparse set of points. Acts as a sub-pixel refiner on top of a
// coarse initial homography (e.g. from ORB optical flow).
//
// Algorithm (Benhimane & Malis, 2004):
//   e    = I_current(H*p) - I_template(p)
//   J    = (J_fwd + J_inv) / 2        - averaged Jacobian (the ESM trick)
//   dH   = -(J^T J)^{-1} J^T e        - least-squares optimization step
//   H   <- H * exp(dH)                - update in homography space
//
// Parameters:
//   templateImg  - grayscale template
//   templatePts  - points on the template (in template coordinates)
//   currentImg   - grayscale current frame
//   H            - initial template->current homography (in/out)
//   maxIter      - iteration cap
//   eps          - convergence threshold (norm of dH)
//
// Returns the number of iterations performed (0 = converged immediately, or failure).

static inline int esmCorrect(const cv::Mat& templateImg,
                              const std::vector<cv::Point2f>& templatePts,
                              const cv::Mat& currentImg,
                              cv::Mat& H,
                              int maxIter = 10,
                              double eps = 0.05)
{
    CV_Assert(!templateImg.empty() && !currentImg.empty());
    CV_Assert(templateImg.type() == CV_8UC1 && currentImg.type() == CV_8UC1);
    CV_Assert(H.rows == 3 && H.cols == 3);

    const int N = (int)templatePts.size();
    if (N < 4) return 0;

    cv::Mat Hf;
    H.convertTo(Hf, CV_64F);

    // Template gradients are fixed across iterations, so precompute them once
    cv::Mat gradTx, gradTy, gradCx, gradCy;
    cv::Sobel(templateImg, gradTx, CV_32F, 1, 0, 1);
    cv::Sobel(templateImg, gradTy, CV_32F, 0, 1, 1);
    cv::Sobel(currentImg,  gradCx, CV_32F, 1, 0, 1);
    cv::Sobel(currentImg,  gradCy, CV_32F, 0, 1, 1);

    int iter = 0;
    for (; iter < maxIter; ++iter) {
        std::vector<cv::Point2f> warpedPts;
        cv::perspectiveTransform(templatePts, warpedPts, Hf);

        // Accumulate error and Jacobian over the 8 homography DOF
        // (h00..h21, with h22 fixed at 1)
        cv::Mat JtJ = cv::Mat::zeros(8, 8, CV_64F);
        cv::Mat Jte = cv::Mat::zeros(8, 1, CV_64F);
        double totalError = 0.0;
        int validPts = 0;

        for (int i = 0; i < N; ++i) {
            const cv::Point2f& tp = templatePts[i];
            const cv::Point2f& wp = warpedPts[i];

            // Both samples need a 1px border for bilinear interpolation
            if (wp.x < 1 || wp.x >= currentImg.cols - 1 ||
                wp.y < 1 || wp.y >= currentImg.rows - 1)
                continue;

            if (tp.x < 1 || tp.x >= templateImg.cols - 1 ||
                tp.y < 1 || tp.y >= templateImg.rows - 1)
                continue;

            // Current-frame intensity at the warped point
            float Ic = 0.f;
            {
                int x0 = (int)wp.x, y0 = (int)wp.y;
                float dx = wp.x - x0, dy = wp.y - y0;
                Ic = (1-dy)*((1-dx)*currentImg.at<uchar>(y0,   x0)   + dx*currentImg.at<uchar>(y0,   x0+1)) +
                     dy    *((1-dx)*currentImg.at<uchar>(y0+1, x0)   + dx*currentImg.at<uchar>(y0+1, x0+1));
            }

            // Template intensity at the source point
            float It = 0.f;
            {
                int x0 = (int)tp.x, y0 = (int)tp.y;
                float dx = tp.x - x0, dy = tp.y - y0;
                It = (1-dy)*((1-dx)*templateImg.at<uchar>(y0,   x0)   + dx*templateImg.at<uchar>(y0,   x0+1)) +
                     dy    *((1-dx)*templateImg.at<uchar>(y0+1, x0)   + dx*templateImg.at<uchar>(y0+1, x0+1));
            }

            double e = (double)(Ic - It);
            totalError += e * e;

            // Current-frame gradient at the warped point
            float gCx = 0.f, gCy = 0.f;
            {
                int x0 = (int)wp.x, y0 = (int)wp.y;
                float dx = wp.x - x0, dy = wp.y - y0;
                gCx = (1-dy)*((1-dx)*gradCx.at<float>(y0,x0)   + dx*gradCx.at<float>(y0,x0+1)) +
                      dy    *((1-dx)*gradCx.at<float>(y0+1,x0) + dx*gradCx.at<float>(y0+1,x0+1));
                gCy = (1-dy)*((1-dx)*gradCy.at<float>(y0,x0)   + dx*gradCy.at<float>(y0,x0+1)) +
                      dy    *((1-dx)*gradCy.at<float>(y0+1,x0) + dx*gradCy.at<float>(y0+1,x0+1));
            }

            // Template gradient at tp, the second half of the ESM average
            float gTx = 0.f, gTy = 0.f;
            {
                int x0 = (int)tp.x, y0 = (int)tp.y;
                float dx = tp.x - x0, dy = tp.y - y0;
                gTx = (1-dy)*((1-dx)*gradTx.at<float>(y0,x0)   + dx*gradTx.at<float>(y0,x0+1)) +
                      dy    *((1-dx)*gradTx.at<float>(y0+1,x0) + dx*gradTx.at<float>(y0+1,x0+1));
                gTy = (1-dy)*((1-dx)*gradTy.at<float>(y0,x0)   + dx*gradTy.at<float>(y0,x0+1)) +
                      dy    *((1-dx)*gradTy.at<float>(y0+1,x0) + dx*gradTy.at<float>(y0+1,x0+1));
            }

            // ESM: 0.5 * (grad_current(wp) + grad_template(tp));
            // both share the same warp Jacobian dW/dp
            double gx = 0.5 * (gCx + gTx);
            double gy = 0.5 * (gCy + gTy);

            // Warp Jacobian dw/dp at tp over the 8 homography parameters,
            // where w = H*p / (h20*x + h21*y + h22)
            double x = tp.x, y = tp.y;
            double h20 = Hf.at<double>(2,0), h21 = Hf.at<double>(2,1), h22 = Hf.at<double>(2,2);
            double denom = h20*x + h21*y + h22;
            if (std::abs(denom) < 1e-8) continue;
            double inv_denom  = 1.0 / denom;
            double inv_denom2 = inv_denom * inv_denom;

            double wx = wp.x, wy = wp.y;

            // Rows are dw_x/dp and dw_y/dp for
            // p = [h00, h01, h02, h10, h11, h12, h20, h21]
            double Jw[2][8] = {
                { x*inv_denom,  y*inv_denom,  inv_denom, 0, 0, 0, -wx*x*inv_denom2, -wx*y*inv_denom2 },
                { 0, 0, 0,  x*inv_denom,  y*inv_denom,  inv_denom, -wy*x*inv_denom2, -wy*y*inv_denom2 }
            };

            // J_i = [gx, gy] * Jw  (1×8)
            double J[8];
            for (int k = 0; k < 8; ++k)
                J[k] = gx * Jw[0][k] + gy * Jw[1][k];

            for (int r = 0; r < 8; ++r) {
                Jte.at<double>(r) += J[r] * e;
                for (int c = 0; c < 8; ++c)
                    JtJ.at<double>(r, c) += J[r] * J[c];
            }
            validPts++;
        }

        if (validPts < 4) break;

        // Solve the normal equations JtJ * dh = -Jte
        cv::Mat dh;
        if (!cv::solve(JtJ, -Jte, dh, cv::DECOMP_SVD)) break;

        // Update as H = H * (I + dH), with dH unpacked from dh
        cv::Mat dH = cv::Mat::eye(3, 3, CV_64F);
        dH.at<double>(0,0) += dh.at<double>(0);
        dH.at<double>(0,1) += dh.at<double>(1);
        dH.at<double>(0,2) += dh.at<double>(2);
        dH.at<double>(1,0) += dh.at<double>(3);
        dH.at<double>(1,1) += dh.at<double>(4);
        dH.at<double>(1,2) += dh.at<double>(5);
        dH.at<double>(2,0) += dh.at<double>(6);
        dH.at<double>(2,1) += dh.at<double>(7);

        Hf = Hf * dH;

        // Renormalize so H[2,2] == 1
        double h22 = Hf.at<double>(2,2);
        if (std::abs(h22) > 1e-8) Hf /= h22;

        double norm = cv::norm(dh);
        if (norm < eps) { iter++; break; }
    }

    Hf.convertTo(H, H.type());
    return iter;
}
