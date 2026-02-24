#ifndef LUMBERJACK_UTILS_H
#define LUMBERJACK_UTILS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <mutex>

namespace lumberjack {

// ----------------------------------------------------------------------------
// TimestampCache
// ----------------------------------------------------------------------------

// Caches a formatted timestamp string and only refreshes when the configured
// interval has elapsed. Avoids calling localtime() + strftime() on every
// log line in high-throughput scenarios.
//
// Usage:
//   TimestampCache ts_cache;
//   ts_cache.set_interval_ms(10);
//   const char* ts = ts_cache.get();
//
// Thread safety: NOT thread-safe. Caller must hold a lock.
class TimestampCache {
public:
    TimestampCache() = default;

    // Set the cache interval in milliseconds. 0 = disabled (recompute every call).
    void set_interval_ms(unsigned int ms) {
        m_interval_ms = ms;
        m_expiry = {};  // force refresh on next get()
    }

    // Returns a formatted timestamp: "YYYY-MM-DD HH:MM:SS.mmm"
    // Returns the cached value if still fresh, otherwise recomputes.
    // Sets did_refresh to true when the timestamp was recomputed.
    const char* get(bool* did_refresh = nullptr) {
        if (m_interval_ms == 0) {
            refresh();
            if (did_refresh) *did_refresh = true;
            return m_buf;
        }
        auto now = std::chrono::steady_clock::now();
        if (now >= m_expiry) {
            refresh();
            m_expiry = now + std::chrono::milliseconds(m_interval_ms);
            if (did_refresh) *did_refresh = true;
        } else {
            if (did_refresh) *did_refresh = false;
        }
        return m_buf;
    }

private:
    unsigned int m_interval_ms = 0;
    char m_buf[32] = {};
    std::chrono::steady_clock::time_point m_expiry = {};

    void refresh() {
        auto now = std::chrono::system_clock::now();
        auto tt  = std::chrono::system_clock::to_time_t(now);
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        char date[24];
        std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));
        snprintf(m_buf, sizeof(m_buf), "%s.%03lld", date, static_cast<long long>(ms.count()));
    }
};

// ----------------------------------------------------------------------------
// WriteBuffer
// ----------------------------------------------------------------------------

// Accumulates formatted log output in a memory buffer and flushes to a FILE*
// in bulk. Eliminates per-call fflush() overhead (~400-500 ns per call).
//
// Usage:
//   WriteBuffer wb;
//   wb.enable(output, 8192);
//   wb.write(output, data, len);
//   wb.flush(output);
//
// Thread safety: NOT thread-safe. Caller must hold a lock.
class WriteBuffer {
public:
    WriteBuffer() = default;

    ~WriteBuffer() {
        free(m_buf);
    }

    // Non-copyable
    WriteBuffer(const WriteBuffer&) = delete;
    WriteBuffer& operator=(const WriteBuffer&) = delete;

    // Enable buffering with the given size in bytes.
    // Flushes pending data and (re)allocates if the size changed.
    void enable(FILE* output, size_t size) {
        flush(output);
        if (m_size != size) {
            free(m_buf);
            m_buf = static_cast<char*>(malloc(size));
            m_size = size;
        }
        m_pos = 0;
        m_enabled = true;
    }

    // Disable buffering. Flushes pending data and frees the buffer.
    void disable(FILE* output) {
        flush(output);
        free(m_buf);
        m_buf = nullptr;
        m_size = 0;
        m_pos = 0;
        m_enabled = false;
    }

    // Write data to the buffer (if enabled) or directly to output.
    void write(FILE* output, const char* data, size_t len) {
        if (!m_enabled || !m_buf) {
            fwrite(data, 1, len, output);
            fflush(output);
            return;
        }
        // Message larger than buffer — flush + direct write
        if (len >= m_size) {
            flush(output);
            fwrite(data, 1, len, output);
            fflush(output);
            return;
        }
        // Flush if it won't fit
        if (m_pos + len > m_size) {
            flush(output);
        }
        memcpy(m_buf + m_pos, data, len);
        m_pos += len;
    }

    // Flush any pending buffered data to the output stream.
    void flush(FILE* output) {
        if (m_pos > 0 && output) {
            fwrite(m_buf, 1, m_pos, output);
            fflush(output);
            m_pos = 0;
        }
    }

    bool is_enabled() const { return m_enabled; }

private:
    bool   m_enabled = false;
    char*  m_buf     = nullptr;
    size_t m_size    = 0;
    size_t m_pos     = 0;
};

} // namespace lumberjack

#endif // LUMBERJACK_UTILS_H
