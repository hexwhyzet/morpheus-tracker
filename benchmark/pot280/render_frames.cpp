#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "../../core/src/image_tracking/common/logger.hpp"
#include "../../core/src/image_tracking/common/tracker_base.hpp"
#include "../../test/framework/tracker_config.hpp"
#include "../framework/dataset_adapter.hpp"
#include "../framework/metrics.hpp"

using namespace morpheus::benchmark;
using namespace morpheus::test;

static void drawQuad(cv::Mat& img, const double pts[8], cv::Scalar color, int thickness = 2) {
    std::vector<cv::Point> quad = {
        {(int)pts[0], (int)pts[1]},
        {(int)pts[2], (int)pts[3]},
        {(int)pts[4], (int)pts[5]},
        {(int)pts[6], (int)pts[7]},
    };
    for (int i = 0; i < 4; i++)
        cv::line(img, quad[i], quad[(i+1)%4], color, thickness);
}

int main(int argc, char* argv[]) {
    Logger::setLevel(LogLevel::WARN);

    if (argc < 4) {
        std::cerr << "Usage:\n"
                  << "  POT280 mode:   " << argv[0] << " <dataset_dir> <sequence> <output> [max_frames=0] [tracker=ORB] [config.json]\n"
                  << "  Custom mode:   " << argv[0] << " --video <video.mp4> --template <tmpl.png> <output> [max_frames=0] [tracker=ORB] [config.json]\n"
                  << "  output: path ending in .mp4/.avi for video, or directory for JPGs\n";
        return 1;
    }

    std::string output;
    int maxFrames = 0;
    std::string trackerType = "ORB";
    std::string configPath;
    std::unique_ptr<BenchmarkSequence> seqOwned;
    BenchmarkSequence* seq = nullptr;

    if (std::string(argv[1]) == "--video") {
        // Custom mode: --video <video> --template <tmpl> <output> [max_frames] [tracker] [config]
        if (argc < 6) {
            std::cerr << "Custom mode requires: --video <video.mp4> --template <tmpl.png> <output>\n";
            return 1;
        }
        std::string videoPath    = argv[2];
        // argv[3] must be "--template"
        std::string templatePath = argv[4];
        output      = argv[5];
        maxFrames   = argc > 6 ? std::stoi(argv[6]) : 0;
        trackerType = argc > 7 ? argv[7] : "ORB";
        configPath  = argc > 8 ? argv[8] : "";

        seqOwned = std::make_unique<BenchmarkSequence>(
            loadFromVideoAndTemplate(videoPath, templatePath));
        seq = seqOwned.get();
    } else {
        // POT280 mode
        std::string datasetDir = argv[1];
        std::string seqName    = argv[2];
        output      = argv[3];
        maxFrames   = argc > 4 ? std::stoi(argv[4]) : 0;
        trackerType = argc > 5 ? argv[5] : "ORB";
        configPath  = argc > 6 ? argv[6] : "";

        static std::vector<BenchmarkSequence> sequences;
        sequences = loadFromPOT280(datasetDir);
        for (auto& s : sequences) {
            if (s.name == seqName) { seq = &s; break; }
        }
        if (!seq) {
            std::cerr << "Sequence not found: " << seqName << "\n";
            return 1;
        }
    }

    TrackerConfig config;
    if (!configPath.empty()) {
        config = TrackerConfig::loadFromFile(configPath);
        config.type = trackerType;
    } else {
        config.type  = trackerType;
        config.debug = false;
    }
    auto tracker = TrackerFactory::createTracker(config);

    tracker->initTemplate(seq->templateImg.data,
                          seq->templateImg.cols,
                          seq->templateImg.rows, true);

    bool videoMode = output.size() >= 4 &&
                     (output.substr(output.size()-4) == ".mp4" ||
                      output.substr(output.size()-4) == ".avi");

    seq->frames->reset();
    cv::Mat probe;
    seq->frames->next(probe);
    int W = probe.cols, H = probe.rows;
    seq->frames->reset();

    cv::VideoWriter writer;
    if (videoMode) {
        int fourcc = cv::VideoWriter::fourcc('m','p','4','v');
        writer.open(output, fourcc, 30.0, cv::Size(W * 4, H));
        if (!writer.isOpened()) {
            std::cerr << "Cannot open video writer: " << output << "\n";
            return 1;
        }
        std::cout << "Writing video: " << output << "\n";
    } else {
        namespace fs = std::filesystem;
        fs::create_directories(output);
        std::cout << "Writing frames to: " << output << "\n";
    }

    cv::Mat frame;
    int frameIdx = 0;
    int rendered = 0;

    while (seq->frames->next(frame)) {
        if (maxFrames > 0 && rendered >= maxFrames) break;

        if (frameIdx == 0) {
            frameIdx++;
            continue;
        }

        tracker->processFrame(frame.data, frame.cols, frame.rows, true);
        HomographyResult hr = tracker->getHomography();

        cv::Mat vis;
        cv::cvtColor(frame, vis, cv::COLOR_GRAY2BGR);

        // GT in green, only when the frame is annotated
        if (seq->gt.count(frameIdx)) {
            const FrameGT& gt = seq->gt.at(frameIdx);
            drawQuad(vis, gt.corners, {0, 255, 0}, 2);

            if (hr.success) {
                double error = computeCornerError(hr.corners, gt.corners);
                cv::putText(vis, "err=" + std::to_string((int)error) + "px",
                            {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,0,255}, 2);
            }
        }

        // Prediction in red
        if (hr.success) {
            drawQuad(vis, hr.corners, {0, 0, 255}, 2);
        } else {
            cv::putText(vis, "LOST", {10, 30}, cv::FONT_HERSHEY_SIMPLEX,
                        0.8, {0, 0, 255}, 2);
        }

        DebugPoints dp = tracker->getDebugPoints();

        // Panel 1: yellow only (trackingPts)
        cv::Mat panelYellow = vis.clone();
        for (const auto& pt : dp.trackingPts)
            cv::circle(panelYellow, pt, 2, {0, 255, 255}, -1);
        cv::putText(panelYellow, "tracking pts", {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, {0,255,255}, 2);

        // Panel 2: original with every layer drawn
        for (const auto& pt : dp.trackingPts)
            cv::circle(vis, pt, 2, {0, 255, 255}, -1);
        for (const auto& pt : dp.correctionPts)
            cv::circle(vis, pt, 2, {255, 0, 0}, -1);
        for (size_t i = 0; i < dp.matchSrc.size() && i < dp.matchDst.size(); i++) {
            cv::circle(vis, dp.matchSrc[i], 3, {0, 255, 0}, -1);
            cv::circle(vis, dp.matchDst[i], 3, {0, 0, 255}, -1);
            cv::line(vis, dp.matchSrc[i], dp.matchDst[i], {0, 200, 200}, 1);
        }
        cv::putText(vis, seq->name + " frame " + std::to_string(frameIdx),
                    {10, 60}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255,255,255}, 1);
        cv::putText(vis, "tracker: " + trackerType,
                    {10, H - 30}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0,255,255}, 2);

        // Panel 3: blue only (correctionPts)
        cv::Mat panelBlue = vis.clone();
        cv::Mat panelBlueClean;
        cv::cvtColor(frame, panelBlueClean, cv::COLOR_GRAY2BGR);
        if (hr.success) drawQuad(panelBlueClean, hr.corners, {0, 0, 255}, 2);
        for (const auto& pt : dp.correctionPts)
            cv::circle(panelBlueClean, pt, 2, {255, 0, 0}, -1);
        cv::putText(panelBlueClean, "correction pts", {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, {255,0,0}, 2);

        // Panel 4: debug image (warped frame with optical flow points)
        cv::Mat panelDebug;
        if (!dp.debugImage.empty() && dp.debugImage.size() == cv::Size(W, H)) {
            panelDebug = dp.debugImage;
        } else {
            panelDebug = cv::Mat::zeros(H, W, CV_8UC3);
            if (!dp.debugImage.empty()) {
                cv::Mat resized;
                cv::resize(dp.debugImage, resized, cv::Size(W, H));
                panelDebug = resized;
            }
            cv::putText(panelDebug, "no debug img", {10, H/2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, {100,100,100}, 1);
        }
        cv::putText(panelDebug, "warped+OF", {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, {255,255,255}, 2);

        // Stitch the 4 panels side by side
        cv::Mat combined(H, W * 4, CV_8UC3);
        panelYellow   .copyTo(combined(cv::Rect(0,   0, W, H)));
        vis           .copyTo(combined(cv::Rect(W,   0, W, H)));
        panelBlueClean.copyTo(combined(cv::Rect(W*2, 0, W, H)));
        panelDebug    .copyTo(combined(cv::Rect(W*3, 0, W, H)));

        if (videoMode) {
            writer.write(combined);
        } else {
            std::string outPath = output + "/" + seq->name + "_frame" +
                                  std::to_string(frameIdx) + ".jpg";
            cv::imwrite(outPath, combined);
        }

        rendered++;
        frameIdx++;
    }

    if (videoMode) writer.release();

    std::cout << "Done. " << rendered << " frames rendered.\n";
    return 0;
}
