#pragma once

#include "logger.hpp"
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>

struct TimingData {
    std::vector<double> measurements;
    double total_time = 0.0;
    size_t count = 0;
    double min_time = std::numeric_limits<double>::max();
    double max_time = 0.0;

    void addMeasurement(double time_ms) {
        measurements.push_back(time_ms);
        total_time += time_ms;
        count++;
        min_time = std::min(min_time, time_ms);
        max_time = std::max(max_time, time_ms);

        // Cap the buffer to keep memory bounded over long sessions
        if (measurements.size() > 1000) {
            measurements.erase(measurements.begin(), measurements.begin() + 100);
        }
    }

    double getAverage() const {
        return count > 0 ? total_time / count : 0.0;
    }

    std::string getStats() const {
        if (count == 0) return "No measurements";

        std::ostringstream oss;
        oss << "avg:" << std::fixed << std::setprecision(2) << getAverage() << "ms"
            << " min:" << min_time << "ms"
            << " max:" << max_time << "ms"
            << " count:" << count;
        return oss.str();
    }
};

class PerformanceLogger {
private:
    static std::unordered_map<std::string, TimingData> timing_data_;
    static bool enabled_;

public:
    static void setEnabled(bool enabled) { enabled_ = enabled; }

    static void recordTiming(const std::string& name, double time_ms) {
        if (!enabled_) return;

        timing_data_[name].addMeasurement(time_ms);
        LOG_DEBUG("PERF", name, ": ", time_ms, "ms");
    }

    static std::string getReport() {
        std::ostringstream report;
        report << "\n=== Performance Report ===\n";

        for (const auto& [name, data] : timing_data_) {
            report << name << ": " << data.getStats() << "\n";
        }

        return report.str();
    }

    static void logReport() {
        LOG_INFO("PERF", getReport());
    }

    static void reset() {
        timing_data_.clear();
    }

    static const std::unordered_map<std::string, TimingData>& getData() {
        return timing_data_;
    }
};

inline std::unordered_map<std::string, TimingData> PerformanceLogger::timing_data_;
inline bool PerformanceLogger::enabled_ = true;

// RAII timer: records its lifetime on destruction
class ScopedTimer {
private:
    std::string name_;
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    bool enabled_;

public:
    explicit ScopedTimer(const std::string& name, bool enabled = true)
        : name_(name), enabled_(enabled) {
        if (enabled_) {
            start_time_ = std::chrono::steady_clock::now();
        }
    }

    ~ScopedTimer() {
        if (enabled_) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time_);
            double time_ms = static_cast<double>(duration.count()) / 1000.0;
            PerformanceLogger::recordTiming(name_, time_ms);
        }
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;
};

// Explicitly driven timer: each checkpoint() records the span since the previous one
class ManualTimer {
private:
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::string context_;
    bool enabled_;

public:
    explicit ManualTimer(const std::string& context = "", bool enabled = true)
        : context_(context), enabled_(enabled) {
        if (enabled_) {
            start_time_ = std::chrono::steady_clock::now();
        }
    }

    void checkpoint(const std::string& name) {
        if (!enabled_) return;

        auto current_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - start_time_);
        double time_ms = static_cast<double>(duration.count()) / 1000.0;

        std::string full_name = context_.empty() ? name : context_ + "::" + name;
        PerformanceLogger::recordTiming(full_name, time_ms);

        start_time_ = current_time;
    }

    double elapsed_ms() const {
        if (!enabled_) return 0.0;

        auto current_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - start_time_);
        return static_cast<double>(duration.count()) / 1000.0;
    }
};

#define SCOPED_TIMER(name) ScopedTimer _timer(name)
#define SCOPED_TIMER_ENABLED(name, enabled) ScopedTimer _timer(name, enabled)
#define MANUAL_TIMER(name) ManualTimer _manual_timer(name)
#define TIMER_CHECKPOINT(name) _manual_timer.checkpoint(name)