// Custom backend example demonstrating:
//   1. A minimal custom backend (in-memory buffer)
//   2. Using lumberjack::WriteBuffer and TimestampCache for high-performance output

#include <lumberjack/lumberjack.h>
#include <lumberjack/utils.h>
#include <vector>
#include <string>
#include <cstdio>
#include <mutex>

// =========================================================================
// Example 1: Minimal custom backend — stores messages in memory
// =========================================================================
namespace memory_backend {

static std::vector<std::string> g_messages;

void init()     { g_messages.clear(); }
void shutdown() { g_messages.clear(); }

void log_write(lumberjack::LogLevel level, const char* message) {
    const char* levels[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG"};
    char buf[512];
    snprintf(buf, sizeof(buf), "[%s] %s", levels[level], message);
    g_messages.push_back(buf);
}

void* span_begin(lumberjack::LogLevel, const char*) { return nullptr; }

void span_end(void*, lumberjack::LogLevel level,
              const char* name, long long elapsed_us) {
    char msg[256];
    snprintf(msg, sizeof(msg), "SPAN '%s' took %lld us", name, elapsed_us);
    log_write(level, msg);
}

void dump() {
    printf("\n  Memory backend (%zu messages):\n", g_messages.size());
    for (size_t i = 0; i < g_messages.size(); i++)
        printf("    %zu: %s\n", i + 1, g_messages[i].c_str());
    printf("\n");
}

lumberjack::LogBackend backend = {
    "memory", init, shutdown, log_write, span_begin, span_end
};

} // namespace memory_backend

// =========================================================================
// Example 2: High-performance file backend using WriteBuffer + TimestampCache
// =========================================================================
namespace fast_file_backend {

static FILE*                     g_file = nullptr;
static std::mutex                g_mutex;
static lumberjack::WriteBuffer   g_buf;
static lumberjack::TimestampCache g_ts;

static const char* const g_levels[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG"};

void init() {
    // Open a log file (in a real app, make the path configurable)
    g_file = fopen("app.log", "w");
    if (!g_file) g_file = stderr;

    // Enable buffered writes (16 KB) and timestamp caching (10 ms)
    g_buf.enable(g_file, 16384);
    g_ts.set_interval_ms(10);
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_buf.flush(g_file);
    if (g_file && g_file != stderr) {
        fclose(g_file);
        g_file = nullptr;
    }
}

void log_write(lumberjack::LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const char* ts = g_ts.get();
    char line[1280];
    int len = snprintf(line, sizeof(line), "[%s] [%s] %s\n",
                       ts, g_levels[level], message);
    if (len < 0) return;
    if (static_cast<size_t>(len) >= sizeof(line)) len = sizeof(line) - 1;

    g_buf.write(g_file, line, static_cast<size_t>(len));
}

void* span_begin(lumberjack::LogLevel, const char*) { return nullptr; }

void span_end(void*, lumberjack::LogLevel level,
              const char* name, long long elapsed_us) {
    char msg[256];
    snprintf(msg, sizeof(msg), "SPAN '%s' took %lld us", name, elapsed_us);
    log_write(level, msg);
}

void flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_buf.flush(g_file);
}

lumberjack::LogBackend backend = {
    "fast_file", init, shutdown, log_write, span_begin, span_end
};

} // namespace fast_file_backend

// =========================================================================
// Main
// =========================================================================
int main() {
    // --- Example 1: Memory backend ---
    printf("=== Example 1: Memory Backend ===\n");
    lumberjack::init();
    lumberjack::set_backend(&memory_backend::backend);
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);

    LOG_INFO("Application started");
    LOG_WARN("Low disk space: %d%% remaining", 12);
    LOG_DEBUG("Cache hit ratio: %.2f", 0.87);

    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "startup");
        for (volatile int i = 0; i < 100000; i++) {}
    }

    memory_backend::dump();

    // --- Example 2: Fast file backend ---
    printf("=== Example 2: Fast File Backend (WriteBuffer + TimestampCache) ===\n");
    lumberjack::set_backend(&fast_file_backend::backend);

    LOG_INFO("Switched to fast file backend");
    LOG_ERROR("Example error: %s", "disk full");

    for (int i = 0; i < 100; i++) {
        LOG_DEBUG("Batch message %d", i);
    }

    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "batch_processing");
        for (volatile int i = 0; i < 500000; i++) {}
    }

    fast_file_backend::flush();

    // Switch back to builtin to print final message to stderr
    lumberjack::set_backend(lumberjack::builtin_backend());
    LOG_INFO("Wrote log output to app.log");
    LOG_INFO("Example complete");

    return 0;
}
