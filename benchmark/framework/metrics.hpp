#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

namespace morpheus::benchmark {

// ============================================================================
// Per-frame result
// ============================================================================

struct FrameResult {
    int frameIndex;
    bool trackerSuccess;        // the tracker returned success=true
    bool hasGT;                 // ground truth exists for this frame
    double cornerError = -1.0;  // mean distance over the 4 corners (px), -1 without GT
};

// ============================================================================
// Per-sequence result
// ============================================================================

struct SequenceResult {
    std::string name;
    int totalFrames     = 0;
    int evaluatedFrames = 0;   // frames that have GT

    double successRate  = 0.0; // % of all frames where trackerSuccess=true
    double meanError    = -1.0; // mean cornerError over annotated frames with success=true
    double medianError  = -1.0;
    double pct75Error   = -1.0; // 75th percentile
    int lostFrames      = 0;   // frames with GT where success=false

    std::vector<FrameResult> frames;
};

// ============================================================================
// Corner error between prediction and GT
// ============================================================================

inline double computeCornerError(const double predicted[8], const double gt[8]) {
    double total = 0.0;
    for (int i = 0; i < 4; i++) {
        double dx = predicted[2*i]   - gt[2*i];
        double dy = predicted[2*i+1] - gt[2*i+1];
        total += std::sqrt(dx*dx + dy*dy);
    }
    return total / 4.0;
}

// ============================================================================
// Finalizes a SequenceResult from the accumulated FrameResults
// ============================================================================

inline void finalizeSequenceResult(SequenceResult& result) {
    int successCount = 0;
    std::vector<double> errors;

    for (const auto& f : result.frames) {
        if (f.trackerSuccess) successCount++;
        if (f.hasGT) {
            result.evaluatedFrames++;
            if (!f.trackerSuccess) {
                result.lostFrames++;
            } else if (f.cornerError >= 0.0) {
                errors.push_back(f.cornerError);
            }
        }
    }

    result.successRate = result.totalFrames > 0
        ? static_cast<double>(successCount) / result.totalFrames * 100.0
        : 0.0;

    if (!errors.empty()) {
        result.meanError = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();

        std::vector<double> sorted = errors;
        std::sort(sorted.begin(), sorted.end());

        result.medianError = sorted[sorted.size() / 2];
        result.pct75Error  = sorted[static_cast<size_t>(sorted.size() * 0.75)];
    }
}

// ============================================================================
// CSV output
// ============================================================================

inline void printCSVHeader(std::ostream& out) {
    out << "sequence,total_frames,evaluated_frames,success_rate_%,"
        << "mean_error_px,median_error_px,p75_error_px,lost_frames\n";
}

inline void printCSVRow(std::ostream& out, const SequenceResult& r) {
    auto fmt = [](double v) -> std::string {
        if (v < 0) return "N/A";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        return buf;
    };

    out << r.name << ","
        << r.totalFrames << ","
        << r.evaluatedFrames << ","
        << fmt(r.successRate) << ","
        << fmt(r.meanError) << ","
        << fmt(r.medianError) << ","
        << fmt(r.pct75Error) << ","
        << r.lostFrames << "\n";
}

inline void printCSVSummary(std::ostream& out, const std::vector<SequenceResult>& results) {
    // Summary row — averaged across all sequences
    int totalFrames = 0, totalEvaluated = 0, totalLost = 0;
    double sumSuccessRate = 0.0, sumMeanError = 0.0;
    int countWithError = 0;

    for (const auto& r : results) {
        totalFrames    += r.totalFrames;
        totalEvaluated += r.evaluatedFrames;
        totalLost      += r.lostFrames;
        sumSuccessRate += r.successRate;
        if (r.meanError >= 0.0) {
            sumMeanError += r.meanError;
            countWithError++;
        }
    }

    int n = static_cast<int>(results.size());
    SequenceResult summary;
    summary.name           = "TOTAL";
    summary.totalFrames    = totalFrames;
    summary.evaluatedFrames = totalEvaluated;
    summary.successRate    = n > 0 ? sumSuccessRate / n : 0.0;
    summary.meanError      = countWithError > 0 ? sumMeanError / countWithError : -1.0;
    summary.lostFrames     = totalLost;

    out << "\n";
    printCSVRow(out, summary);
}

} // namespace morpheus::benchmark
