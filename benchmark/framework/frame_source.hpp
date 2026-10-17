#pragma once

#include <filesystem>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace morpheus::benchmark {

namespace fs = std::filesystem;

// Frame source abstraction — either a video or a directory of PNGs
class FrameSource {
public:
    virtual bool next(cv::Mat& frame) = 0;
    virtual int totalFrames() const = 0;
    virtual void reset() = 0;
    virtual ~FrameSource() = default;
};

// Reads frames from .avi/.mp4 through VideoCapture
class VideoFrameSource : public FrameSource {
public:
    explicit VideoFrameSource(const std::string& path) : path_(path) {
        open();
    }

    bool next(cv::Mat& frame) override {
        cv::Mat raw;
        if (!cap_.read(raw)) return false;
        if (raw.channels() == 3)
            cv::cvtColor(raw, frame, cv::COLOR_BGR2GRAY);
        else
            frame = raw;
        return true;
    }

    int totalFrames() const override {
        return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    }

    void reset() override {
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
    }

private:
    void open() {
        cap_.open(path_);
        if (!cap_.isOpened())
            throw std::runtime_error("Cannot open video: " + path_);
    }

    std::string path_;
    cv::VideoCapture cap_;
};

// Reads frames from a directory of PNG files (frame_0000.png, frame_0001.png, ...)
class DirFrameSource : public FrameSource {
public:
    explicit DirFrameSource(const std::string& dir) {
        if (!fs::exists(dir))
            throw std::runtime_error("Frames directory not found: " + dir);

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".png")
                paths_.push_back(entry.path().string());
        }
        std::sort(paths_.begin(), paths_.end());
    }

    bool next(cv::Mat& frame) override {
        if (idx_ >= static_cast<int>(paths_.size())) return false;
        frame = cv::imread(paths_[idx_++], cv::IMREAD_GRAYSCALE);
        return !frame.empty();
    }

    int totalFrames() const override {
        return static_cast<int>(paths_.size());
    }

    void reset() override {
        idx_ = 0;
    }

private:
    std::vector<std::string> paths_;
    int idx_ = 0;
};

} // namespace morpheus::benchmark
