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

// ---------------------------------------------------------------------------
// Configuration state
// ---------------------------------------------------------------------------
static FILE*        g_output             = stderr;
static bool         g_buffered           = false;
static char*        g_writeBuffer        = nullptr;
static size_t       g_writeBufferSize    = 0;
static size_t       g_writeBufferPos     = 0;
static unsigned int g_timestampCacheMs   = 0; // 0 = disabled
static std::mutex   g_mutex;

// ---------------------------------------------------------------------------
// Cached timestamp
//
// Instead of calling localtime() + strftime() per log line (~100-200ns),
// we cache the formatted timestamp string and only refresh it when the
// configured interval has elapsed. For interval_ms=10, this means at most
// 1 strftime call per 10ms instead of potentially thousands.
// ---------------------------------------------------------------------------
static char                           g_cachedTimestamp[32] = {};
static std::chrono::steady_clock::time_point g_timestampExpiry = {};

// Level-to-string lookup table (faster than switch)
static const char* const g_levelStrings[LOG_COUNT] = {
    "NONE ", // 0 - never used in practice
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG"
};

// Refresh the cached timestamp. Called under lock.
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

// Get the timestamp string. If caching is enabled, returns the cached version
// and only refreshes when the interval expires. Called under lock.
static const char* get_timestamp_locked() {
    if (g_timestampCacheMs == 0) {
        // No caching — always recompute
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

// ---------------------------------------------------------------------------
// Buffered output
//
// Instead of calling fprintf + fflush per log line (~400-500ns for the
// fflush alone), we accumulate formatted output in a memory buffer and
// flush it in bulk when:
//   1. The buffer is full
//   2. The user calls builtin_flush()
//   3. The backend shuts down
//
// This converts many small kernel writes into fewer large ones.
// ---------------------------------------------------------------------------

// Flush the write buffer to the output FILE. Called under lock.
static void flush_buffer_locked() {
    if (g_writeBufferPos > 0 && g_output) {
        fwrite(g_writeBuffer, 1, g_writeBufferPos, g_output);
        fflush(g_output);
        g_writeBufferPos = 0;
    }
}

// Write a string to the buffer, flushing if needed. Called under lock.
static void buffered_write_locked(const char* data, size_t len) {
    // If this single message is larger than the buffer, flush + direct write
    if (len >= g_writeBufferSize) {
        flush_buffer_locked();
        fwrite(data, 1, len, g_output);
        fflush(g_output);
        return;
    }

    // Flush if it won't fit
    if (g_writeBufferPos + len > g_writeBufferSize) {
        flush_buffer_locked();
    }

    memcpy(g_writeBuffer + g_writeBufferPos, data, len);
    g_writeBufferPos += len;
}

// ---------------------------------------------------------------------------
// Backend callbacks
// ---------------------------------------------------------------------------

static void builtin_init() {
    // Nothing to do — state is managed by config functions
}

static void builtin_shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    flush_buffer_locked();
    g_output = stderr;
}

static void builtin_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const char* ts = get_timestamp_locked();
    const char* level_str = g_levelStrings[level];

    // Format into a stack buffer: [timestamp] [LEVEL] message\n
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

static void* builtin_span_begin(LogLevel, const char*) {
    return nullptr;
}

static void builtin_span_end(void*, LogLevel level,
                              const char* name, long long elapsed_us) {
    char message[256];
    snprintf(message, sizeof(message), "SPAN '%s' took %lld us", name, elapsed_us);
    builtin_log_write(level, message);
}

// ---------------------------------------------------------------------------
// Public backend accessor
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Configuration API
// ---------------------------------------------------------------------------

void builtin_set_output(FILE* file) {
    std::lock_guard<std::mutex> lock(g_mutex);
    flush_buffer_locked();
    g_output = file;
}

void builtin_set_buffered(bool enabled, size_t buffer_size) {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Flush any pending data before changing mode
    flush_buffer_locked();

    if (enabled && buffer_size > 0) {
        // (Re)allocate if size changed
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

void builtin_set_timestamp_cache_ms(unsigned int interval_ms) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_timestampCacheMs = interval_ms;
    // Force an immediate refresh on next use
    g_timestampExpiry = {};
}

} // namespace lumberjack