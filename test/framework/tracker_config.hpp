#pragma once

#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../src/image_tracking/common/tracker_base.hpp"

namespace morpheus::test {

using json = nlohmann::json;

// ============================================================================
// ORB detector parameters
// ============================================================================
struct ORBParams {
    int nFeatures = 2000;
    int nDescriptorFeatures = 3000;
    std::string descriptorType = "ORB"; // "ORB" or "BEBLID"
    float scaleFactor = 1.2f;
    int nLevels = 8;
    int edgeThreshold = 31;
    int firstLevel = 0;
    int WTA_K = 2;
    int patchSize = 31;
    int fastThreshold = 20;

    static ORBParams fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Optical flow parameters
// ============================================================================
struct OpticalFlowParams {
    int winSize = 15;
    int maxLevel = 5;
    int maxIterations = 30;
    double epsilon = 0.01;
    double minEigThreshold = 0.0001;

    static OpticalFlowParams fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Homography validation parameters
// ============================================================================
struct ValidationParams {
    double minDeterminant = 0.1;        // 1/10
    double maxDeterminant = 10.0;       // 10
    double maxConditionNumber = 100.0;
    double maxPerspectiveDistortion = 0.002;
    int minPoints = 4;

    static ValidationParams fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Tracking parameters
// ============================================================================
struct TrackingParams {
    int maxConsecutiveFailures = 3;
    float goodMatchRatio = 0.8f;
    int minMatchesForRecognition = 50;
    int topDescriptorMatches = 50;
    int retryIterations = 3;

    // Shi-Tomasi corners for optical flow instead of ORB points
    bool useShiTomasi = false;
    int shiTomasiMaxCorners = 500;
    double shiTomasiQuality = 0.01;
    double shiTomasiMinDist = 5.0;

    // Kalman filter
    bool useKalman = false;
    float kalmanProcessNoise = 1.0f;
    float kalmanMeasNoise = 10.0f;

    // ESM correction
    bool useESM = false;
    int esmMaxIter = 5;
    double esmEps = 0.05;
    int esmPointStep = 10;

    // RANSAC
    double ransacReprojThreshold = 3.0;

    // Forward-backward consistency check
    bool useFBCheck = false;
    float fbCheckThreshold = 1.0f;

    // OBD-style periodic correction built into ORB tracker
    bool useOBDCorrection = false;
    int obdCorrectionGridSize = 8;

    // PeriodicOBD
    int obdCorrectionInterval = 10;
    double obdCorrectionThreshold = 50.0;
    bool obdUpdateTrackingPts = false;

    static TrackingParams fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Tracker configuration
// ============================================================================
struct TrackerConfig {
    std::string name;           // "ORB_Default", "OBD_Fast", etc.
    std::string type;           // "ORB", "OBD"
    bool debug = false;

    // Unspecified parameters fall back to the defaults above
    ORBParams orbParams;
    OpticalFlowParams opticalFlowParams;
    ValidationParams validationParams;
    TrackingParams trackingParams;

    static TrackerConfig loadFromFile(const std::string& jsonPath);
    void saveToFile(const std::string& jsonPath) const;

    static TrackerConfig fromJson(const json& j);
    json toJson() const;
};

// ============================================================================
// Tracker factory
// ============================================================================
class TrackerFactory {
public:
    // NOTE: only CPU trackers (ORB, OBD) are returned.
    // The WebGL trackers (OBDMixed, OBDWebGL) are not supported in native tests.
    static std::unique_ptr<TrackerBase> createTracker(const TrackerConfig& config);

    // Applies the parameters that can be changed without recreating the tracker
    static void applyConfig(TrackerBase* tracker, const TrackerConfig& config);
};

// ============================================================================
// Manages a collection of tracker configs
// ============================================================================
class TrackerConfigManager {
public:
    void loadConfigsFromDir(const std::string& configsDir);

    void addConfig(const TrackerConfig& config);

    const TrackerConfig* getConfig(const std::string& name) const;

    std::vector<std::string> getConfigNames() const;

    size_t size() const { return configs_.size(); }

private:
    std::map<std::string, TrackerConfig> configs_;
};

} // namespace morpheus::test
