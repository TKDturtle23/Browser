#include "Logger.h"
#include <cstdarg>
#include <cstdio>

void Logger::Init() {
    log_entries.clear();
    log_buffer.clear();
}

std::vector<LogEntry> Logger::log_entries;
std::vector<LogEntry> Logger::log_buffer;

// Helper that processes a finalized va_list bundle cleanly
void Logger::LogInternal_Va(LogLevel level, const std::string& format, const std::string& source, int Indent, va_list args) {
    // Make a copy of args to safely preview the required buffer size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format.c_str(), args_copy);
    va_end(args_copy);

    if (size <= 0) {
        log_entries.push_back({format, source, level});
        log_buffer.push_back({format, source, level});
        return;
    }

    // Allocate exact buffer size needed and format
    std::vector<char> buf(size + 1);
    std::vsnprintf(buf.data(), buf.size(), format.c_str(), args);

    std::string formatted_msg(buf.data(), size);
    log_entries.push_back({formatted_msg, source, level});
    log_buffer.push_back({formatted_msg, source, level});
}

// Public API mappings - va_start now safely targets 'source'
void Logger::Log_Verbose(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Verbose, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log_Info(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Info, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log_Warning(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Warning, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log_Error(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Error, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log_Fatal(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Fatal, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log_Debug(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::Debug, msg, source, Indent, args);
    va_end(args);
}

void Logger::Log(std::string msg, std::string source, int Indent, ...) {
    va_list args;
    va_start(args, Indent); // Fixed: target the last named argument
    LogInternal_Va(LogLevel::None, msg, source, Indent, args);
    va_end(args);
}

std::optional<LogEntry> Logger::GetNextLog() {
    if (log_buffer.empty()) {
        return std::nullopt;
    }
    LogEntry entry = std::move(log_buffer.front());
    log_buffer.erase(log_buffer.begin());
    return entry;
}
