# Lumberjack 🪓

A high-performance, branchless C++17 logging library designed for performance-critical applications like games and real-time systems.

## Features

- **Branchless Dispatch**: Function pointer arrays eliminate branching overhead at call sites
- **Runtime Log Levels**: Change verbosity on the fly without recompiling
- **Pluggable Backends**: Switch logging destinations at runtime
- **Zero Dependencies**: Built-in backend uses only C++ standard library
- **RAII Span Timing**: Automatic performance measurement with minimal code
- **Thread-Safe**: Built-in backend includes mutex protection for concurrent logging
- **Modern C++17**: Clean, idiomatic code with no exceptions or RTTI in hot paths

## Quick Start

```cpp
#include <lumberjack/lumberjack.h>

int main() {
    lumberjack::init();
    
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
[2026-02-23 21:35:28.605] [DEBUG] SPAN 'validation' took 33273 μs
[2026-02-23 21:35:28.646] [DEBUG] SPAN 'transformation' took 41208 μs
[2026-02-23 21:35:28.671] [INFO] SPAN 'process_data' took 154571 μs
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

The core innovation is branchless dispatch through function pointer arrays:

```cpp
// Traditional approach (branching)
if (level <= current_level) {
    format_and_log(level, fmt, args);
}

// Lumberjack approach (branchless)
g_logFunctions[level](level, fmt, args);  // Direct function pointer call
```

When logging is disabled for a level, the function pointer points to a no-op that returns immediately. This eliminates branch mispredictions and keeps the CPU pipeline flowing.

**Backend Contract**: All backends must provide valid (non-null) function pointers. This contract is enforced at configuration time (when calling `set_backend()`), eliminating all runtime checks in the hot path.

**Overhead when disabled**: ~1-2 CPU cycles (function pointer call + immediate return)  
**Runtime checks in hot path**: Zero - all function pointers guaranteed valid by contract

### Benchmark Results

Performance comparison against traditional branching logger with equivalent features (timestamps, mutex, formatting, flushing). 1,000,000 iterations on macOS:

| Scenario | Empty (no-op) | Lumberjack | Branching | Speedup |
|----------|---------------|------------|-----------|---------|
| Single disabled call | 15.7 ns | 14.6 ns | 15.0 ns | ~same |
| Single enabled call | - | 837 ns | 824 ns | ~same |
| Mixed workload (3 enabled + 2 disabled) | - | 2,456 ns | 2,429 ns | ~same |
| Tight loop (100 disabled calls) | 49.9 ns | 82.0 ns | 167.2 ns | 2.04x faster |
| Tight loop mixed (60 disabled + 40 enabled) | - | 31,982 ns | 31,510 ns | ~same |
| Tight loop (100 enabled calls) | - | 79,380 ns | 78,650 ns | ~same |

**Key Insights:**

The branchless design shines in tight loops with disabled logging - the most common pattern in performance-critical code:
- **Lumberjack overhead**: 32.1 ns for 100 calls = **0.32 ns per disabled log** (1.64x cost of compiled-out code)
- **Branching overhead**: 117.4 ns for 100 calls = **1.17 ns per disabled log** (3.35x cost of compiled-out code)
- **Result**: 2.04x faster and much closer to zero-cost

When logs are enabled, I/O dominates (~800 ns per log) and dispatch method doesn't matter. The branchless advantage appears specifically where it's needed: hot paths with disabled debug logging.

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
    ↓
Active Backend (builtin, custom, etc.)
    ↓
Output Destination (stderr, file, network, etc.)
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
