#include <gtest/gtest.h>
#include "framework/dataset_format.hpp"
#include "src/image_tracking/ORB.hpp"
#include "src/image_tracking/OBD.hpp"
#include <opencv2/opencv.hpp>
#include <fstream>
#include <iostream>

using namespace morpheus::test;

// Base fixture for tracker tests
class TrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string datasetPath = "../datasets/example_dataset/dataset.json";
        std::ifstream file(datasetPath);
        if (file.good()) {
            dataset = DatasetConfig::loadFromFile(datasetPath);
            hasDataset = true;
        } else {
            hasDataset = false;
        }
    }

    DatasetConfig dataset;
    bool hasDataset = false;
};

TEST_F(TrackerTest, ORBTracker_BasicTracking) {
    if (!hasDataset) {
        GTEST_SKIP() << "Dataset not available";
    }

    ORBTracker tracker(false);

    std::string templatePath = dataset.getTemplatePath(0);
    cv::Mat templateImg = cv::imread(templatePath, cv::IMREAD_GRAYSCALE);

    if (templateImg.empty()) {
        GTEST_SKIP() << "Template image not found: " << templatePath;
    }

    tracker.initTemplate(templateImg.data, templateImg.cols, templateImg.rows, true);

    SequenceGroundTruth gt = dataset.loadSequenceGroundTruth(0);

    int successCount = 0;
    int totalFrames = 0;

    const SequenceInfo& seq = dataset.sequences[0];
    for (size_t i = 0; i < std::min(gt.frames.size(), size_t(5)); ++i) {
        std::string framePath = seq.framesDir + "/" + gt.frames[i];
        cv::Mat frame = cv::imread(framePath, cv::IMREAD_GRAYSCALE);

        if (frame.empty()) {
            continue;
        }

        tracker.processFrame(frame.data, frame.cols, frame.rows, true);
        HomographyResult result = tracker.getHomography();

        totalFrames++;
        if (result.success) {
            successCount++;
        }

        if (gt.groundTruth.count(i) > 0) {
            const FrameGroundTruth& frameGT = gt.groundTruth.at(i);

            if (frameGT.visible && result.success && frameGT.corners.has_value()) {
                std::vector<double> predictedCorners(result.corners, result.corners + 8);
                double error = gt_utils::computeCornerError(predictedCorners, frameGT.corners.value());

                std::cout << "Frame " << i << " corner error: " << error << " px" << std::endl;
            }
        }
    }

    if (totalFrames > 0) {
        double successRate = static_cast<double>(successCount) / totalFrames;
        std::cout << "Success rate: " << successRate * 100 << "%" << std::endl;

        EXPECT_GT(successRate, 0.3) << "Success rate too low";
    }
}

TEST_F(TrackerTest, OBDTracker_BasicTracking) {
    if (!hasDataset) {
        GTEST_SKIP() << "Dataset not available";
    }

    OBDTracker tracker(false);

    std::string templatePath = dataset.getTemplatePath(0);
    cv::Mat templateImg = cv::imread(templatePath, cv::IMREAD_GRAYSCALE);

    if (templateImg.empty()) {
        GTEST_SKIP() << "Template image not found: " << templatePath;
    }

    tracker.initTemplate(templateImg.data, templateImg.cols, templateImg.rows, true);

    SequenceGroundTruth gt = dataset.loadSequenceGroundTruth(0);

    int successCount = 0;
    int totalFrames = 0;

    const SequenceInfo& seq = dataset.sequences[0];
    for (size_t i = 0; i < std::min(gt.frames.size(), size_t(5)); ++i) {
        std::string framePath = seq.framesDir + "/" + gt.frames[i];
        cv::Mat frame = cv::imread(framePath, cv::IMREAD_GRAYSCALE);

        if (frame.empty()) {
            continue;
        }

        tracker.processFrame(frame.data, frame.cols, frame.rows, true);
        HomographyResult result = tracker.getHomography();

        totalFrames++;
        if (result.success) {
            successCount++;
        }
    }

    if (totalFrames > 0) {
        double successRate = static_cast<double>(successCount) / totalFrames;
        std::cout << "OBD Success rate: " << successRate * 100 << "%" << std::endl;

        EXPECT_GT(successRate, 0.3) << "Success rate too low";
    }
}

// TODO: compare ORB vs OBD across different parameter sets
