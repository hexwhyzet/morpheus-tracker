#include <iostream>
#include <opencv2/imgcodecs.hpp>

#include "base64.hpp"
#include "json.hpp"

struct DebugImage {
    std::string name;
    std::vector<uint8_t> data;
    size_t width;
    size_t height;
    size_t channels;
};

struct DebugData {
    void addImage(const std::string &name, const cv::Mat &image) {
        DebugImage debugImage;
        debugImage.name = name;
        debugImage.width = image.cols;
        debugImage.height = image.rows;
        debugImage.channels = image.channels();
        debugImage.data.assign(image.data,
                               image.data + image.total() * image.elemSize());

        images_.push_back(std::move(debugImage));
    }

    template <typename T>
    void addImage(const std::string &name, const T *imageData,
                  const size_t width, const size_t height,
                  const size_t channels) {
        DebugImage debugImage;
        debugImage.name = name;
        debugImage.width = width;
        debugImage.height = height;
        debugImage.channels = channels;
        debugImage.data.assign(
            imageData, imageData + (width * height * channels) * sizeof(T));

        images_.push_back(std::move(debugImage));
    }

    void addInfo(const std::string &key, const std::string &value) {
        info_[key] = value;
    }

    std::string buildJson() const {
        nlohmann::json j;
        j["info"] = {};
        for (const auto &kv : info_) {
            j["info"][kv.first] = kv.second;
        }
        for (const auto &img : images_) {
            std::string base64str =
                websocketpp::base64_encode(img.data.data(), img.data.size());
            j["images"].push_back({{"name", img.name},
                                   {"width", img.width},
                                   {"height", img.height},
                                   {"channels", img.channels},
                                   {"base64", base64str}});
        }
        return j.dump();
    }

    void clear() {
        info_.clear();
        images_.clear();
    }

   private:
    std::map<std::string, std::string> info_;
    std::vector<DebugImage> images_;
};
