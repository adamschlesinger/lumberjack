#include <lumberjack/lumberjack.h>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <mutex>
#include <thread>

// =========================================================================
// Naive branching logger for comparison
// A straightforward logger with timestamps, mutex, formatting, and flushing.
// No optimizations — represents a typical hand-rolled logger.
// =========================================================================
class NaiveLogger {
public:
    enum Level { ERROR = 1, WARN = 2, INFO = 3, DEBUG = 4 };

    NaiveLogger() : m_level(INFO), m_output(stderr) {}

    void set_level(Level level) { m_level = level; }
    void set_output(FILE* output) { m_output = output; }

    void log_error(const char* fmt, ...) {
        if (m_level >= ERROR) { va_list a; va_start(a, fmt); write(ERROR, fmt, a); va_end(a); }
    }
    void log_warn(const char* fmt, ...) {
        if (m_level >= WARN) { va_list a; va_start(a, fmt); write(WARN, fmt, a); va_end(a); }
    }
    void log_info(const char* fmt, ...) {
        if (m_level >= INFO) { va_list a; va_start(a, fmt); write(INFO, fmt, a); va_end(a); }
    }
    void log_debug(const char* fmt, ...) {
        if (m_level >= DEBUG) { va_list a; va_start(a, fmt); write(DEBUG, fmt, a); va_end(a); }
    }

private:
    Level m_level;
    FILE* m_output;
    std::mutex m_mutex;

    const char* level_str(Level l) {
        switch (l) {
            case ERROR: return "ERROR"; case WARN: return "WARN";
            case INFO:  return "INFO";  case DEBUG: return "DEBUG";
            default: return "?";
        }
    }

    void write(Level level, const char* fmt, va_list args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        char message[1024];
        vsnprintf(message, sizeof(message), fmt, args);

        auto now = std::chrono::system_clock::now();
        auto tt  = std::chrono::system_clock::to_time_t(now);
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));

        fprintf(m_output, "[%s.%03lld] [%s] %s\n",
                ts, static_cast<long long>(ms.count()), level_str(level), message);
        fflush(m_output);
    }
};

// =========================================================================
// Benchmark utilities
// =========================================================================
struct BenchmarkResult {
    const char* name;
    double mean_ns;
    double median_ns;
    double min_ns;
    double max_ns;
    double stddev_ns;
};

template<typename Func>
BenchmarkResult benchmark(const char* name, Func&& func, int iterations, int warmup = 1000) {
    std::vector<double> timings;
    timings.reserve(iterations);

    for (int i = 0; i < warmup; ++i) func();

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        timings.push_back(static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
    }

    std::sort(timings.begin(), timings.end());
    double sum    = std::accumulate(timings.begin(), timings.end(), 0.0);
    double mean   = sum / timings.size();
    double median = timings[timings.size() / 2];
    double sq_sum = 0.0;
    for (double t : timings) sq_sum += (t - mean) * (t - mean);

    return {name, mean, median, timings.front(), timings.back(),
            std::sqrt(sq_sum / timings.size())};
}

void print_result(const BenchmarkResult& r) {
    printf("  %-52s Mean: %9.1f ns  Median: %9.1f ns  Min: %8.1f ns\n",
           r.name, r.mean_ns, r.median_ns, r.min_ns);
}

void print_comparison(const BenchmarkResult& baseline, const BenchmarkResult& test) {
    double speedup = baseline.mean_ns / test.mean_ns;
    printf("    -> %s is %.2fx %s than %s\n",
           test.name, std::abs(speedup),
           speedup > 1.0 ? "faster" : "slower", baseline.name);
}

// =========================================================================
// Main
// =========================================================================
int main() {
    const int N = 1000000;

    printf("=============================================================\n");
    printf("  Lumberjack Performance Benchmark\n");
    printf("  Iterations: %d\n", N);
    printf("=============================================================\n\n");

    FILE* devnull = fopen("/dev/null", "w");
    NaiveLogger naive;
    naive.set_output(devnull);

    lumberjack::init();
    lumberjack::builtin_set_output(devnull);

    // =================================================================
    // TEST 1: Disabled log levels (most common production case)
    // =================================================================
    printf("--- Test 1: Single Disabled Call (DEBUG when level=INFO) ---\n");
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    naive.set_level(NaiveLogger::INFO);

    auto empty = benchmark("Empty (compiled-out baseline)", [&]() {}, N);
    auto naive_dis = benchmark("Naive (disabled)", [&]() {
        naive.log_debug("Debug: %d %s", 42, "test");
    }, N);
    auto lj_dis = benchmark("Lumberjack (disabled)", [&]() {
        LOG_DEBUG("Debug: %d %s", 42, "test");
    }, N);

    print_result(empty);
    print_result(naive_dis);
    print_result(lj_dis);
    print_comparison(naive_dis, lj_dis);
    printf("\n");

    // =================================================================
    // TEST 2: Disabled spans (clock noop optimization)
    // =================================================================
    printf("--- Test 2: Disabled Span (LOG_SPAN at DEBUG when level=INFO) ---\n");

    auto span_empty = benchmark("Empty (baseline)", [&]() {}, N);
    auto span_dis = benchmark("Lumberjack Span (disabled)", [&]() {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "noop_span");
    }, N);

    print_result(span_empty);
    print_result(span_dis);
    printf("    -> Overhead per disabled span: %.1f ns\n\n",
           span_dis.mean_ns - span_empty.mean_ns);

    // =================================================================
    // TEST 3: Enabled single call
    // =================================================================
    printf("--- Test 3: Single Enabled Call (INFO) — Backend Modes ---\n");
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);

    auto naive_en = benchmark("Naive (enabled)", [&]() {
        naive.log_info("Info: %d %s", 42, "test");
    }, N);

    // Lumberjack: unbuffered (original behavior)
    lumberjack::builtin_set_buffered(false);
    lumberjack::builtin_set_timestamp_cache_ms(0);
    auto lj_unbuf = benchmark("Lumberjack unbuffered", [&]() {
        LOG_INFO("Info: %d %s", 42, "test");
    }, N);

    // Lumberjack: buffered + cached timestamp
    lumberjack::builtin_set_buffered(true, 8192);
    lumberjack::builtin_set_timestamp_cache_ms(10);
    auto lj_buf_cache = benchmark("Lumberjack buf+cache", [&]() {
        LOG_INFO("Info: %d %s", 42, "test");
    }, N);
    lumberjack::builtin_flush();

    print_result(naive_en);
    print_result(lj_unbuf);
    print_result(lj_buf_cache);
    print_comparison(naive_en, lj_unbuf);
    print_comparison(naive_en, lj_buf_cache);
    printf("\n");

    // =================================================================
    // TEST 4: Tight loop — 100 disabled calls
    // =================================================================
    printf("--- Test 4: Tight Loop (100 Disabled Calls) ---\n");
    lumberjack::builtin_set_buffered(false);
    lumberjack::builtin_set_timestamp_cache_ms(0);
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    naive.set_level(NaiveLogger::INFO);

    auto loop_empty = benchmark("Empty loop", [&]() {
        for (int i = 0; i < 100; ++i) { }
    }, N / 100);

    auto loop_naive = benchmark("Naive (100 disabled)", [&]() {
        for (int i = 0; i < 100; ++i)
            naive.log_debug("Debug: %d", i);
    }, N / 100);

    auto loop_lj = benchmark("Lumberjack (100 disabled)", [&]() {
        for (int i = 0; i < 100; ++i)
            LOG_DEBUG("Debug: %d", i);
    }, N / 100);

    print_result(loop_empty);
    print_result(loop_naive);
    print_result(loop_lj);
    print_comparison(loop_naive, loop_lj);
    printf("\n");

    // =================================================================
    // TEST 5: Tight loop — 100 enabled calls
    // =================================================================
    printf("--- Test 5: Tight Loop (100 Enabled Calls) — Backend Modes ---\n");

    auto loop_naive_en = benchmark("Naive (100 enabled)", [&]() {
        for (int i = 0; i < 100; ++i)
            naive.log_info("Info: %d", i);
    }, N / 100);

    // Lumberjack: unbuffered
    lumberjack::builtin_set_buffered(false);
    lumberjack::builtin_set_timestamp_cache_ms(0);
    auto loop_lj_unbuf = benchmark("Lumberjack unbuf (100 en)", [&]() {
        for (int i = 0; i < 100; ++i)
            LOG_INFO("Info: %d", i);
    }, N / 100);

    // Lumberjack: buffered + cached timestamp
    lumberjack::builtin_set_buffered(true, 16384);
    lumberjack::builtin_set_timestamp_cache_ms(10);
    auto loop_lj_full = benchmark("Lumberjack buf+cache (100 en)", [&]() {
        for (int i = 0; i < 100; ++i)
            LOG_INFO("Info: %d", i);
    }, N / 100);
    lumberjack::builtin_flush();

    print_result(loop_naive_en);
    print_result(loop_lj_unbuf);
    print_result(loop_lj_full);
    print_comparison(loop_naive_en, loop_lj_unbuf);
    print_comparison(loop_naive_en, loop_lj_full);
    printf("\n");

    // =================================================================
    // TEST 6: Mixed workload
    // =================================================================
    printf("--- Test 6: Mixed Workload (3 enabled + 2 disabled) ---\n");
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);

    auto mix_naive = benchmark("Naive (mixed)", [&]() {
        naive.log_error("Error: %d", 1);
        naive.log_warn("Warn: %d", 2);
        naive.log_info("Info: %d", 3);
        naive.log_debug("Debug: %d", 4);
        naive.log_debug("Debug: %d", 5);
    }, N);

    // Lumberjack: unbuffered
    lumberjack::builtin_set_buffered(false);
    lumberjack::builtin_set_timestamp_cache_ms(0);
    auto mix_lj = benchmark("Lumberjack unbuffered (mixed)", [&]() {
        LOG_ERROR("Error: %d", 1);
        LOG_WARN("Warn: %d", 2);
        LOG_INFO("Info: %d", 3);
        LOG_DEBUG("Debug: %d", 4);
        LOG_DEBUG("Debug: %d", 5);
    }, N);

    // Lumberjack: optimized
    lumberjack::builtin_set_buffered(true, 8192);
    lumberjack::builtin_set_timestamp_cache_ms(10);
    auto mix_lj_opt = benchmark("Lumberjack buf+cache (mixed)", [&]() {
        LOG_ERROR("Error: %d", 1);
        LOG_WARN("Warn: %d", 2);
        LOG_INFO("Info: %d", 3);
        LOG_DEBUG("Debug: %d", 4);
        LOG_DEBUG("Debug: %d", 5);
    }, N);
    lumberjack::builtin_flush();

    print_result(mix_naive);
    print_result(mix_lj);
    print_result(mix_lj_opt);
    print_comparison(mix_naive, mix_lj);
    print_comparison(mix_naive, mix_lj_opt);
    printf("\n");

    // =================================================================
    // TEST 7: Enabled spans
    // =================================================================
    printf("--- Test 7: Enabled Span Overhead ---\n");
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
    lumberjack::builtin_set_buffered(true, 16384);
    lumberjack::builtin_set_timestamp_cache_ms(10);

    auto span_en = benchmark("Lumberjack Span (enabled, buf+cache)", [&]() {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "bench_span");
    }, N);
    lumberjack::builtin_flush();

    print_result(span_en);
    printf("\n");

    // =================================================================
    fclose(devnull);

    printf("=============================================================\n");
    printf("  Summary\n");
    printf("=============================================================\n");
    printf("  Disabled path:  Function pointer noop — near zero cost\n");
    printf("  Disabled spans: Clock noop eliminates steady_clock reads\n");
    printf("  Buffered mode:  Eliminates per-call fflush (biggest win)\n");
    printf("  Cached TS:      Amortizes localtime/strftime cost\n");
    printf("  All optimizations stack and are runtime-switchable.\n");

    return 0;
}