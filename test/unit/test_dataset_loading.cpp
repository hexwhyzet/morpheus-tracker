#include <gtest/gtest.h>
#include "framework/dataset_format.hpp"
#include <fstream>

using namespace morpheus::test;

TEST(DatasetTest, LoadExampleDataset) {
    std::string datasetPath = "../datasets/example_dataset/dataset.json";

    std::ifstream file(datasetPath);
    if (!file.good()) {
        GTEST_SKIP() << "Dataset file not found: " << datasetPath;
    }

    DatasetConfig dataset = DatasetConfig::loadFromFile(datasetPath);

    EXPECT_EQ(dataset.name, "example_dataset");
    EXPECT_GE(dataset.templates.size(), 1);
    EXPECT_GE(dataset.sequences.size(), 1);
}

TEST(DatasetTest, LoadGroundTruth) {
    std::string gtPath = "../datasets/example_dataset/sequences/seq_000/gt.json";

    std::ifstream file(gtPath);
    if (!file.good()) {
        GTEST_SKIP() << "Ground truth file not found: " << gtPath;
    }

    SequenceGroundTruth gt = SequenceGroundTruth::loadFromFile(gtPath);

    EXPECT_GE(gt.frames.size(), 1);
    EXPECT_GE(gt.groundTruth.size(), 1);
}

TEST(DatasetTest, GroundTruthValidation) {
    FrameGroundTruth gt;
    gt.frameIndex = 0;
    gt.visible = true;
    gt.corners = {100.0, 100.0, 400.0, 100.0, 400.0, 300.0, 100.0, 300.0};

    EXPECT_TRUE(gt_utils::isValid(gt));
}

TEST(DatasetTest, CornersToHomographyConversion) {
    std::vector<double> corners = {0.0, 0.0, 640.0, 0.0, 640.0, 480.0, 0.0, 480.0};

    std::vector<double> H = gt_utils::cornersToHomography(corners, 640, 480);
    EXPECT_EQ(H.size(), 9);

    std::vector<double> cornersBack = gt_utils::homographyToCorners(H, 640, 480);
    EXPECT_EQ(cornersBack.size(), 8);

    // The round trip must land back on the original corners
    for (size_t i = 0; i < corners.size(); ++i) {
        EXPECT_NEAR(corners[i], cornersBack[i], 1.0);
    }
}
