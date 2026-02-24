// core.cpp — Branchless dispatch engine and public API implementation.
//
// The key idea: instead of checking `if (level <= g_currentLevel)` on every
// log call, we maintain arrays of function pointers indexed by LogLevel.
// Active levels point to real implementations; inactive levels point to
// no-ops that return immediately. set_level() rewires the arrays, so the
// hot path is a single indirect call with no branch.

#include "lumberjack/lumberjack.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace lumberjack {

// ----------------------------------------------------------------------------
// Forward declarations — no-op and dispatch variants
// ----------------------------------------------------------------------------

static void log_noop(LogLevel level, const char* fmt, ...);
static void log_dispatch(LogLevel level, const char* fmt, ...);
static void* span_begin_noop(LogLevel level, const char* name);
static void* span_begin_dispatch(LogLevel level, const char* name);
static void span_end_noop(void* handle, LogLevel level, const char* name, long long elapsed_us);
static void span_end_dispatch(void* handle, LogLevel level, const char* name, long long elapsed_us);
static std::chrono::steady_clock::time_point clock_noop();
static std::chrono::steady_clock::time_point clock_real();

// Safe no-op callbacks used by the placeholder backend before init().
static void backend_noop_init() {}
static void backend_noop_shutdown() {}
static void backend_noop_log_write(LogLevel, const char*) {}
static void* backend_noop_span_begin(LogLevel, const char*) { return nullptr; }
static void backend_noop_span_end(void*, LogLevel, const char*, long long) {}

// Internal function pointer types for span dispatch.
using SpanBeginFunction = void* (*)(LogLevel, const char*);
using SpanEndFunction = void (*)(void*, LogLevel, const char*, long long);

// ----------------------------------------------------------------------------
// Global dispatch tables
//
// All tables start fully wired to no-ops. init() / set_level() rewire them
// to real implementations for the active levels.
// ----------------------------------------------------------------------------

LogFunction g_logFunctions[LOG_COUNT] = {
    log_noop,  // LOG_LEVEL_NONE — always a no-op
    log_noop,  // LOG_LEVEL_ERROR
    log_noop,  // LOG_LEVEL_WARN
    log_noop,  // LOG_LEVEL_INFO
    log_noop   // LOG_LEVEL_DEBUG
};

ClockFunction g_clockFunctions[LOG_COUNT] = {
    clock_noop,
    clock_noop,
    clock_noop,
    clock_noop,
    clock_noop
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

// Placeholder backend active before init(). All callbacks are no-ops so
// log calls before initialization are safe (just silently dropped).
static LogBackend g_activeBackend = {
    "noop",
    backend_noop_init,
    backend_noop_shutdown,
    backend_noop_log_write,
    backend_noop_span_begin,
    backend_noop_span_end
};

// ----------------------------------------------------------------------------
// No-op implementations — immediate return, no work
// ----------------------------------------------------------------------------

static void log_noop(LogLevel, const char*, ...) {}

// Formats the message via vsnprintf and forwards to the active backend.
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

static void span_end_noop(void*, LogLevel, const char*, long long) {}

// Forwards to the active backend's span_begin callback.
static void* span_begin_dispatch(LogLevel level, const char* name) {
    return g_activeBackend.span_begin(level, name);
}

// Forwards to the active backend's span_end callback.
static void span_end_dispatch(void* handle, LogLevel level, const char* name, long long elapsed_us) {
    g_activeBackend.span_end(handle, level, name, elapsed_us);
}

// Returns a zero-initialized time_point (no syscall).
static std::chrono::steady_clock::time_point clock_noop() {
    return {};
}

// Reads the real steady clock.
static std::chrono::steady_clock::time_point clock_real() {
    return std::chrono::steady_clock::now();
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void init() {
    set_backend(builtin_backend());
    set_level(LOG_LEVEL_INFO);
}

// Rewires all four dispatch tables so that levels [1..level] point to real
// implementations and everything else points to no-ops. Index 0 (NONE) is
// always a no-op.
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

// Validates that all function pointers are non-null, shuts down the current
// backend, shallow-copies the new one into g_activeBackend, and calls init().
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

// ----------------------------------------------------------------------------
// Span implementation
// ----------------------------------------------------------------------------

// Reads the clock (real or no-op depending on level) and notifies the
// backend that a span has started.
Span::Span(LogLevel level, const char* name)
    : m_level(level)
    , m_name(name)
    , m_handle(nullptr)
    , m_start(g_clockFunctions[level]())
{
    m_handle = g_spanBeginFunctions[level](level, name);
}

// Reads the clock again, computes the delta in microseconds, and notifies
// the backend that the span has ended.
Span::~Span() {
    auto end = g_clockFunctions[m_level]();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - m_start).count();

    g_spanEndFunctions[m_level](m_handle, m_level, m_name, elapsed);
}

} // namespace lumberjack
