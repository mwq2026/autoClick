#include "core/Logger.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <windows.h>

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() = default;
Logger::~Logger() = default;

void Logger::SetLevel(LogLevel level) {
    std::scoped_lock lock(mutex_);
    level_ = level;
}

LogLevel Logger::GetLevel() const {
    std::scoped_lock lock(mutex_);
    return level_;
}

void Logger::SetMaxEntries(int max) {
    std::scoped_lock lock(mutex_);
    maxEntries_ = max;
    if (maxEntries_ > 0 && (int)entries_.size() > maxEntries_) {
        entries_.erase(entries_.begin(), entries_.begin() + ((int)entries_.size() - maxEntries_));
    }
}

int Logger::GetMaxEntries() const {
    std::scoped_lock lock(mutex_);
    return maxEntries_;
}

void Logger::SetFileOutput(bool enabled, const std::string& path) {
    std::scoped_lock lock(mutex_);
    fileOutput_ = enabled;
    if (!path.empty()) filePath_ = path;
}

bool Logger::IsFileOutputEnabled() const {
    std::scoped_lock lock(mutex_);
    return fileOutput_;
}

std::string Logger::GetFilePath() const {
    std::scoped_lock lock(mutex_);
    return filePath_;
}

void Logger::Log(LogLevel level, const char* source, const char* fmt, ...) {
    {
        std::scoped_lock lock(mutex_);
        if (level < level_) return;
    }

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    LogEntry entry;
    auto now = std::chrono::system_clock::now();
    entry.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    entry.level = level;
    entry.threadId = GetCurrentThreadId();
    entry.source = source ? source : "";
    entry.message = buf;

    std::scoped_lock lock(mutex_);
    entries_.push_back(entry);
    if (maxEntries_ > 0 && (int)entries_.size() > maxEntries_) {
        entries_.erase(entries_.begin());
    }
    if (fileOutput_) WriteToFile(entry);
}

void Logger::LogWithStack(LogLevel level, const char* source, const char* message, const char* stack) {
    {
        std::scoped_lock lock(mutex_);
        if (level < level_) return;
    }

    LogEntry entry;
    auto now = std::chrono::system_clock::now();
    entry.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    entry.level = level;
    entry.threadId = GetCurrentThreadId();
    entry.source = source ? source : "";
    entry.message = message ? message : "";
    entry.stackTrace = stack ? stack : "";

    std::scoped_lock lock(mutex_);
    entries_.push_back(entry);
    if (maxEntries_ > 0 && (int)entries_.size() > maxEntries_) {
        entries_.erase(entries_.begin());
    }
    if (fileOutput_) WriteToFile(entry);
}

void Logger::Clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

std::vector<LogEntry> Logger::GetEntries() const {
    std::scoped_lock lock(mutex_);
    return entries_;
}

std::vector<LogEntry> Logger::GetEntries(LogLevel minLevel) const {
    std::scoped_lock lock(mutex_);
    std::vector<LogEntry> result;
    for (const auto& e : entries_) {
        if (e.level >= minLevel) result.push_back(e);
    }
    return result;
}

size_t Logger::EntryCount() const {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

const char* Logger::LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    default: return "?";
    }
}

std::string Logger::FormatTimestamp(int64_t ms) {
    const time_t sec = static_cast<time_t>(ms / 1000);
    const int millis = static_cast<int>(ms % 1000);
    struct tm t{};
    localtime_s(&t, &sec);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec, millis);
    return buf;
}

void Logger::WriteToFile(const LogEntry& entry) {
    std::ofstream out(filePath_, std::ios::app);
    if (!out) return;
    out << FormatTimestamp(entry.timestampMs)
        << " [" << LevelName(entry.level) << "]"
        << " [T:" << entry.threadId << "]"
        << " [" << entry.source << "] "
        << entry.message;
    if (!entry.stackTrace.empty()) {
        out << "\n  Stack: " << entry.stackTrace;
    }
    out << "\n";
}
