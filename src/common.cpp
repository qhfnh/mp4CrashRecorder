/*
 * MP4 Crash-Safe Recorder - Common Implementation
 * 
 * Common types and utilities implementation
 * 
 * License: GPL v2+
 */

#include "common.h"

namespace mp4_recorder {

namespace {
LogSeverity g_min_severity = LogSeverity::LS_INFO;
std::string g_log_file;
bool g_enable_file_logging = false;
}

void LogSettings::SetMinSeverity(LogSeverity severity) {
    g_min_severity = severity;
}

LogSeverity LogSettings::GetMinSeverity() {
    return g_min_severity;
}

void LogSettings::EnableFileLogging(const std::string& filename) {
    g_log_file = filename;
    g_enable_file_logging = true;
    writeToFile("=== MP4 Crash-Safe Recorder Log ===");
    writeToFile("Started at: " + getCurrentTime());
}

void LogSettings::DisableFileLogging() {
    g_enable_file_logging = false;
}

bool LogSettings::ShouldLog(LogSeverity severity) {
    return severity >= g_min_severity && g_min_severity != LogSeverity::LS_NONE;
}

void LogSettings::Log(LogSeverity severity, const std::string& msg,
                      const char* file, int line) {
    if (!ShouldLog(severity)) {
        return;
    }

    std::string formatted = std::string("[") + severityToString(severity) + "] ";
    if (file && line > 0) {
        formatted += std::string(file) + ":" + std::to_string(line) + " ";
    }
    formatted += msg;

    if (severity == LogSeverity::LS_ERROR) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
    writeToFile(formatted);
}

std::string LogSettings::getCurrentTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);

    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void LogSettings::writeToFile(const std::string& msg) {
    if (!g_enable_file_logging || g_log_file.empty()) {
        return;
    }

    try {
        std::ofstream file(g_log_file, std::ios::app);
        if (file.is_open()) {
            file << getCurrentTime() << " " << msg << std::endl;
            file.close();
        }
    } catch (...) {
        // Silently ignore file write errors
    }
}

const char* LogSettings::severityToString(LogSeverity severity) {
    switch (severity) {
        case LogSeverity::LS_ERROR:
            return "ERROR";
        case LogSeverity::LS_WARNING:
            return "WARNING";
        case LogSeverity::LS_INFO:
            return "INFO";
        case LogSeverity::LS_VERBOSE:
            return "DEBUG";
        case LogSeverity::LS_NONE:
            return "NONE";
        default:
            return "INFO";
    }
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : file_(file), line_(line), severity_(severity), enabled_(LogSettings::ShouldLog(severity)) {
}

LogMessage::~LogMessage() {
    if (enabled_) {
        LogSettings::Log(severity_, stream_.str(), file_, line_);
    }
}

} // namespace mp4_recorder
