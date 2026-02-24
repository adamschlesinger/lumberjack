// builtin.cpp — Default stderr logging backend with optional buffering
// and timestamp caching.
//
// Performance knobs exposed through the public API:
//   builtin_set_buffered()          — batch writes to avoid per-call fflush()
//   builtin_set_timestamp_cache_ms() — amortize localtime/strftime cost
//
// All mutable state is protected by g_mutex. The backend is safe to use
// from multiple threads.

#include "lumberjack/lumberjack.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>

namespace lumberjack {

// ----------------------------------------------------------------------------
// Configuration state
// ----------------------------------------------------------------------------

static FILE*        g_output             = stderr;
static bool         g_buffered           = false;
static char*        g_writeBuffer        = nullptr;
static size_t       g_writeBufferSize    = 0;
static size_t       g_writeBufferPos     = 0;
static unsigned int g_timestampCacheMs   = 0;  // 0 = disabled
static std::mutex   g_mutex;

// ----------------------------------------------------------------------------
// Timestamp caching
//
// Formatting a timestamp (localtime + strftime) costs ~100-200ns per call.
// When caching is enabled, we store the formatted string and only refresh
// it once the configured interval has elapsed. For interval_ms=10 this
// means at most one strftime call per 10ms instead of potentially thousands.
// ----------------------------------------------------------------------------

static char g_cachedTimestamp[32] = {};
static std::chrono::steady_clock::time_point g_timestampExpiry = {};

// Lookup table mapping LogLevel ordinals to fixed-width display strings.
static const char* const g_levelStrings[LOG_COUNT] = {
    "NONE ",  // index 0 — never used in formatted output
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG"
};

// Formats the current wall-clock time into g_cachedTimestamp.
// Must be called under g_mutex.
static void refresh_timestamp_locked() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char date_part[24];
    std::strftime(date_part, sizeof(date_part), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time_t_now));

    snprintf(g_cachedTimestamp, sizeof(g_cachedTimestamp),
             "%s.%03lld", date_part, static_cast<long long>(ms.count()));
}

// Returns the current timestamp string. When caching is enabled, reuses the
// cached value until the expiry interval has passed.
// Must be called under g_mutex.
static const char* get_timestamp_locked() {
    if (g_timestampCacheMs == 0) {
        refresh_timestamp_locked();
        return g_cachedTimestamp;
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= g_timestampExpiry) {
        refresh_timestamp_locked();
        g_timestampExpiry = now + std::chrono::milliseconds(g_timestampCacheMs);
    }
    return g_cachedTimestamp;
}

// ----------------------------------------------------------------------------
// Buffered output
//
// Unbuffered mode calls fwrite + fflush per log line (~400-500ns for the
// fflush alone on most systems). Buffered mode accumulates output in a
// heap-allocated buffer and flushes in bulk when:
//   1. The buffer is full.
//   2. The caller invokes builtin_flush().
//   3. The backend shuts down.
// ----------------------------------------------------------------------------

// Writes the buffer contents to g_output and resets the position.
// Must be called under g_mutex.
static void flush_buffer_locked() {
    if (g_writeBufferPos > 0 && g_output) {
        fwrite(g_writeBuffer, 1, g_writeBufferPos, g_output);
        fflush(g_output);
        g_writeBufferPos = 0;
    }
}

// Appends data to the write buffer, flushing first if it won't fit.
// Messages larger than the entire buffer are written directly.
// Must be called under g_mutex.
static void buffered_write_locked(const char* data, size_t len) {
    if (len >= g_writeBufferSize) {
        flush_buffer_locked();
        fwrite(data, 1, len, g_output);
        fflush(g_output);
        return;
    }

    if (g_writeBufferPos + len > g_writeBufferSize) {
        flush_buffer_locked();
    }

    memcpy(g_writeBuffer + g_writeBufferPos, data, len);
    g_writeBufferPos += len;
}

// ----------------------------------------------------------------------------
// Backend callbacks
// ----------------------------------------------------------------------------

static void builtin_init() {
    // No-op — configuration state is managed by the public config functions.
}

// Flushes any pending buffered data and resets the output to stderr.
static void builtin_shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    flush_buffer_locked();
    g_output = stderr;
}

// Formats and writes a single log line: [timestamp] [LEVEL] message\n
// Respects the current buffered/unbuffered mode.
static void builtin_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const char* ts = get_timestamp_locked();
    const char* level_str = g_levelStrings[level];

    char line[1280];
    int len = snprintf(line, sizeof(line), "[%s] [%s] %s\n", ts, level_str, message);
    if (len < 0) return;
    if (static_cast<size_t>(len) >= sizeof(line)) len = sizeof(line) - 1;

    if (g_buffered && g_writeBuffer) {
        buffered_write_locked(line, static_cast<size_t>(len));
    } else {
        fwrite(line, 1, static_cast<size_t>(len), g_output);
        fflush(g_output);
    }
}

// The builtin backend doesn't track span handles — returns nullptr.
static void* builtin_span_begin(LogLevel, const char*) {
    return nullptr;
}

// Logs the span name and elapsed time as a regular log line.
static void builtin_span_end(void*, LogLevel level,
                              const char* name, long long elapsed_us) {
    char message[256];
    snprintf(message, sizeof(message), "SPAN '%s' took %lld us", name, elapsed_us);
    builtin_log_write(level, message);
}

// ----------------------------------------------------------------------------
// Public backend accessor
// ----------------------------------------------------------------------------

LogBackend* builtin_backend() {
    static LogBackend backend = {
        "builtin",
        builtin_init,
        builtin_shutdown,
        builtin_log_write,
        builtin_span_begin,
        builtin_span_end
    };
    return &backend;
}

// ----------------------------------------------------------------------------
// Configuration API
// ----------------------------------------------------------------------------

// Flushes pending data before switching to the new output stream.
void builtin_set_output(FILE* file) {
    std::lock_guard<std::mutex> lock(g_mutex);
    flush_buffer_locked();
    g_output = file;
}

// Toggles buffered mode. Flushes and frees the old buffer when disabling,
// or (re)allocates when enabling with a different size.
void builtin_set_buffered(bool enabled, size_t buffer_size) {
    std::lock_guard<std::mutex> lock(g_mutex);

    flush_buffer_locked();

    if (enabled && buffer_size > 0) {
        if (g_writeBufferSize != buffer_size) {
            free(g_writeBuffer);
            g_writeBuffer = static_cast<char*>(malloc(buffer_size));
            g_writeBufferSize = buffer_size;
        }
        g_writeBufferPos = 0;
        g_buffered = true;
    } else {
        free(g_writeBuffer);
        g_writeBuffer = nullptr;
        g_writeBufferSize = 0;
        g_writeBufferPos = 0;
        g_buffered = false;
    }
}

void builtin_flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    flush_buffer_locked();
}

// Forces the next log call to recompute the timestamp by resetting the
// expiry to the epoch.
void builtin_set_timestamp_cache_ms(unsigned int interval_ms) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_timestampCacheMs = interval_ms;
    g_timestampExpiry = {};
}

} // namespace lumberjack
