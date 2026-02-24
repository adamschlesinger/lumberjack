#include "lumberjack/lumberjack.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace lumberjack {

// Forward declarations
static void log_noop(LogLevel level, const char* fmt, ...);
static void log_dispatch(LogLevel level, const char* fmt, ...);
static const char* level_to_string(LogLevel level);

// No-op backend functions for safe static initialization
static void backend_noop_init() {}
static void backend_noop_shutdown() {}
static void backend_noop_log_write(LogLevel level, const char* message) {}
static void* backend_noop_span_begin(LogLevel level, const char* name) { return nullptr; }
static void backend_noop_span_end(void* handle, LogLevel level, const char* name, long long elapsed_us) {}

// Global state
LogFunction g_logFunctions[LOG_COUNT] = {
    log_noop,  // LOG_LEVEL_NONE
    log_noop,  // LOG_LEVEL_ERROR
    log_noop,  // LOG_LEVEL_WARN
    log_noop,  // LOG_LEVEL_INFO
    log_noop   // LOG_LEVEL_DEBUG
};

static LogLevel g_currentLevel = LOG_LEVEL_INFO;

// Safe no-op backend for static initialization
static LogBackend g_activeBackend = {
    "noop",
    backend_noop_init,
    backend_noop_shutdown,
    backend_noop_log_write,
    backend_noop_span_begin,
    backend_noop_span_end
};

// No-op function for disabled log levels
static void log_noop(LogLevel level, const char* fmt, ...) {
    // Empty, immediate return for performance
}

// Active dispatch function that formats and sends to backend
static void log_dispatch(LogLevel level, const char* fmt, ...) {
    // Format the message using vsnprintf
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Call backend with formatted message - truly branchless!
    g_activeBackend.log_write(level, buffer);
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

// Public API implementations
void init() {
    // Initialize with builtin backend and INFO level
    // This ensures all function pointers are valid before any logging occurs
    set_backend(builtin_backend());
    set_level(LOG_LEVEL_INFO);
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
    // Validate backend contract: all function pointers must be non-null
    if (!backend || 
        !backend->init || 
        !backend->shutdown || 
        !backend->log_write || 
        !backend->span_begin || 
        !backend->span_end) {
        return;  // Invalid backend - reject it
    }
    
    // Call shutdown on old backend (guaranteed non-null by contract)
    g_activeBackend.shutdown();

    // Copy the new backend by value
    g_activeBackend = *backend;

    // Call init on new backend (guaranteed non-null by contract)
    g_activeBackend.init();
}

LogBackend* get_backend() {
    return &g_activeBackend;
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
        
        // Call backend span_begin (guaranteed non-null by contract)
        m_handle = g_activeBackend.span_begin(level, name);
    }
}

Span::~Span() {
    if (m_active) {
        // Calculate elapsed time in microseconds
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_start).count();
        
        // Call backend span_end (guaranteed non-null by contract)
        g_activeBackend.span_end(m_handle, m_level, m_name, elapsed);
    }
}

} // namespace lumberjack
