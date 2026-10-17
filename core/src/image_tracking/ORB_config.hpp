#pragma once

#include <string>

struct ORBConfig {
    // ORB detector/descriptor
    int nFeatures           = 2000;
    int nDescriptorFeatures = 3000;
    std::string descriptorType = "ORB"; // "ORB" or "BEBLID"

    int   orbLevels        = 12;
    float orbScaleFactor   = 1.15f;
    int   orbFastThreshold = 12;

    // Optical flow
    int opticalFlowWinSize   = 15;
    int opticalFlowMaxLevel  = 5;
    int opticalFlowIterations = 1;  // iterative refinement passes

    // Shi-Tomasi for optical flow tracking (instead of ORB keypoints)
    bool useShiTomasi         = false;
    int  shiTomasiMaxCorners  = 500;
    double shiTomasiQuality   = 0.01;
    double shiTomasiMinDist   = 5.0;

    // Kalman filter smoothing
    bool useKalman            = false;
    float kalmanProcessNoise  = 1.0f;
    float kalmanMeasNoise     = 10.0f;

    // ESM correction after optical flow
    bool useESM               = false;
    int  esmMaxIter           = 5;
    double esmEps             = 0.05;
    int  esmPointStep         = 10;   // grid spacing of sample points on the template

    // OBD-style periodic correction via Shi-Tomasi in warped frame
    bool useOBDCorrection        = false;
    int  obdCorrectionInterval   = 10;   // every N frames
    double obdCorrectionThreshold = 50.0; // max corner distance to accept correction
    int  obdCorrectionGridSize   = 8;    // NxN grid for uniform point sampling
    bool obdUpdateTrackingPts    = false; // re-anchor trackFramePts after correction
    double obdCorrectionAlpha    = 0.3;  // blend factor: 0=no correction, 1=full correction

    // Forward-backward consistency check for optical flow
    bool  useFBCheck         = false;
    float fbCheckThreshold   = 1.0f;  // max pixel distance between orig and back-tracked point

    // Fraction of the tracked points that must survive a frame. Four points
    // are enough to fit a homography, so without this the tracker keeps
    // reporting a lock off whatever handful of points drifted onto the
    // background after the target left the frame.
    float minTrackedRatio    = 0.35f;

    // RANSAC
    double ransacReprojThreshold  = 3.0;

    // Matching
    float goodMatchRatio          = 0.8f;
    int   minMatchesForRecognition = 50;

    // Validation
    double minDeterminant           = 0.1;
    double maxDeterminant           = 10.0;
    double maxConditionNumber       = 100.0;
    double maxPerspectiveDistortion = 0.002;
};
