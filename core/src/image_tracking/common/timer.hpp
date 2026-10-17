#pragma once

#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include "performance_logger.hpp"

struct Timer {
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    bool enable;

   public:
    explicit Timer(bool enable = true)
        : start_time(std::chrono::steady_clock::now()), enable(enable) {}

    void create_timestamp(std::string &&name) {
        if (!enable) {
            return;
        }

        auto new_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            new_time.time_since_epoch() - start_time.time_since_epoch());
        swap(start_time, new_time);
        PerformanceLogger::recordTiming(name, static_cast<double>(duration.count()) / 1000);
    }
};

