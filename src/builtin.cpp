#include "lumberjack/lumberjack.h"
#include <cstdio>
#include <ctime>
#include <chrono>
#include <mutex>

namespace lumberjack {

// Global state for built-in backend
static FILE* g_output = stderr;
static std::mutex g_mutex;

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

// Built-in backend implementation functions
static void builtin_init() {
    // No initialization needed for built-in backend
}

static void builtin_shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Reset to stderr on shutdown
    g_output = stderr;
}

static void builtin_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Format timestamp: [YYYY-MM-DD HH:MM:SS.mmm]
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                  std::localtime(&time_t_now));
    
    // Get level string
    const char* level_str = level_to_string(level);
    
    // Write formatted message: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
    fprintf(g_output, "[%s.%03lld] [%s] %s\n", 
            timestamp, static_cast<long long>(ms.count()), level_str, message);
    fflush(g_output);
}

static void* builtin_span_begin(LogLevel level, const char* name) {
    // For built-in backend, we don't need to track spans
    // The Span class handles timing
    return nullptr;
}

static void builtin_span_end(void* handle, LogLevel level, 
                             const char* name, long long elapsed_us) {
    char message[256];
    snprintf(message, sizeof(message), "SPAN '%s' took %lld μs", 
             name, elapsed_us);
    builtin_log_write(level, message);
}

} // namespace lumberjack
