/*
 * MP4 Crash-Safe Recorder - Common Definitions
 * 
 * Common types and utilities
 * 
 * License: GPL v2+
 */

#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

namespace mp4_recorder {

// Logging levels (compatibility)
enum class LogLevel {
    SILENT = 0,
    ERROR = 1,
    INFO = 2,
    DEBUG = 3
};

// WebRTC-style log severity
enum class LogSeverity {
    LS_VERBOSE = 0,
    LS_INFO = 1,
    LS_WARNING = 2,
    LS_ERROR = 3,
    LS_NONE = 4
};

class LogSettings {
public:
    static void SetMinSeverity(LogSeverity severity);
    static LogSeverity GetMinSeverity();
    static void EnableFileLogging(const std::string& filename = "mp4_recorder.log");
    static void DisableFileLogging();
    static bool ShouldLog(LogSeverity severity);
    static void Log(LogSeverity severity, const std::string& msg,
                    const char* file, int line);

private:
    static std::string getCurrentTime();
    static void writeToFile(const std::string& msg);
    static const char* severityToString(LogSeverity severity);
};

class LogMessage {
public:
    LogMessage(const char* file, int line, LogSeverity severity);
    ~LogMessage();

    template <typename T>
    LogMessage& operator<<(const T& value) {
        if (enabled_) {
            stream_ << value;
        }
        return *this;
    }

private:
    const char* file_;
    int line_;
    LogSeverity severity_;
    bool enabled_;
    std::ostringstream stream_;
};

inline void SetLogSeverity(LogSeverity severity) {
    LogSettings::SetMinSeverity(severity);
}

inline void SetLogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::SILENT:
            SetLogSeverity(LogSeverity::LS_NONE);
            break;
        case LogLevel::ERROR:
            SetLogSeverity(LogSeverity::LS_ERROR);
            break;
        case LogLevel::INFO:
            SetLogSeverity(LogSeverity::LS_INFO);
            break;
        case LogLevel::DEBUG:
            SetLogSeverity(LogSeverity::LS_VERBOSE);
            break;
        default:
            SetLogSeverity(LogSeverity::LS_INFO);
            break;
    }
}

inline void EnableFileLogging(const std::string& filename = "mp4_recorder.log") {
    LogSettings::EnableFileLogging(filename);
}

inline void DisableFileLogging() {
    LogSettings::DisableFileLogging();
}

#define MCSR_LOG(severity) ::mp4_recorder::LogMessage(__FILE__, __LINE__, ::mp4_recorder::LogSeverity::LS_##severity)

// Utility functions
inline uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline uint64_t readBE64(const uint8_t* p) {
    return (uint64_t(readBE32(p)) << 32) | uint64_t(readBE32(p + 4));
}

inline void writeBE32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

inline void writeBE64(uint8_t* p, uint64_t v) {
    writeBE32(p, (uint32_t)(v >> 32));
    writeBE32(p + 4, (uint32_t)v);
}

} // namespace mp4_recorder

#endif // COMMON_H
