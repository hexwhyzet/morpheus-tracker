#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "frame_source.hpp"
#include "../../test/framework/dataset_format.hpp"

namespace morpheus::benchmark {

namespace fs = std::filesystem;

// ============================================================================
// Uniform sequence representation used by the benchmark
// ============================================================================

struct FrameGT {
    bool annotated = false;   // false → frame is unlabeled, skipped during evaluation
    double corners[8] = {};   // x0y0 x1y1 x2y2 x3y3 in frame pixels
};

struct BenchmarkSequence {
    std::string name;
    std::unique_ptr<FrameSource> frames;
    cv::Mat templateImg;                    // grayscale, frame 0 or a separate file
    std::map<int, FrameGT> gt;             // frameIndex → GT (annotated frames only)
    int totalFrames = 0;
};

// ============================================================================
// POT-280 adapter
//
// Expected layout of datasetDir:
//   videos/V01/V01_1.avi
//   videos/V02/V02_1.avi  ...
//   annotations/V01_1_gt_points.txt
//   annotations/V01_1_annotation_types.txt  ...
// ============================================================================

inline std::vector<BenchmarkSequence> loadFromPOT280(const std::string& datasetDir) {
    fs::path base(datasetDir);
    fs::path videosDir     = base / "videos";
    fs::path annotationsDir = base / "annotations";

    if (!fs::exists(videosDir))
        throw std::runtime_error("videos/ not found in: " + datasetDir);
    if (!fs::exists(annotationsDir))
        throw std::runtime_error("annotations/ not found in: " + datasetDir);

    std::vector<BenchmarkSequence> sequences;

    // Collect every .avi file and sort for a deterministic order
    std::vector<fs::path> aviFiles;
    for (const auto& sceneDir : fs::directory_iterator(videosDir)) {
        if (!sceneDir.is_directory()) continue;
        for (const auto& entry : fs::directory_iterator(sceneDir)) {
            if (entry.path().extension() == ".avi")
                aviFiles.push_back(entry.path());
        }
    }
    std::sort(aviFiles.begin(), aviFiles.end());

    for (const auto& aviPath : aviFiles) {
        // Sequence name: "V01_1" from "V01_1.avi"
        std::string name = aviPath.stem().string();

        fs::path gtPointsPath = annotationsDir / (name + "_gt_points.txt");
        fs::path annTypesPath = annotationsDir / (name + "_annotation_types.txt");

        if (!fs::exists(gtPointsPath) || !fs::exists(annTypesPath)) {
            // No annotations — skip this sequence
            continue;
        }

        std::vector<int> annTypes;
        {
            std::ifstream f(annTypesPath);
            int t;
            while (f >> t) annTypes.push_back(t);
        }

        // gt_points holds 8 numbers per line
        std::vector<std::array<double, 8>> gtPoints;
        {
            std::ifstream f(gtPointsPath);
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                std::istringstream ss(line);
                std::array<double, 8> pts{};
                for (int i = 0; i < 8; i++) ss >> pts[i];
                gtPoints.push_back(pts);
            }
        }

        if (annTypes.size() != gtPoints.size()) {
            throw std::runtime_error(
                "annotation_types and gt_points line count mismatch for: " + name);
        }

        // Build frameIndex → FrameGT, keeping only type > 0
        std::map<int, FrameGT> gt;
        for (int i = 0; i < static_cast<int>(annTypes.size()); i++) {
            if (annTypes[i] == 0) continue;
            FrameGT fgt;
            fgt.annotated = true;
            for (int k = 0; k < 8; k++) fgt.corners[k] = gtPoints[i][k];
            gt[i] = fgt;
        }

        auto source = std::make_unique<VideoFrameSource>(aviPath.string());
        int total = source->totalFrames();

        cv::Mat frame0;
        if (!source->next(frame0) || frame0.empty())
            throw std::runtime_error("Cannot read frame 0 from: " + aviPath.string());
        source->reset();

        // Cut the template out of frame 0 using its GT corners (TL, TR, BR, BL)
        if (!gt.count(0))
            throw std::runtime_error("No GT annotation for frame 0 in: " + name);

        const auto& c = gt.at(0).corners;
        std::vector<cv::Point2f> srcPts = {
            {(float)c[0], (float)c[1]},
            {(float)c[2], (float)c[3]},
            {(float)c[4], (float)c[5]},
            {(float)c[6], (float)c[7]},
        };

        // The template size is the object's bounding box
        float w = std::max({srcPts[0].x, srcPts[1].x, srcPts[2].x, srcPts[3].x}) -
                  std::min({srcPts[0].x, srcPts[1].x, srcPts[2].x, srcPts[3].x});
        float h = std::max({srcPts[0].y, srcPts[1].y, srcPts[2].y, srcPts[3].y}) -
                  std::min({srcPts[0].y, srcPts[1].y, srcPts[2].y, srcPts[3].y});

        std::vector<cv::Point2f> dstPts = {
            {0,    0   },
            {w,    0   },
            {w,    h   },
            {0,    h   },
        };

        cv::Mat H = cv::getPerspectiveTransform(srcPts, dstPts);
        cv::Mat templateImg;
        cv::warpPerspective(frame0, templateImg, H, cv::Size((int)w, (int)h));

        BenchmarkSequence seq;
        seq.name        = name;
        seq.frames      = std::move(source);
        seq.templateImg = templateImg;
        seq.gt          = std::move(gt);
        seq.totalFrames = total;

        sequences.push_back(std::move(seq));
    }

    return sequences;
}

// ============================================================================
// Adapter for an arbitrary video plus template (no annotations)
// ============================================================================

inline BenchmarkSequence loadFromVideoAndTemplate(const std::string& videoPath,
                                                   const std::string& templatePath,
                                                   const std::string& name = "")
{
    cv::Mat templateImg = cv::imread(templatePath, cv::IMREAD_GRAYSCALE);
    if (templateImg.empty())
        throw std::runtime_error("Cannot load template: " + templatePath);

    auto source = std::make_unique<VideoFrameSource>(videoPath);
    int total = source->totalFrames();

    // Fall back to the file name when no sequence name is given
    std::string seqName = name.empty() ? fs::path(videoPath).stem().string() : name;

    BenchmarkSequence seq;
    seq.name        = seqName;
    seq.frames      = std::move(source);
    seq.templateImg = templateImg;
    seq.gt          = {};        // no annotations
    seq.totalFrames = total;

    return seq;
}

// ============================================================================
// Adapter for our own DatasetConfig (PNG-based, test/framework)
// ============================================================================

inline std::vector<BenchmarkSequence> loadFromDatasetConfig(
    const morpheus::test::DatasetConfig& config)
{
    std::vector<BenchmarkSequence> sequences;

    for (int si = 0; si < static_cast<int>(config.sequences.size()); si++) {
        const auto& seqInfo = config.sequences[si];

        auto seqGT = config.loadSequenceGroundTruth(si);

        std::map<int, FrameGT> gt;
        for (const auto& [frameIdx, fgt] : seqGT.groundTruth) {
            if (!fgt.visible || !fgt.corners.has_value()) continue;
            FrameGT fg;
            fg.annotated = true;
            const auto& c = fgt.corners.value();
            for (int k = 0; k < 8; k++) fg.corners[k] = c[k];
            gt[frameIdx] = fg;
        }

        int tmplIdx = seqInfo.templateIndex.value_or(0);
        cv::Mat templateImg = cv::imread(config.getTemplatePath(tmplIdx), cv::IMREAD_GRAYSCALE);
        if (templateImg.empty())
            throw std::runtime_error("Cannot load template: " + config.getTemplatePath(tmplIdx));

        auto source = std::make_unique<DirFrameSource>(seqInfo.framesDir);
        int total = source->totalFrames();

        BenchmarkSequence seq;
        seq.name        = seqInfo.name;
        seq.frames      = std::move(source);
        seq.templateImg = templateImg;
        seq.gt          = std::move(gt);
        seq.totalFrames = total;

        sequences.push_back(std::move(seq));
    }

    return sequences;
}

} // namespace morpheus::benchmark
