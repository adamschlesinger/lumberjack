#include "lumberjack/lumberjack.h"
#include <cstdio>
#include <mutex>
#include <vector>
#include <thread>
#include <string>

namespace lumberjack {

namespace {
    // ANSI color codes
    const char* COLOR_RESET = "\033[0m";
    const char* COLOR_RED = "\033[31m";      // ERROR
    const char* COLOR_YELLOW = "\033[33m";   // WARN
    const char* COLOR_GREEN = "\033[32m";    // INFO
    const char* COLOR_BLUE = "\033[34m";     // DEBUG
    
    // Global mutex for thread-safe output
    static std::mutex g_output_mutex;
}

// Helper function to convert log level to color code
static const char* level_to_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return COLOR_RED;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_INFO:  return COLOR_GREEN;
        case LOG_LEVEL_DEBUG: return COLOR_BLUE;
        default:              return COLOR_RESET;
    }
}

// Helper function to convert log level to string
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_NONE:  return "NONE";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "UNKNOWN";
    }
}

// Pretty backend implementation functions (stubs)
static void pretty_init() {
    // Stub: No initialization needed yet
}

static void pretty_shutdown() {
    // Stub: No cleanup needed yet
}

static void pretty_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_output_mutex);
    
    // Get color for level
    const char* color = level_to_color(level);
    const char* level_str = level_to_string(level);
    
    // Output: [LEVEL] message (with color)
    fprintf(stderr, "%s[%s] %s%s\n", 
            color, level_str, message, COLOR_RESET);
    fflush(stderr);
}

static void* pretty_span_begin(LogLevel level, const char* name) {
    // Stub: Span tracking will be added in task 4
    return nullptr;
}

static void pretty_span_end(void* handle, LogLevel level, 
                           const char* name, long long elapsed_us) {
    // Stub: Span tracking will be added in task 4
}

// Pretty backend accessor
LogBackend* pretty_backend() {
    static LogBackend backend = {
        "pretty",
        pretty_init,
        pretty_shutdown,
        pretty_log_write,
        pretty_span_begin,
        pretty_span_end
    };
    return &backend;
}

} // namespace lumberjack
