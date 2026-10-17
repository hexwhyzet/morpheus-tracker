#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
private:
    static LogLevel current_level_;
    static bool enabled_;

public:
    static void setLevel(LogLevel level) { current_level_ = level; }
    static void setEnabled(bool enabled) { enabled_ = enabled; }

    template<typename... Args>
    static void log(LogLevel level, const std::string& tag, Args&&... args) {
        if (!enabled_ || level < current_level_) return;

        std::ostringstream oss;
        oss << "[" << levelToString(level) << "][" << tag << "] ";
        (oss << ... << args);

#ifdef EMSCRIPTEN
        emscripten_log(EM_LOG_CONSOLE, "%s", oss.str().c_str());
#else
        std::cout << oss.str() << std::endl;
#endif
    }

    template<typename... Args>
    static void debug(const std::string& tag, Args&&... args) {
        log(LogLevel::DEBUG, tag, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(const std::string& tag, Args&&... args) {
        log(LogLevel::INFO, tag, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(const std::string& tag, Args&&... args) {
        log(LogLevel::WARN, tag, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(const std::string& tag, Args&&... args) {
        log(LogLevel::ERROR, tag, std::forward<Args>(args)...);
    }


private:
    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

inline LogLevel Logger::current_level_ = LogLevel::INFO;
inline bool Logger::enabled_ = true;

#ifndef NDEBUG
#define LOG_DEBUG(tag, ...) Logger::debug(tag, __VA_ARGS__)
#define IF_DEBUG(code) code
#else
#define LOG_DEBUG(tag, ...) ((void)0)
#define IF_DEBUG(code) ((void)0)
#endif
#define LOG_INFO(tag, ...) Logger::info(tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) Logger::warn(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::error(tag, __VA_ARGS__)