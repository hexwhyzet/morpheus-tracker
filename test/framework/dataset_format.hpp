#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace morpheus::test {

using json = nlohmann::json;

// ============================================================================
// Ground truth for a single frame
// ============================================================================
struct FrameGroundTruth {
    int frameIndex;
    bool visible = true;

    // Either representation may be used:
    std::optional<std::vector<double>> corners;      // [x0,y0, x1,y1, x2,y2, x3,y3] (8 elements)
    std::optional<std::vector<double>> homography;   // [h00,h01,h02, h10,h11,h12, h20,h21,h22] (9 elements)

    // Optional attributes
    std::optional<double> occlusion;   // [0.0 - 1.0] how much is occluded
    std::optional<double> blur;        // [0.0 - 1.0] amount of blur
    std::optional<double> noise;       // [0.0 - 1.0] noise level

    static FrameGroundTruth fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Ground truth for a whole sequence (gt.json)
// ============================================================================
struct SequenceGroundTruth {
    std::vector<std::string> frames;                    // ["frame_0000.png", "frame_0001.png", ...]
    std::map<int, FrameGroundTruth> groundTruth;        // frameIndex -> GT

    static SequenceGroundTruth loadFromFile(const std::string& gtJsonPath);
    void saveToFile(const std::string& gtJsonPath) const;
};

// ============================================================================
// Sequence types
// ============================================================================
enum class SequenceType {
    TRACKING,    // Regular tracking
    REDETECT,    // Redetection after a loss
    NEGATIVE     // Negative samples (the object is absent)
};

inline std::string sequenceTypeToString(SequenceType type) {
    switch(type) {
        case SequenceType::TRACKING: return "tracking";
        case SequenceType::REDETECT: return "redetect";
        case SequenceType::NEGATIVE: return "negative";
        default: return "unknown";
    }
}

inline SequenceType stringToSequenceType(const std::string& str) {
    if (str == "tracking") return SequenceType::TRACKING;
    if (str == "redetect") return SequenceType::REDETECT;
    if (str == "negative") return SequenceType::NEGATIVE;
    throw std::runtime_error("Unknown sequence type: " + str);
}

// ============================================================================
// Description of a single sequence
// ============================================================================
struct SequenceInfo {
    std::string name;              // "seq_000"
    SequenceType type;             // tracking / redetect / negative
    std::string framesDir;         // "sequences/seq_000/frames"
    std::string gtPath;            // "sequences/seq_000/gt.json"

    // Optional metadata
    std::optional<int> templateIndex;      // Which template to use (index into templates[])
    std::map<std::string, std::string> metadata;

    static SequenceInfo fromJson(const json& j, const std::string& datasetBasePath);
    json toJson() const;
};

// ============================================================================
// Template description
// ============================================================================
struct TemplateInfo {
    std::string name;              // "t0"
    std::string path;              // "templates/t0.png"

    // Optional metadata
    std::map<std::string, std::string> metadata;

    static TemplateInfo fromJson(const json& j, const std::string& datasetBasePath);
    json toJson() const;
};

// ============================================================================
// Dataset configuration (dataset.json)
// ============================================================================
struct DatasetConfig {
    std::string name;
    std::string basePath;          // Path to the dataset directory

    std::vector<TemplateInfo> templates;
    std::vector<SequenceInfo> sequences;

    // Metadata
    std::map<std::string, std::string> tags;
    std::optional<int> fps;
    std::optional<std::string> version;
    std::optional<std::string> description;

    static DatasetConfig loadFromFile(const std::string& datasetJsonPath);
    void saveToFile(const std::string& datasetJsonPath) const;

    std::string getTemplatePath(int index) const;
    SequenceGroundTruth loadSequenceGroundTruth(int sequenceIndex) const;
};

// ============================================================================
// Manages a collection of datasets
// ============================================================================
class DatasetManager {
public:
    // Loads every dataset found in a datasets/ directory
    void loadDatasetsFromDir(const std::string& datasetsDir);

    void addDataset(const DatasetConfig& dataset);

    const DatasetConfig* getDataset(const std::string& name) const;

    std::vector<std::string> getDatasetNames() const;

    size_t size() const { return datasets_.size(); }

private:
    std::map<std::string, DatasetConfig> datasets_;
};

// ============================================================================
// Ground truth helpers
// ============================================================================
namespace gt_utils {
    std::vector<double> cornersToHomography(const std::vector<double>& corners,
                                           int templateWidth, int templateHeight);

    std::vector<double> homographyToCorners(const std::vector<double>& homography,
                                           int templateWidth, int templateHeight);

    bool isValid(const FrameGroundTruth& gt);

    // Mean distance between predicted and ground truth corners
    double computeCornerError(const std::vector<double>& predicted,
                            const std::vector<double>& groundTruth);
}

} // namespace morpheus::test
