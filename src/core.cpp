#include "lumberjack/lumberjack.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace lumberjack {

// Forward declarations
static void log_noop(LogLevel level, const char* fmt, ...);
static void log_dispatch(LogLevel level, const char* fmt, ...);
static void* span_begin_noop(LogLevel level, const char* name);
static void* span_begin_dispatch(LogLevel level, const char* name);
static void span_end_noop(void* handle, LogLevel level, const char* name, long long elapsed_us);
static void span_end_dispatch(void* handle, LogLevel level, const char* name, long long elapsed_us);
static const char* level_to_string(LogLevel level);

// No-op backend functions for safe static initialization
static void backend_noop_init() {}
static void backend_noop_shutdown() {}
static void backend_noop_log_write(LogLevel level, const char* message) {}
static void* backend_noop_span_begin(LogLevel level, const char* name) { return nullptr; }
static void backend_noop_span_end(void* handle, LogLevel level, const char* name, long long elapsed_us) {}

// Function pointer types for branchless dispatch
using SpanBeginFunction = void* (*)(LogLevel, const char*);
using SpanEndFunction = void (*)(void*, LogLevel, const char*, long long);

// Global state
LogFunction g_logFunctions[LOG_COUNT] = {
    log_noop,  // LOG_LEVEL_NONE
    log_noop,  // LOG_LEVEL_ERROR
    log_noop,  // LOG_LEVEL_WARN
    log_noop,  // LOG_LEVEL_INFO
    log_noop   // LOG_LEVEL_DEBUG
};

// Span function pointer arrays for branchless dispatch
static SpanBeginFunction g_spanBeginFunctions[LOG_COUNT] = {
    span_begin_noop,  // LOG_LEVEL_NONE
    span_begin_noop,  // LOG_LEVEL_ERROR
    span_begin_noop,  // LOG_LEVEL_WARN
    span_begin_noop,  // LOG_LEVEL_INFO
    span_begin_noop   // LOG_LEVEL_DEBUG
};

static SpanEndFunction g_spanEndFunctions[LOG_COUNT] = {
    span_end_noop,  // LOG_LEVEL_NONE
    span_end_noop,  // LOG_LEVEL_ERROR
    span_end_noop,  // LOG_LEVEL_WARN
    span_end_noop,  // LOG_LEVEL_INFO
    span_end_noop   // LOG_LEVEL_DEBUG
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

// No-op span functions for disabled log levels
static void* span_begin_noop(LogLevel level, const char* name) {
    return nullptr;
}

static void span_end_noop(void* handle, LogLevel level, const char* name, long long elapsed_us) {
    // Empty, immediate return for performance
}

// Active span dispatch functions
static void* span_begin_dispatch(LogLevel level, const char* name) {
    return g_activeBackend.span_begin(level, name);
}

static void span_end_dispatch(void* handle, LogLevel level, const char* name, long long elapsed_us) {
    g_activeBackend.span_end(handle, level, name, elapsed_us);
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
    
    // Update function pointer arrays based on level threshold
    // For levels <= active level: point to dispatch functions
    // For levels > active level: point to no-op functions
    for (int i = 0; i < LOG_COUNT; i++) {
        if (i <= level) {
            g_logFunctions[i] = log_dispatch;
            g_spanBeginFunctions[i] = span_begin_dispatch;
            g_spanEndFunctions[i] = span_end_dispatch;
        } else {
            g_logFunctions[i] = log_noop;
            g_spanBeginFunctions[i] = span_begin_noop;
            g_spanEndFunctions[i] = span_end_noop;
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
{
    // Always start timing and call span_begin - branchless!
    m_start = std::chrono::steady_clock::now();
    m_handle = g_spanBeginFunctions[level](level, name);
}

Span::~Span() {
    // Calculate elapsed time in microseconds
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - m_start).count();
    
    // Call span_end - branchless!
    g_spanEndFunctions[m_level](m_handle, m_level, m_name, elapsed);
}

} // namespace lumberjack
