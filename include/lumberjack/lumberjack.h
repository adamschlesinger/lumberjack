// lumberjack.h — Public API for the lumberjack logging library.
//
// Lumberjack is a lightweight, branchless-dispatch C++ logging library.
// Inactive log levels resolve to no-op function pointers at runtime,
// so disabled calls carry near-zero overhead (no branch prediction misses,
// no format string evaluation).
//
// Quick start:
//   lumberjack::init();
//   LOG_INFO("server started on port %d", port);
//
// See examples/ for backend customization and span usage.

#ifndef LUMBERJACK_H
#define LUMBERJACK_H

#include <cstdio>
#include <chrono>

namespace lumberjack {

// ----------------------------------------------------------------------------
// Log levels
// ----------------------------------------------------------------------------

// Verbosity levels in ascending order. Setting the active level to N enables
// all levels <= N. LOG_LEVEL_NONE disables everything; LOG_COUNT is a
// sentinel used for array sizing and should not be passed to API functions.
enum LogLevel {
    LOG_LEVEL_NONE  = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_INFO  = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_COUNT       = 5
};

// Concise aliases inspired by Rust's tracing crate.
// Usage: lumberjack::Level::Error, lumberjack::Level::Debug, etc.
namespace Level {
    constexpr LogLevel Error = LOG_LEVEL_ERROR;
    constexpr LogLevel Warn  = LOG_LEVEL_WARN;
    constexpr LogLevel Info  = LOG_LEVEL_INFO;
    constexpr LogLevel Debug = LOG_LEVEL_DEBUG;
} // namespace Level

// ----------------------------------------------------------------------------
// Backend interface
// ----------------------------------------------------------------------------

// A pluggable logging destination. Implement this struct to route log output
// to a custom sink (file, network, in-memory buffer, etc.).
//
// All function pointers must be non-null. Backends that don't need certain
// functionality should provide no-op implementations.
//
//   name       — Human-readable identifier, used for diagnostics.
//   init       — Called once when the backend is activated via set_backend().
//   shutdown   — Called when the backend is replaced or the library tears down.
//   log_write  — Receives a fully-formatted message string at the given level.
//   span_begin — Called when a Span is constructed. Return an opaque handle
//                (or nullptr) that will be passed back to span_end.
//   span_end   — Called when a Span is destroyed. Receives the handle from
//                span_begin, the span name, and elapsed time in microseconds.
struct LogBackend {
    const char* name;
    void (*init)();
    void (*shutdown)();
    void (*log_write)(LogLevel level, const char* message);
    void* (*span_begin)(LogLevel level, const char* name);
    void (*span_end)(void* handle, LogLevel level, const char* name, long long elapsed_us);
};

// ----------------------------------------------------------------------------
// Core API
// ----------------------------------------------------------------------------

// Initializes the library with the builtin backend at LOG_LEVEL_INFO.
// Safe to call multiple times; each call resets state.
void init();

// Sets the active log level. Levels above this threshold become no-ops.
// Takes effect immediately for all subsequent log calls and spans.
void set_level(LogLevel level);

// Returns the current active log level.
LogLevel get_level();

// Installs a new backend, shutting down the previous one first.
// The backend pointer is copied — the caller retains ownership of the
// LogBackend struct but must keep it alive for the duration of use.
// Silently returns if backend or any of its function pointers are null.
void set_backend(LogBackend* backend);

// Returns a pointer to the currently active backend (never null after init).
LogBackend* get_backend();

// ----------------------------------------------------------------------------
// Built-in backend
// ----------------------------------------------------------------------------

// Returns the built-in stderr logging backend. This is the default backend
// installed by init(). Output format: [timestamp] [LEVEL] message
LogBackend* builtin_backend();

// Redirects built-in backend output to the given FILE stream.
// Flushes any pending buffered data before switching.
void builtin_set_output(FILE* file);

// Enables or disables buffered write mode. When enabled, log output is
// accumulated in a memory buffer and flushed in bulk — either when the
// buffer fills, on an explicit builtin_flush() call, or at shutdown.
// This eliminates per-call fflush() overhead.
//
// buffer_size controls the allocation size in bytes (default 8192).
// Disabling flushes any pending data and frees the buffer.
void builtin_set_buffered(bool enabled, size_t buffer_size = 8192);

// Flushes the built-in backend's write buffer to the output stream.
// No-op if buffered mode is not active.
void builtin_flush();

// Controls timestamp caching for the built-in backend. Instead of calling
// localtime/strftime on every log line, the formatted timestamp is cached
// and refreshed at most once per interval_ms milliseconds.
// Set to 0 to disable caching (recompute every call).
//
// When seq is true, each log line includes a monotonically increasing
// counter that resets whenever the cached timestamp refreshes. This
// restores ordering resolution lost by caching.
// Output format becomes: [timestamp] [LEVEL] #N message
void builtin_set_timestamp_cache(unsigned int interval_ms, bool seq = false);

// ----------------------------------------------------------------------------
// Function pointer types (public for macro / Span use)
// ----------------------------------------------------------------------------

// Signature for log dispatch functions. Accepts printf-style format + args.
using LogFunction = void (*)(LogLevel, const char*, ...);

// Signature for clock read functions. Returns a steady_clock time_point
// (or a zero time_point for the no-op path).
using ClockFunction = std::chrono::steady_clock::time_point (*)();

// Dispatch tables indexed by LogLevel. Active levels point to real
// implementations; inactive levels point to no-ops. The macros below
// index directly into these arrays for branchless dispatch.
extern LogFunction g_logFunctions[LOG_COUNT];
extern ClockFunction g_clockFunctions[LOG_COUNT];

// ----------------------------------------------------------------------------
// Span — RAII timing measurement
// ----------------------------------------------------------------------------

// Measures wall-clock time between construction and destruction, reporting
// the elapsed duration through the active backend's span callbacks.
//
// When the log level is inactive, both the clock reads and the backend
// callbacks resolve to no-ops via function pointer dispatch — near-zero
// overhead with no branches.
//
// Usage:
//   {
//       lumberjack::Span span(LOG_LEVEL_INFO, "db_query");
//       execute_query();
//   }  // elapsed time logged here
//
// Or via the convenience macro:
//   LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "db_query");
//
// Non-copyable and non-movable to prevent accidental lifetime issues.
class Span {
public:
    // Constructs a span at the given level with a descriptive name.
    // Reads the clock and calls span_begin on the active backend.
    Span(LogLevel level, const char* name);

    // Reads the clock, computes elapsed microseconds, and calls span_end.
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

// ----------------------------------------------------------------------------
// Convenience macros (global namespace)
// ----------------------------------------------------------------------------

// Log at a specific level with printf-style formatting.
// These index directly into the dispatch table — inactive levels are no-ops.
#define LOG_ERROR(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_ERROR](lumberjack::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_WARN](lumberjack::LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_INFO](lumberjack::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_DEBUG](lumberjack::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
// Log at a dynamic level — useful when the level is a runtime variable.
//   LogLevel lvl = compute_level();
//   LOG_AT(lvl, "something happened: %s", detail);
#define LOG_AT(level, fmt, ...) \
    lumberjack::g_logFunctions[level](level, fmt, ##__VA_ARGS__)

// Creates an RAII Span scoped to the enclosing block. The span name appears
// in backend output along with the elapsed time when the block exits.
#define LOG_SPAN(level, name) lumberjack::Span _log_span_##__LINE__(level, name)

// Level-specific span macros — tracing-style convenience.
//   ERROR_SPAN("db_write");   // equivalent to LOG_SPAN(lumberjack::LOG_LEVEL_ERROR, "db_write")
//   INFO_SPAN("request");
//   TRACE_SPAN("hot_loop");
#define ERROR_SPAN(name) LOG_SPAN(lumberjack::LOG_LEVEL_ERROR, name)
#define WARN_SPAN(name)  LOG_SPAN(lumberjack::LOG_LEVEL_WARN, name)
#define INFO_SPAN(name)  LOG_SPAN(lumberjack::LOG_LEVEL_INFO, name)
#define DEBUG_SPAN(name) LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, name)

// Short-form macros — inspired by Rust's tracing crate.
//   ERROR("connection lost: %s", reason);
//   INFO("listening on port %d", port);
//   TRACE("tick");
//
// These are opt-in. If another library defines the same names, you can
// #define LUMBERJACK_NO_SHORT_MACROS before including this header to
// suppress them and stick with the LOG_* variants.
#ifndef LUMBERJACK_NO_SHORT_MACROS
#define ERROR(fmt, ...) LOG_ERROR(fmt, ##__VA_ARGS__)
#define WARN(fmt, ...)  LOG_WARN(fmt, ##__VA_ARGS__)
#define INFO(fmt, ...)  LOG_INFO(fmt, ##__VA_ARGS__)
#define DEBUG(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
#endif

#endif // LUMBERJACK_H
