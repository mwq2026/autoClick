#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Fatal = 4
};

struct LogEntry {
    int64_t     timestampMs;   // milliseconds since epoch
    LogLevel    level;
    uint32_t    threadId;
    std::string source;        // class::function
    std::string message;
    std::string stackTrace;    // only for Error/Fatal
};

class Logger {
public:
    static Logger& Instance();

    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;

    void SetMaxEntries(int max);
    int  GetMaxEntries() const;

    void SetFileOutput(bool enabled, const std::string& path = "autoclicker.log");
    bool IsFileOutputEnabled() const;
    std::string GetFilePath() const;

    void Log(LogLevel level, const char* source, const char* fmt, ...);
    void LogWithStack(LogLevel level, const char* source, const char* message, const char* stack);

    void Clear();
    std::vector<LogEntry> GetEntries() const;
    std::vector<LogEntry> GetEntries(LogLevel minLevel) const;
    size_t EntryCount() const;

    static const char* LevelName(LogLevel level);
    static std::string FormatTimestamp(int64_t ms);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void WriteToFile(const LogEntry& entry);

    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    LogLevel level_{ LogLevel::Info };
    int maxEntries_{ 10000 };
    bool fileOutput_{ false };
    std::string filePath_{ "autoclicker.log" };
};

// Convenience macros
#define LOG_DEBUG(src, ...) Logger::Instance().Log(LogLevel::Debug, src, __VA_ARGS__)
#define LOG_INFO(src, ...)  Logger::Instance().Log(LogLevel::Info,  src, __VA_ARGS__)
#define LOG_WARN(src, ...)  Logger::Instance().Log(LogLevel::Warn,  src, __VA_ARGS__)
#define LOG_ERROR(src, ...) Logger::Instance().Log(LogLevel::Error, src, __VA_ARGS__)
#define LOG_FATAL(src, ...) Logger::Instance().Log(LogLevel::Fatal, src, __VA_ARGS__)
