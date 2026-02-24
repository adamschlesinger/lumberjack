# Lumberjack 🪓

A high-performance C++17 logging library designed for performance-critical applications like games and real-time systems.

## Features

- **Branchless Dispatch**: Function pointer arrays eliminate branching overhead at disabled call sites
- **Buffered Writes**: Optional write buffering eliminates per-call fflush overhead (biggest perf win)
- **Cached Timestamps**: Amortizes localtime/strftime cost across rapid log calls
- **Branchless Spans**: Disabled spans skip clock reads via function pointer dispatch (~25 ns overhead)
- **Sequence Numbers**: Optional per-timestamp-interval counter restores log ordering resolution when using cached timestamps
- **Runtime Log Levels**: Change verbosity on the fly without recompiling
- **Pluggable Backends**: Switch logging destinations at runtime
- **RAII Span Timing**: Automatic performance measurement with minimal code
- **Thread-Safe**: Built-in backend includes mutex protection for concurrent logging
- **Zero Dependencies**: Built-in backend uses only C++ standard library
- **Modern C++17**: Clean, idiomatic code with no exceptions or RTTI in hot paths

## Quick Start

```cpp
#include <lumberjack/lumberjack.h>

int main() {
    lumberjack::init();
    
    // Enable high-performance mode
    lumberjack::builtin_set_buffered(true, 8192);
    lumberjack::builtin_set_timestamp_cache(10);
    
    // Or with sequence numbers for sub-timestamp ordering
    // lumberjack::builtin_set_timestamp_cache(10, true);
    
    LOG_INFO("Application started");
    LOG_ERROR("Something went wrong: %s", error_msg);
    LOG_DEBUG("Debug info: value=%d", 42);
    
    // Change log level at runtime
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
    
    // Measure performance with RAII spans
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "expensive_operation");
        // Your code here - timing is automatic
    }
    
    // Flush before exit (also happens automatically on shutdown)
    lumberjack::builtin_flush();
    return 0;
}
```

## Building

### Requirements

- CMake 3.15 or later
- C++17 compatible compiler
- RapidCheck (automatically fetched for property-based tests)

### Build Library

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Build with Examples

```bash
cmake -Dlumberjack_BUILD_EXAMPLES=ON ..
cmake --build .
./examples/example_basic
```

### Build with Tests

```bash
cmake -Dlumberjack_BUILD_TESTS=ON ..
cmake --build .
ctest
```

### Install

```bash
cmake --build . --target install
```

## Usage

### Basic Logging

```cpp
#include <lumberjack/lumberjack.h>

// Initialize with defaults (INFO level, stderr output)
lumberjack::init();

// Log at different levels
LOG_ERROR("Critical error: %s", message);
LOG_WARN("Warning: resource usage at %d%%", usage);
LOG_INFO("User %s logged in", username);
LOG_DEBUG("Cache hit rate: %.2f", hit_rate);
```

### Log Levels

```cpp
// Five log levels in ascending verbosity
lumberjack::LOG_LEVEL_NONE   // Suppress all output
lumberjack::LOG_LEVEL_ERROR  // Errors only
lumberjack::LOG_LEVEL_WARN   // Warnings and errors
lumberjack::LOG_LEVEL_INFO   // Informational (default)
lumberjack::LOG_LEVEL_DEBUG  // Verbose debug output

// Change level at runtime
lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
auto current = lumberjack::get_level();
```

### Performance Timing with Spans

```cpp
void process_data() {
    LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "process_data");
    
    // Nested spans work great
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "validation");
        validate_input();
    }
    
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "transformation");
        transform_data();
    }
    
    // Span automatically logs elapsed time on destruction
}
```

Output:
```
[2026-02-23 21:35:28] [DEBUG] SPAN 'validation' took 33273 us
[2026-02-23 21:35:28] [DEBUG] SPAN 'transformation' took 41208 us
[2026-02-23 21:35:28] [INFO ] SPAN 'process_data' took 154571 us
```

### Custom Backends

**Important**: All LogBackend function pointers must be non-null. Backends that don't need certain functionality should provide no-op implementations.

```cpp
// Define your backend - all function pointers must be valid
lumberjack::LogBackend my_backend = {
    .name = "my_backend",
    .init = []() { /* setup */ },
    .shutdown = []() { /* cleanup */ },
    .log_write = [](lumberjack::LogLevel level, const char* msg) {
        // Send to your logging system
    },
    .span_begin = [](lumberjack::LogLevel level, const char* name) -> void* {
        return nullptr; // Return handle for span tracking, or nullptr
    },
    .span_end = [](void* handle, lumberjack::LogLevel level, 
                   const char* name, long long elapsed_us) {
        // Handle span completion (even if you don't track spans, provide this)
    }
};

// Switch to your backend
lumberjack::set_backend(&my_backend);
```

The library validates all function pointers when you call `set_backend()` and will reject invalid backends. This contract enables truly branchless dispatch with zero runtime checks.

### CMake Integration

After installation, use `find_package` in your project:

```cmake
find_package(lumberjack REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE lumberjack::lumberjack)
```

## Performance

Lumberjack uses three techniques to minimize logging overhead:

1. **Branchless dispatch** — Function pointer arrays route disabled log calls to a no-op that returns immediately. No branch prediction, no pipeline stalls.
2. **Buffered writes** — Log lines accumulate in a memory buffer and flush in bulk, converting many small kernel writes into fewer large ones.
3. **Cached timestamps** — The formatted timestamp string is reused within a configurable interval, avoiding per-call `localtime()`/`strftime()`.

```cpp
// Traditional approach (branching, unbuffered, per-call timestamp)
if (level <= current_level) {
    format_timestamp();       // ~200 ns
    format_and_log(level, fmt, args);
    fflush(output);           // ~400 ns
}

// Lumberjack approach
g_logFunctions[level](level, fmt, args);  // noop if disabled, buffered if enabled
```

**Backend Contract**: All backends must provide valid (non-null) function pointers. This contract is enforced at configuration time (when calling `set_backend()`), eliminating all runtime checks in the hot path.

### Built-in Backend Optimizations

The built-in backend supports two optional optimizations that can be enabled at runtime:

```cpp
// Buffered writes — accumulate log lines in memory, flush when full or on demand
lumberjack::builtin_set_buffered(true, 8192);  // 8 KB buffer

// Cached timestamps — reuse formatted timestamp string within a time window
lumberjack::builtin_set_timestamp_cache(10); // refresh every 10 ms

// Cached timestamps with sequence numbers for sub-timestamp ordering
lumberjack::builtin_set_timestamp_cache(10, true);
// Output: [2026-02-24 10:15:03.042] [INFO ] #0 first message
//         [2026-02-24 10:15:03.042] [INFO ] #1 second message
//         [2026-02-24 10:15:03.042] [WARN ] #2 third message

// Manual flush when needed (also flushes on shutdown and output change)
lumberjack::builtin_flush();
```

All three optimizations are runtime-switchable and stack together.

### Benchmark Results

Performance comparison against a naive branching logger with equivalent base features (timestamps, mutex, formatting, flushing). 1,000,000 iterations on macOS:

**Disabled path (production hot path)**

| Scenario | Naive | Lumberjack | Speedup |
|----------|-------|------------|---------|
| Single disabled call | 15 ns | 15 ns | ~same (both near zero) |
| Disabled span | - | ~25 ns overhead | clock noop |
| Tight loop (100 disabled) | 158 ns | 80 ns | ~2x faster |

**Enabled path**

| Scenario | Naive | LJ Unbuffered | LJ Buf+Cache | Best Speedup |
|----------|-------|---------------|--------------|--------------|
| Single enabled call | 800 ns | 834 ns | 157 ns | 5.1x faster |
| 100 enabled calls | 76,136 ns | 81,864 ns | 12,998 ns | 5.9x faster |
| Mixed (3 en + 2 dis) | 2,339 ns | 2,460 ns | 413 ns | 5.7x faster |
| Enabled span | - | - | 218 ns | - |

**Sequence number overhead (buf+cache, 100 enabled calls)**

| Scenario | Mean | Per-call overhead |
|----------|------|-------------------|
| Seq OFF | 12,887 ns | — |
| Seq ON | 14,873 ns | ~20 ns |

**Key Insights:**

- Disabled logs are near zero-cost. The branchless design wins in tight loops (~2x) by avoiding branch overhead.
- Disabled spans cost ~25 ns thanks to clock function pointer dispatch (no `steady_clock::now()` calls).
- Unbuffered Lumberjack performs ~same as the naive logger (~0.96x) since I/O dominates.
- Buffered writes + cached timestamps deliver a 5-6x speedup by eliminating per-call `fflush()` and amortizing `localtime`/`strftime` cost.
- Sequence numbers add ~20 ns per call when enabled — negligible relative to the buf+cache baseline. Disabled by default.
- All optimizations are runtime-switchable — no recompilation needed.

Run the benchmark yourself:
```bash
./run_perf_test.sh
```

## Thread Safety

- **Concurrent logging**: Safe - the built-in backend uses mutex protection
- **Level changes**: Not thread-safe - call `set_level()` during initialization or from a single thread
- **Backend changes**: Not thread-safe - call `set_backend()` during initialization or from a single thread
- **Custom backends**: Responsible for their own thread safety

## Examples

Check out the `examples/` directory:

- `basic.cpp` - Simple logging with level changes
- `spans.cpp` - RAII span timing and nested spans
- `custom_backend.cpp` - Implementing a custom backend

## Testing

The library includes comprehensive tests:

- **Unit tests**: Specific scenarios and edge cases
- **Property-based tests**: Universal correctness properties verified across random inputs
- **Thread safety tests**: Concurrent logging from multiple threads

Run tests with:
```bash
cd build
ctest --output-on-failure
```

## Architecture

```
Application Code
    ↓
LOG_ERROR/WARN/INFO/DEBUG macros
    ↓
Function Pointer Array [branchless dispatch]
    ↓ (disabled → noop return)        ↓ (enabled)
    ↓                          Format message (vsnprintf)
    ↓                                 ↓
    ↓                          Active Backend
    ↓                                 ↓
    ↓                    ┌─ Buffered write (memcpy to buffer)
    ↓                    └─ Cached timestamp (reuse if fresh)
    ↓                                 ↓
  [done]                  Output (stderr, file, network, etc.)
```

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! The library follows these principles:

- Minimal overhead in hot paths
- No heap allocations for disabled log levels
- Clean, modern C++17 code
- Comprehensive test coverage

## Why "Lumberjack"?

Because it chops through logs efficiently! 🪓🌲
