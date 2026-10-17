#include "dataset_format.hpp"
#include <fstream>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

namespace morpheus::test {

// ============================================================================
// FrameGroundTruth
// ============================================================================

FrameGroundTruth FrameGroundTruth::fromJson(const json& j) {
    FrameGroundTruth gt;

    gt.frameIndex = j.value("frame_index", 0);
    gt.visible = j.value("visible", true);

    if (j.contains("corners")) {
        gt.corners = j["corners"].get<std::vector<double>>();
        if (gt.corners->size() != 8) {
            throw std::runtime_error("corners must have exactly 8 elements");
        }
    }

    if (j.contains("homography")) {
        gt.homography = j["homography"].get<std::vector<double>>();
        if (gt.homography->size() != 9) {
            throw std::runtime_error("homography must have exactly 9 elements");
        }
    }

    if (j.contains("occlusion")) {
        gt.occlusion = j["occlusion"].get<double>();
    }

    if (j.contains("blur")) {
        gt.blur = j["blur"].get<double>();
    }

    if (j.contains("noise")) {
        gt.noise = j["noise"].get<double>();
    }

    return gt;
}

json FrameGroundTruth::toJson() const {
    json j;
    j["frame_index"] = frameIndex;
    j["visible"] = visible;

    if (corners.has_value()) {
        j["corners"] = corners.value();
    }

    if (homography.has_value()) {
        j["homography"] = homography.value();
    }

    if (occlusion.has_value()) {
        j["occlusion"] = occlusion.value();
    }

    if (blur.has_value()) {
        j["blur"] = blur.value();
    }

    if (noise.has_value()) {
        j["noise"] = noise.value();
    }

    return j;
}

// ============================================================================
// SequenceGroundTruth
// ============================================================================

SequenceGroundTruth SequenceGroundTruth::loadFromFile(const std::string& gtJsonPath) {
    std::ifstream file(gtJsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open gt.json: " + gtJsonPath);
    }

    json j;
    file >> j;

    SequenceGroundTruth seqGT;

    if (j.contains("frames")) {
        seqGT.frames = j["frames"].get<std::vector<std::string>>();
    }

    if (j.contains("ground_truth")) {
        for (const auto& item : j["ground_truth"]) {
            FrameGroundTruth frameGT = FrameGroundTruth::fromJson(item);
            seqGT.groundTruth[frameGT.frameIndex] = frameGT;
        }
    }

    return seqGT;
}

void SequenceGroundTruth::saveToFile(const std::string& gtJsonPath) const {
    json j;

    j["frames"] = frames;

    json gtArray = json::array();
    for (const auto& [frameIdx, gt] : groundTruth) {
        gtArray.push_back(gt.toJson());
    }
    j["ground_truth"] = gtArray;

    std::ofstream file(gtJsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write gt.json: " + gtJsonPath);
    }

    file << j.dump(2);
}

// ============================================================================
// SequenceInfo
// ============================================================================

SequenceInfo SequenceInfo::fromJson(const json& j, const std::string& datasetBasePath) {
    SequenceInfo seq;

    seq.name = j["name"].get<std::string>();
    seq.type = stringToSequenceType(j["type"].get<std::string>());

    seq.framesDir = (fs::path(datasetBasePath) / j["frames_dir"].get<std::string>()).string();
    seq.gtPath = (fs::path(datasetBasePath) / j["gt_path"].get<std::string>()).string();

    if (j.contains("template_index")) {
        seq.templateIndex = j["template_index"].get<int>();
    }

    if (j.contains("metadata")) {
        seq.metadata = j["metadata"].get<std::map<std::string, std::string>>();
    }

    return seq;
}

json SequenceInfo::toJson() const {
    json j;
    j["name"] = name;
    j["type"] = sequenceTypeToString(type);
    j["frames_dir"] = framesDir;
    j["gt_path"] = gtPath;

    if (templateIndex.has_value()) {
        j["template_index"] = templateIndex.value();
    }

    if (!metadata.empty()) {
        j["metadata"] = metadata;
    }

    return j;
}

// ============================================================================
// TemplateInfo
// ============================================================================

TemplateInfo TemplateInfo::fromJson(const json& j, const std::string& datasetBasePath) {
    TemplateInfo tmpl;

    tmpl.name = j["name"].get<std::string>();
    tmpl.path = (fs::path(datasetBasePath) / j["path"].get<std::string>()).string();

    if (j.contains("metadata")) {
        tmpl.metadata = j["metadata"].get<std::map<std::string, std::string>>();
    }

    return tmpl;
}

json TemplateInfo::toJson() const {
    json j;
    j["name"] = name;
    j["path"] = path;

    if (!metadata.empty()) {
        j["metadata"] = metadata;
    }

    return j;
}

// ============================================================================
// DatasetConfig
// ============================================================================

DatasetConfig DatasetConfig::loadFromFile(const std::string& datasetJsonPath) {
    std::ifstream file(datasetJsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open dataset.json: " + datasetJsonPath);
    }

    json j;
    file >> j;

    DatasetConfig config;
    config.basePath = fs::path(datasetJsonPath).parent_path().string();

    config.name = j["name"].get<std::string>();

    if (j.contains("templates")) {
        for (const auto& tmplJson : j["templates"]) {
            config.templates.push_back(TemplateInfo::fromJson(tmplJson, config.basePath));
        }
    }

    if (j.contains("sequences")) {
        for (const auto& seqJson : j["sequences"]) {
            config.sequences.push_back(SequenceInfo::fromJson(seqJson, config.basePath));
        }
    }

    if (j.contains("tags")) {
        config.tags = j["tags"].get<std::map<std::string, std::string>>();
    }

    if (j.contains("fps")) {
        config.fps = j["fps"].get<int>();
    }

    if (j.contains("version")) {
        config.version = j["version"].get<std::string>();
    }

    if (j.contains("description")) {
        config.description = j["description"].get<std::string>();
    }

    return config;
}

void DatasetConfig::saveToFile(const std::string& datasetJsonPath) const {
    json j;

    j["name"] = name;

    json templatesArray = json::array();
    for (const auto& tmpl : templates) {
        templatesArray.push_back(tmpl.toJson());
    }
    j["templates"] = templatesArray;

    json sequencesArray = json::array();
    for (const auto& seq : sequences) {
        sequencesArray.push_back(seq.toJson());
    }
    j["sequences"] = sequencesArray;

    if (!tags.empty()) {
        j["tags"] = tags;
    }

    if (fps.has_value()) {
        j["fps"] = fps.value();
    }

    if (version.has_value()) {
        j["version"] = version.value();
    }

    if (description.has_value()) {
        j["description"] = description.value();
    }

    std::ofstream file(datasetJsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write dataset.json: " + datasetJsonPath);
    }

    file << j.dump(2);
}

std::string DatasetConfig::getTemplatePath(int index) const {
    if (index < 0 || index >= static_cast<int>(templates.size())) {
        throw std::out_of_range("Template index out of range");
    }
    return templates[index].path;
}

SequenceGroundTruth DatasetConfig::loadSequenceGroundTruth(int sequenceIndex) const {
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(sequences.size())) {
        throw std::out_of_range("Sequence index out of range");
    }
    return SequenceGroundTruth::loadFromFile(sequences[sequenceIndex].gtPath);
}

// ============================================================================
// DatasetManager
// ============================================================================

void DatasetManager::loadDatasetsFromDir(const std::string& datasetsDir) {
    if (!fs::exists(datasetsDir)) {
        throw std::runtime_error("Datasets directory does not exist: " + datasetsDir);
    }

    for (const auto& entry : fs::directory_iterator(datasetsDir)) {
        if (entry.is_directory()) {
            fs::path datasetJsonPath = entry.path() / "dataset.json";
            if (fs::exists(datasetJsonPath)) {
                try {
                    DatasetConfig config = DatasetConfig::loadFromFile(datasetJsonPath.string());
                    addDataset(config);
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to load dataset from "
                              << datasetJsonPath << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void DatasetManager::addDataset(const DatasetConfig& dataset) {
    datasets_[dataset.name] = dataset;
}

const DatasetConfig* DatasetManager::getDataset(const std::string& name) const {
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> DatasetManager::getDatasetNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : datasets_) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// GT Utils
// ============================================================================

namespace gt_utils {

std::vector<double> cornersToHomography(const std::vector<double>& corners,
                                       int templateWidth, int templateHeight) {
    if (corners.size() != 8) {
        throw std::invalid_argument("corners must have 8 elements");
    }

    // Source corners in template coordinates
    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(0, 0),
        cv::Point2f(templateWidth, 0),
        cv::Point2f(templateWidth, templateHeight),
        cv::Point2f(0, templateHeight)
    };

    // Destination corners in frame coordinates
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(corners[0], corners[1]),
        cv::Point2f(corners[2], corners[3]),
        cv::Point2f(corners[4], corners[5]),
        cv::Point2f(corners[6], corners[7])
    };

    cv::Mat H = cv::findHomography(srcPoints, dstPoints);

    std::vector<double> homography(9);
    for (int i = 0; i < 9; i++) {
        homography[i] = H.at<double>(i / 3, i % 3);
    }

    return homography;
}

std::vector<double> homographyToCorners(const std::vector<double>& homography,
                                       int templateWidth, int templateHeight) {
    if (homography.size() != 9) {
        throw std::invalid_argument("homography must have 9 elements");
    }

    cv::Mat H(3, 3, CV_64F);
    for (int i = 0; i < 9; i++) {
        H.at<double>(i / 3, i % 3) = homography[i];
    }

    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(0, 0),
        cv::Point2f(templateWidth, 0),
        cv::Point2f(templateWidth, templateHeight),
        cv::Point2f(0, templateHeight)
    };

    std::vector<cv::Point2f> dstPoints;
    cv::perspectiveTransform(srcPoints, dstPoints, H);

    std::vector<double> corners(8);
    for (int i = 0; i < 4; i++) {
        corners[2 * i] = dstPoints[i].x;
        corners[2 * i + 1] = dstPoints[i].y;
    }

    return corners;
}

bool isValid(const FrameGroundTruth& gt) {
    if (!gt.visible) {
        return true; // invisible is a valid state
    }

    // At least one of corners or homography must be present
    return gt.corners.has_value() || gt.homography.has_value();
}

double computeCornerError(const std::vector<double>& predicted,
                         const std::vector<double>& groundTruth) {
    if (predicted.size() != 8 || groundTruth.size() != 8) {
        throw std::invalid_argument("Both corner arrays must have 8 elements");
    }

    double totalError = 0.0;
    for (int i = 0; i < 4; i++) {
        double dx = predicted[2 * i] - groundTruth[2 * i];
        double dy = predicted[2 * i + 1] - groundTruth[2 * i + 1];
        totalError += std::sqrt(dx * dx + dy * dy);
    }

    return totalError / 4.0;
}

} // namespace gt_utils

} // namespace morpheus::test
