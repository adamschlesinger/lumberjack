#include "lumberjack/lumberjack.h"
#include "lumberjack/utils.h"
#include <cstdio>
#include <mutex>

namespace lumberjack {

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static FILE*          g_output = stderr;
static std::mutex     g_mutex;
static TimestampCache g_tsCache;
static WriteBuffer    g_writeBuf;
static bool           g_seqEnabled = false;
static unsigned long  g_seqCounter = 0;

static const char* const g_levelStrings[LOG_COUNT] = {
    "NONE ", "ERROR", "WARN ", "INFO ", "DEBUG"
};

// ---------------------------------------------------------------------------
// Backend callbacks
// ---------------------------------------------------------------------------

static void builtin_init() {}

static void builtin_shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_writeBuf.flush(g_output);
    g_output = stderr;
}

static void builtin_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const char* level_str = g_levelStrings[level];

    char line[1280];
    int len;
    if (g_seqEnabled) {
        bool refreshed = false;
        const char* ts = g_tsCache.get(&refreshed);
        if (refreshed) g_seqCounter = 0;
        len = snprintf(line, sizeof(line), "[%s] [%s] #%lu %s\n",
                       ts, level_str, g_seqCounter++, message);
    } else {
        const char* ts = g_tsCache.get();
        len = snprintf(line, sizeof(line), "[%s] [%s] %s\n", ts, level_str, message);
    }
    if (len < 0) return;
    if (static_cast<size_t>(len) >= sizeof(line)) len = sizeof(line) - 1;

    g_writeBuf.write(g_output, line, static_cast<size_t>(len));
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
    g_writeBuf.flush(g_output);
    g_output = file;
}

void builtin_set_buffered(bool enabled, size_t buffer_size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (enabled) {
        g_writeBuf.enable(g_output, buffer_size);
    } else {
        g_writeBuf.disable(g_output);
    }
}

void builtin_flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_writeBuf.flush(g_output);
}

void builtin_set_timestamp_cache(unsigned int interval_ms, bool seq) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_tsCache.set_interval_ms(interval_ms);
    g_seqEnabled = seq;
    g_seqCounter = 0;
}

} // namespace lumberjack
