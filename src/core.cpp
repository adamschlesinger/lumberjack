#include "lumberjack/lumberjack.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace lumberjack {

// Forward declarations
static void log_noop(LogLevel level, const char* fmt, ...);
static void log_dispatch(LogLevel level, const char* fmt, ...);
static const char* level_to_string(LogLevel level);

// Global state
LogFunction g_logFunctions[LOG_COUNT] = {
    log_noop,  // LOG_LEVEL_NONE
    log_noop,  // LOG_LEVEL_ERROR
    log_noop,  // LOG_LEVEL_WARN
    log_noop,  // LOG_LEVEL_INFO
    log_noop   // LOG_LEVEL_DEBUG
};

static LogLevel g_currentLevel = LOG_LEVEL_INFO;
static LogBackend* g_activeBackend = nullptr;

// No-op function for disabled log levels
static void log_noop(LogLevel level, const char* fmt, ...) {
    // Empty, immediate return for performance
}

// Active dispatch function that formats and sends to backend
static void log_dispatch(LogLevel level, const char* fmt, ...) {
    if (!g_activeBackend || !g_activeBackend->log_write) {
        return;
    }
    
    // Format the message using vsnprintf
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Call backend with formatted message
    g_activeBackend->log_write(level, buffer);
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

// Public API implementations (stubs for now, will be implemented in later tasks)
void init() {
    set_level(LOG_LEVEL_INFO);
    set_backend(builtin_backend());
}

void set_level(LogLevel level) {
    g_currentLevel = level;
    
    // Update function pointer array based on level threshold
    // For levels <= active level: point to log_dispatch
    // For levels > active level: point to log_noop
    for (int i = 0; i < LOG_COUNT; i++) {
        if (i <= level) {
            g_logFunctions[i] = log_dispatch;
        } else {
            g_logFunctions[i] = log_noop;
        }
    }
}

LogLevel get_level() {
    return g_currentLevel;
}

void set_backend(LogBackend* backend) {
    // Call shutdown on old backend if it exists
    if (g_activeBackend && g_activeBackend->shutdown) {
        g_activeBackend->shutdown();
    }

    // Update pointer to new backend
    g_activeBackend = backend;

    // Call init on new backend if it exists
    if (g_activeBackend && g_activeBackend->init) {
        g_activeBackend->init();
    }
}

LogBackend* get_backend() {
    return g_activeBackend;
}

// Span class implementation
Span::Span(LogLevel level, const char* name)
    : m_level(level)
    , m_name(name)
    , m_handle(nullptr)
    , m_active(false)
{
    // Check if this level is active
    if (level <= get_level()) {
        m_active = true;
        m_start = std::chrono::steady_clock::now();
        
        // Call backend span_begin if available
        if (g_activeBackend && g_activeBackend->span_begin) {
            m_handle = g_activeBackend->span_begin(level, name);
        }
    }
}

Span::~Span() {
    if (m_active) {
        // Calculate elapsed time in microseconds
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_start).count();
        
        // Call backend span_end if available
        if (g_activeBackend && g_activeBackend->span_end) {
            g_activeBackend->span_end(m_handle, m_level, m_name, elapsed);
        }
    }
}

} // namespace lumberjack
