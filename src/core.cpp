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
static std::chrono::steady_clock::time_point clock_noop();
static std::chrono::steady_clock::time_point clock_real();

// No-op backend functions for safe static initialization
static void backend_noop_init() {}
static void backend_noop_shutdown() {}
static void backend_noop_log_write(LogLevel, const char*) {}
static void* backend_noop_span_begin(LogLevel, const char*) { return nullptr; }
static void backend_noop_span_end(void*, LogLevel, const char*, long long) {}

// Function pointer types for branchless dispatch
using SpanBeginFunction = void* (*)(LogLevel, const char*);
using SpanEndFunction = void (*)(void*, LogLevel, const char*, long long);

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

LogFunction g_logFunctions[LOG_COUNT] = {
    log_noop,  // LOG_LEVEL_NONE
    log_noop,  // LOG_LEVEL_ERROR
    log_noop,  // LOG_LEVEL_WARN
    log_noop,  // LOG_LEVEL_INFO
    log_noop   // LOG_LEVEL_DEBUG
};

ClockFunction g_clockFunctions[LOG_COUNT] = {
    clock_noop,  // LOG_LEVEL_NONE
    clock_noop,  // LOG_LEVEL_ERROR
    clock_noop,  // LOG_LEVEL_WARN
    clock_noop,  // LOG_LEVEL_INFO
    clock_noop   // LOG_LEVEL_DEBUG
};

static SpanBeginFunction g_spanBeginFunctions[LOG_COUNT] = {
    span_begin_noop,
    span_begin_noop,
    span_begin_noop,
    span_begin_noop,
    span_begin_noop
};

static SpanEndFunction g_spanEndFunctions[LOG_COUNT] = {
    span_end_noop,
    span_end_noop,
    span_end_noop,
    span_end_noop,
    span_end_noop
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

// ---------------------------------------------------------------------------
// No-op / dispatch implementations
// ---------------------------------------------------------------------------

static void log_noop(LogLevel, const char*, ...) {
    // Immediate return — branchless disabled path
}

static void log_dispatch(LogLevel level, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    g_activeBackend.log_write(level, buffer);
}

static void* span_begin_noop(LogLevel, const char*) {
    return nullptr;
}

static void span_end_noop(void*, LogLevel, const char*, long long) {
    // Immediate return
}

static void* span_begin_dispatch(LogLevel level, const char* name) {
    return g_activeBackend.span_begin(level, name);
}

static void span_end_dispatch(void* handle, LogLevel level, const char* name, long long elapsed_us) {
    g_activeBackend.span_end(handle, level, name, elapsed_us);
}

// Clock function pointers: noop returns a zero time_point, real reads the clock
static std::chrono::steady_clock::time_point clock_noop() {
    return {};
}

static std::chrono::steady_clock::time_point clock_real() {
    return std::chrono::steady_clock::now();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void init() {
    set_backend(builtin_backend());
    set_level(LOG_LEVEL_INFO);
}

void set_level(LogLevel level) {
    g_currentLevel = level;

    for (int i = 0; i < LOG_COUNT; i++) {
        if (i > 0 && i <= level) {
            g_logFunctions[i]       = log_dispatch;
            g_clockFunctions[i]     = clock_real;
            g_spanBeginFunctions[i] = span_begin_dispatch;
            g_spanEndFunctions[i]   = span_end_dispatch;
        } else {
            g_logFunctions[i]       = log_noop;
            g_clockFunctions[i]     = clock_noop;
            g_spanBeginFunctions[i] = span_begin_noop;
            g_spanEndFunctions[i]   = span_end_noop;
        }
    }
}

LogLevel get_level() {
    return g_currentLevel;
}

void set_backend(LogBackend* backend) {
    if (!backend ||
        !backend->init ||
        !backend->shutdown ||
        !backend->log_write ||
        !backend->span_begin ||
        !backend->span_end) {
        return;
    }

    g_activeBackend.shutdown();
    g_activeBackend = *backend;
    g_activeBackend.init();
}

LogBackend* get_backend() {
    return &g_activeBackend;
}

// ---------------------------------------------------------------------------
// Span implementation — uses clock function pointers for branchless timing
// ---------------------------------------------------------------------------

Span::Span(LogLevel level, const char* name)
    : m_level(level)
    , m_name(name)
    , m_handle(nullptr)
    , m_start(g_clockFunctions[level]())
{
    m_handle = g_spanBeginFunctions[level](level, name);
}

Span::~Span() {
    auto end = g_clockFunctions[m_level]();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - m_start).count();

    g_spanEndFunctions[m_level](m_handle, m_level, m_name, elapsed);
}

} // namespace lumberjack