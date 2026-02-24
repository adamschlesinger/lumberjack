#ifndef LUMBERJACK_H
#define LUMBERJACK_H

#include <cstdio>
#include <chrono>

namespace lumberjack {

// Log levels in ascending verbosity order
enum LogLevel {
    LOG_LEVEL_NONE = 0,   // No logging
    LOG_LEVEL_ERROR = 1,  // Critical errors only
    LOG_LEVEL_WARN = 2,   // Warnings and errors
    LOG_LEVEL_INFO = 3,   // Informational messages
    LOG_LEVEL_DEBUG = 4,  // Verbose debug output
    LOG_COUNT = 5         // Total count for array sizing
};

// Backend interface for pluggable logging destinations
struct LogBackend {
    const char* name;
    void (*init)();
    void (*shutdown)();
    void (*log_write)(LogLevel level, const char* message);
    void* (*span_begin)(LogLevel level, const char* name);
    void (*span_end)(void* handle, LogLevel level, const char* name, long long elapsed_us);
};

// Core API functions
void init();
void set_level(LogLevel level);
LogLevel get_level();
void set_backend(LogBackend* backend);
LogBackend* get_backend();

// Backend accessors
LogBackend* builtin_backend();

// Built-in backend configuration (for testing)
void builtin_set_output(FILE* file);

// Function pointer type for branchless dispatch
using LogFunction = void (*)(LogLevel, const char*, ...);

// Global function pointer array (exposed for macro use)
extern LogFunction g_logFunctions[LOG_COUNT];

// RAII Span class for automatic timing measurement
class Span {
public:
    Span(LogLevel level, const char* name);
    ~Span();
    
    // Delete copy/move constructors and assignment operators
    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;
    Span(Span&&) = delete;
    Span& operator=(Span&&) = delete;
    
private:
    LogLevel m_level;
    const char* m_name;
    void* m_handle;
    std::chrono::steady_clock::time_point m_start;
    bool m_active;
};

} // namespace lumberjack

// Logging macros (global namespace for convenience)
#define LOG_ERROR(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_ERROR](lumberjack::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_WARN](lumberjack::LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_INFO](lumberjack::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_DEBUG](lumberjack::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#endif // LUMBERJACK_H
