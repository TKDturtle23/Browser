#ifndef BROWSER_LOGGER_H
#define BROWSER_LOGGER_H
#include <optional>
#include <string>
#include <vector>

enum class LogLevel {
    Verbose,
    Info,
    Warning,
    Error,
    Fatal,
    Debug,
    None
};

struct LogEntry {
    std::string msg;
    std::string source;
    LogLevel level;
    int Indent;
};

class Logger {
public:
    static void Init();

    static void Log_Verbose(std::string msg, std::string source, int Indent, ...);
    static void Log_Info(std::string msg, std::string source, int Indent, ...);
    static void Log_Warning(std::string msg, std::string source, int Indent, ...);
    static void Log_Error(std::string msg, std::string source, int Indent, ...);
    static void Log_Fatal(std::string msg, std::string source, int Indent, ...);
    static void Log_Debug(std::string msg, std::string source, int Indent, ...);
    static void Log(std::string msg, std::string source, int Indent, ...); // without a log level
    static std::optional<LogEntry> GetNextLog();

private:
    static std::vector<LogEntry> log_entries;
    static std::vector<LogEntry> log_buffer;

    // Cleaned up core processing engine
    static void LogInternal_Va(LogLevel level, const std::string& format, const std::string& source, int Indent, va_list args);
};

#endif //BROWSER_LOGGER_H