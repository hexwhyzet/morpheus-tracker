#include "tracker_config.hpp"

#include <fstream>
#include <stdexcept>

#include "src/image_tracking/OBD.hpp"
#include "src/image_tracking/ORB.hpp"
#include "src/image_tracking/ShiTomasi.hpp"

namespace morpheus::test {

// ============================================================================
// ORBParams
// ============================================================================

ORBParams ORBParams::fromJson(const json& j) {
    ORBParams p;
    p.nFeatures            = j.value("nFeatures", p.nFeatures);
    p.nDescriptorFeatures  = j.value("nDescriptorFeatures", p.nDescriptorFeatures);
    p.descriptorType       = j.value("descriptorType", p.descriptorType);
    p.scaleFactor          = j.value("scaleFactor", p.scaleFactor);
    p.nLevels              = j.value("nLevels", p.nLevels);
    p.edgeThreshold        = j.value("edgeThreshold", p.edgeThreshold);
    p.firstLevel           = j.value("firstLevel", p.firstLevel);
    p.WTA_K                = j.value("WTA_K", p.WTA_K);
    p.patchSize            = j.value("patchSize", p.patchSize);
    p.fastThreshold        = j.value("fastThreshold", p.fastThreshold);
    return p;
}

json ORBParams::toJson() const {
    return {
        {"nFeatures",           nFeatures},
        {"nDescriptorFeatures", nDescriptorFeatures},
        {"descriptorType",      descriptorType},
        {"scaleFactor",         scaleFactor},
        {"nLevels",             nLevels},
        {"edgeThreshold",       edgeThreshold},
        {"firstLevel",          firstLevel},
        {"WTA_K",               WTA_K},
        {"patchSize",           patchSize},
        {"fastThreshold",       fastThreshold},
    };
}

// ============================================================================
// OpticalFlowParams
// ============================================================================

OpticalFlowParams OpticalFlowParams::fromJson(const json& j) {
    OpticalFlowParams p;
    p.winSize         = j.value("winSize", p.winSize);
    p.maxLevel        = j.value("maxLevel", p.maxLevel);
    p.maxIterations   = j.value("maxIterations", p.maxIterations);
    p.epsilon         = j.value("epsilon", p.epsilon);
    p.minEigThreshold = j.value("minEigThreshold", p.minEigThreshold);
    return p;
}

json OpticalFlowParams::toJson() const {
    return {
        {"winSize",         winSize},
        {"maxLevel",        maxLevel},
        {"maxIterations",   maxIterations},
        {"epsilon",         epsilon},
        {"minEigThreshold", minEigThreshold},
    };
}

// ============================================================================
// ValidationParams
// ============================================================================

ValidationParams ValidationParams::fromJson(const json& j) {
    ValidationParams p;
    p.minDeterminant           = j.value("minDeterminant", p.minDeterminant);
    p.maxDeterminant           = j.value("maxDeterminant", p.maxDeterminant);
    p.maxConditionNumber       = j.value("maxConditionNumber", p.maxConditionNumber);
    p.maxPerspectiveDistortion = j.value("maxPerspectiveDistortion", p.maxPerspectiveDistortion);
    p.minPoints                = j.value("minPoints", p.minPoints);
    return p;
}

json ValidationParams::toJson() const {
    return {
        {"minDeterminant",           minDeterminant},
        {"maxDeterminant",           maxDeterminant},
        {"maxConditionNumber",       maxConditionNumber},
        {"maxPerspectiveDistortion", maxPerspectiveDistortion},
        {"minPoints",                minPoints},
    };
}

// ============================================================================
// TrackingParams
// ============================================================================

TrackingParams TrackingParams::fromJson(const json& j) {
    TrackingParams p;
    p.maxConsecutiveFailures  = j.value("maxConsecutiveFailures", p.maxConsecutiveFailures);
    p.goodMatchRatio          = j.value("goodMatchRatio", p.goodMatchRatio);
    p.minMatchesForRecognition = j.value("minMatchesForRecognition", p.minMatchesForRecognition);
    p.topDescriptorMatches    = j.value("topDescriptorMatches", p.topDescriptorMatches);
    p.retryIterations         = j.value("retryIterations", p.retryIterations);
    p.useShiTomasi            = j.value("useShiTomasi", p.useShiTomasi);
    p.shiTomasiMaxCorners     = j.value("shiTomasiMaxCorners", p.shiTomasiMaxCorners);
    p.shiTomasiQuality        = j.value("shiTomasiQuality", p.shiTomasiQuality);
    p.shiTomasiMinDist        = j.value("shiTomasiMinDist", p.shiTomasiMinDist);
    p.useKalman               = j.value("useKalman", p.useKalman);
    p.kalmanProcessNoise      = j.value("kalmanProcessNoise", p.kalmanProcessNoise);
    p.kalmanMeasNoise         = j.value("kalmanMeasNoise", p.kalmanMeasNoise);
    p.useESM                  = j.value("useESM", p.useESM);
    p.esmMaxIter              = j.value("esmMaxIter", p.esmMaxIter);
    p.esmEps                  = j.value("esmEps", p.esmEps);
    p.esmPointStep            = j.value("esmPointStep", p.esmPointStep);
    p.ransacReprojThreshold   = j.value("ransacReprojThreshold", p.ransacReprojThreshold);
    p.useFBCheck              = j.value("useFBCheck", p.useFBCheck);
    p.fbCheckThreshold        = j.value("fbCheckThreshold", p.fbCheckThreshold);
    p.useOBDCorrection        = j.value("useOBDCorrection", p.useOBDCorrection);
    p.obdCorrectionGridSize   = j.value("obdCorrectionGridSize", p.obdCorrectionGridSize);
    p.obdCorrectionInterval   = j.value("obdCorrectionInterval", p.obdCorrectionInterval);
    p.obdCorrectionThreshold  = j.value("obdCorrectionThreshold", p.obdCorrectionThreshold);
    p.obdUpdateTrackingPts    = j.value("obdUpdateTrackingPts", p.obdUpdateTrackingPts);
    return p;
}

json TrackingParams::toJson() const {
    return {
        {"maxConsecutiveFailures",   maxConsecutiveFailures},
        {"goodMatchRatio",           goodMatchRatio},
        {"minMatchesForRecognition", minMatchesForRecognition},
        {"topDescriptorMatches",     topDescriptorMatches},
        {"retryIterations",          retryIterations},
        {"useShiTomasi",             useShiTomasi},
        {"shiTomasiMaxCorners",      shiTomasiMaxCorners},
        {"shiTomasiQuality",         shiTomasiQuality},
        {"shiTomasiMinDist",         shiTomasiMinDist},
        {"useKalman",                useKalman},
        {"kalmanProcessNoise",       kalmanProcessNoise},
        {"kalmanMeasNoise",          kalmanMeasNoise},
        {"useESM",                   useESM},
        {"esmMaxIter",               esmMaxIter},
        {"esmEps",                   esmEps},
        {"esmPointStep",             esmPointStep},
        {"ransacReprojThreshold",    ransacReprojThreshold},
        {"useFBCheck",               useFBCheck},
        {"fbCheckThreshold",         fbCheckThreshold},
        {"useOBDCorrection",         useOBDCorrection},
        {"obdCorrectionInterval",    obdCorrectionInterval},
        {"obdCorrectionThreshold",   obdCorrectionThreshold},
        {"obdUpdateTrackingPts",     obdUpdateTrackingPts},
    };
}

// ============================================================================
// TrackerConfig
// ============================================================================

TrackerConfig TrackerConfig::fromJson(const json& j) {
    TrackerConfig c;
    c.name  = j.value("name", std::string{});
    c.type  = j.value("type", std::string{"ORB"});
    c.debug = j.value("debug", false);

    if (j.contains("orb_params"))        c.orbParams        = ORBParams::fromJson(j["orb_params"]);
    if (j.contains("optical_flow_params")) c.opticalFlowParams = OpticalFlowParams::fromJson(j["optical_flow_params"]);
    if (j.contains("validation_params")) c.validationParams = ValidationParams::fromJson(j["validation_params"]);
    if (j.contains("tracking_params"))   c.trackingParams   = TrackingParams::fromJson(j["tracking_params"]);

    return c;
}

json TrackerConfig::toJson() const {
    return {
        {"name",                 name},
        {"type",                 type},
        {"debug",                debug},
        {"orb_params",           orbParams.toJson()},
        {"optical_flow_params",  opticalFlowParams.toJson()},
        {"validation_params",    validationParams.toJson()},
        {"tracking_params",      trackingParams.toJson()},
    };
}

TrackerConfig TrackerConfig::loadFromFile(const std::string& jsonPath) {
    std::ifstream f(jsonPath);
    if (!f.is_open())
        throw std::runtime_error("Cannot open tracker config: " + jsonPath);
    json j;
    f >> j;
    return fromJson(j);
}

void TrackerConfig::saveToFile(const std::string& jsonPath) const {
    std::ofstream f(jsonPath);
    if (!f.is_open())
        throw std::runtime_error("Cannot write tracker config: " + jsonPath);
    f << toJson().dump(2);
}

// ============================================================================
// TrackerFactory
// ============================================================================

std::unique_ptr<TrackerBase> TrackerFactory::createTracker(const TrackerConfig& config) {
    ORBConfig orbConfig;
    orbConfig.nFeatures              = config.orbParams.nFeatures;
    orbConfig.nDescriptorFeatures    = config.orbParams.nDescriptorFeatures;
    orbConfig.descriptorType         = config.orbParams.descriptorType;
    orbConfig.opticalFlowWinSize     = config.opticalFlowParams.winSize;
    orbConfig.opticalFlowMaxLevel    = config.opticalFlowParams.maxLevel;
    orbConfig.opticalFlowIterations  = config.trackingParams.retryIterations;
    orbConfig.goodMatchRatio         = config.trackingParams.goodMatchRatio;
    orbConfig.minMatchesForRecognition = config.trackingParams.minMatchesForRecognition;
    orbConfig.minDeterminant         = config.validationParams.minDeterminant;
    orbConfig.maxDeterminant         = config.validationParams.maxDeterminant;
    orbConfig.maxConditionNumber     = config.validationParams.maxConditionNumber;
    orbConfig.maxPerspectiveDistortion = config.validationParams.maxPerspectiveDistortion;
    orbConfig.useShiTomasi           = config.trackingParams.useShiTomasi;
    orbConfig.shiTomasiMaxCorners    = config.trackingParams.shiTomasiMaxCorners;
    orbConfig.shiTomasiQuality       = config.trackingParams.shiTomasiQuality;
    orbConfig.shiTomasiMinDist       = config.trackingParams.shiTomasiMinDist;
    orbConfig.useKalman              = config.trackingParams.useKalman;
    orbConfig.kalmanProcessNoise     = config.trackingParams.kalmanProcessNoise;
    orbConfig.kalmanMeasNoise        = config.trackingParams.kalmanMeasNoise;
    orbConfig.useESM                 = config.trackingParams.useESM;
    orbConfig.esmMaxIter             = config.trackingParams.esmMaxIter;
    orbConfig.esmEps                 = config.trackingParams.esmEps;
    orbConfig.esmPointStep           = config.trackingParams.esmPointStep;
    orbConfig.ransacReprojThreshold  = config.trackingParams.ransacReprojThreshold;
    orbConfig.useFBCheck             = config.trackingParams.useFBCheck;
    orbConfig.fbCheckThreshold       = config.trackingParams.fbCheckThreshold;
    orbConfig.useOBDCorrection       = config.trackingParams.useOBDCorrection;
    orbConfig.obdCorrectionGridSize  = config.trackingParams.obdCorrectionGridSize;
    orbConfig.obdCorrectionInterval  = config.trackingParams.obdCorrectionInterval;
    orbConfig.obdCorrectionThreshold = config.trackingParams.obdCorrectionThreshold;
    orbConfig.obdUpdateTrackingPts   = config.trackingParams.obdUpdateTrackingPts;

    if (config.type == "OBD") {
        return std::make_unique<OBDTracker>(config.debug, orbConfig);
    }
    if (config.type == "ShiTomasi") {
        return std::make_unique<ShiTomasiTracker>(config.debug, orbConfig);
    }
    return std::make_unique<ORBTracker>(config.debug, orbConfig);
}

void TrackerFactory::applyConfig(TrackerBase* /*tracker*/, const TrackerConfig& /*config*/) {
    // All parameters are applied by the constructor at creation time.
}

// ============================================================================
// TrackerConfigManager
// ============================================================================

void TrackerConfigManager::loadConfigsFromDir(const std::string& configsDir) {
    namespace fs = std::filesystem;
    if (!fs::exists(configsDir))
        throw std::runtime_error("Configs directory not found: " + configsDir);

    for (const auto& entry : fs::directory_iterator(configsDir)) {
        if (entry.path().extension() == ".json") {
            try {
                TrackerConfig c = TrackerConfig::loadFromFile(entry.path().string());
                addConfig(c);
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to load config " << entry.path() << ": " << e.what() << "\n";
            }
        }
    }
}

void TrackerConfigManager::addConfig(const TrackerConfig& config) {
    configs_[config.name] = config;
}

const TrackerConfig* TrackerConfigManager::getConfig(const std::string& name) const {
    auto it = configs_.find(name);
    return it != configs_.end() ? &it->second : nullptr;
}

std::vector<std::string> TrackerConfigManager::getConfigNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : configs_)
        names.push_back(name);
    return names;
}

} // namespace morpheus::test
