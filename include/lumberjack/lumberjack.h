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
// CONTRACT: All function pointers must be non-null. Backends that don't need
// certain functionality should provide no-op implementations.
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

// Built-in backend configuration
void builtin_set_output(FILE* file);

// Buffered write mode: logs are accumulated in a memory buffer and only
// flushed when the buffer is full, on explicit flush, or on shutdown.
// This eliminates per-call fflush() overhead.
void builtin_set_buffered(bool enabled, size_t buffer_size = 8192);

// Manually flush the built-in backend's buffer (only relevant in buffered mode)
void builtin_flush();

// Cached timestamp mode: instead of calling localtime/strftime per log line,
// the timestamp string is cached and only refreshed every `interval_ms`
// milliseconds. Set to 0 to disable caching (recompute every call).
void builtin_set_timestamp_cache_ms(unsigned int interval_ms);

// Function pointer types for branchless dispatch
using LogFunction = void (*)(LogLevel, const char*, ...);
using ClockFunction = std::chrono::steady_clock::time_point (*)();

// Global function pointer arrays (exposed for macro and Span use)
extern LogFunction g_logFunctions[LOG_COUNT];
extern ClockFunction g_clockFunctions[LOG_COUNT];

// RAII Span class for automatic timing measurement
// When the log level is inactive, both the clock reads and span callbacks
// are no-ops via function pointer dispatch — near zero overhead.
class Span {
public:
    Span(LogLevel level, const char* name);
    ~Span();
    
    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;
    Span(Span&&) = delete;
    Span& operator=(Span&&) = delete;
    
private:
    LogLevel m_level;
    const char* m_name;
    void* m_handle;
    std::chrono::steady_clock::time_point m_start;
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
#define LOG_SPAN(level, name) lumberjack::Span _log_span_##__LINE__(level, name)

#endif // LUMBERJACK_H