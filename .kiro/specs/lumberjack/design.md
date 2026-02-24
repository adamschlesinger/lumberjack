# Design Document: Lumberjack Logging Library

## Overview

Lumberjack is a high-performance C++17 logging library that eliminates branching overhead through function pointer dispatch. The core innovation is using arrays of function pointers indexed by log level, allowing the CPU to avoid branch prediction penalties at call sites. The library supports runtime-switchable backends through a simple interface, includes a zero-dependency built-in backend, and provides optional OpenTelemetry integration for distributed tracing.

The design prioritizes performance in the hot path (when logging is disabled) while maintaining flexibility for backend customization. RAII-based span timing provides automatic performance measurement with minimal code intrusion.

## Architecture

### Namespace Organization

All library components are contained within the `lumberjack` namespace to prevent naming conflicts and provide clear API boundaries. The public API is accessed through the namespace:

```cpp
namespace lumberjack {
    // All types, functions, and constants
}
```

Macros (LOG_ERROR, LOG_WARN, etc.) are defined in the global namespace for convenience but internally call lumberjack namespace functions.

### Initialization

The library uses static initialization for global state. All globals are initialized to safe default values:
- `g_logFunctions` array points to `log_noop` for all levels initially
- `g_currentLevel` defaults to `LOG_LEVEL_INFO`
- `g_activeBackend` points to the built-in backend

An optional `init()` function is provided for explicit initialization, but the library is safe to use immediately via static initialization. The `init()` function simply calls `set_level(LOG_LEVEL_INFO)` and `set_backend(builtin_backend())` to ensure proper setup.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Code                      │
│  LOG_ERROR(), LOG_WARN(), LOG_INFO(), LOG_DEBUG()       │
│  LOG_SPAN(), lumberjack::set_level(), set_backend()     │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│              Core Logger (core.cpp)                      │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Function Pointer Arrays                         │   │
│  │  g_logFunctions[LOG_COUNT]                       │   │
│  │  g_clockFunctions[LOG_COUNT]                     │   │
│  └─────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Active Backend Pointer                          │   │
│  │  g_activeBackend -> LogBackend*                  │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                  LogBackend Interface                    │
│  init(), shutdown(), log_write(),                       │
│  span_begin(), span_end()                               │
└────────────────────┬────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         ▼                       ▼
┌──────────────────┐  ┌──────────────────┐
│ Builtin Backend  │  │   Custom         │
│  (builtin.cpp)   │  │   Backend        │
│                  │  │  (user code)     │
│  fprintf-based   │  │                  │
│  Zero deps       │  │  User impl       │
│  Thread-safe     │  │                  │
└──────────────────┘  └──────────────────┘
```

### Branchless Dispatch Mechanism

The core performance optimization relies on function pointer arrays that are updated when the log level changes:

```cpp
namespace lumberjack {
    // Function pointer type
    using LogFunction = void (*)(LogLevel, const char*, ...);

    // Global array indexed by LogLevel enum
    LogFunction g_logFunctions[LOG_COUNT];

    // When level is below threshold: points to no-op
    void log_noop(LogLevel level, const char* fmt, ...) { }

    // When level is at or above threshold: points to active dispatch
    void log_dispatch(LogLevel level, const char* fmt, ...);
}

// Macro expands to direct function pointer call with level passed explicitly
#define LOG_ERROR(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_ERROR](lumberjack::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
```

**Example Expansion:**
```cpp
LOG_ERROR("Failed to load texture: %s", filename);
// Expands to:
lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_ERROR](lumberjack::LOG_LEVEL_ERROR, "Failed to load texture: %s", filename);
```

When `set_level()` is called, the function pointers are updated:

```cpp
namespace lumberjack {
    void set_level(LogLevel level) {
        for (int i = 0; i < LOG_COUNT; i++) {
            if (i <= level) {
                g_logFunctions[i] = log_dispatch;
            } else {
                g_logFunctions[i] = log_noop;
            }
        }
    }
}
```

This eliminates branching at call sites - the CPU simply calls through the function pointer without checking the log level. The LogLevel is passed directly to the dispatch function, eliminating any need for prefix parsing.

## Components and Interfaces

### Core Logger (core.cpp)

**Responsibilities:**
- Maintain function pointer arrays for branchless dispatch
- Manage active log level
- Manage active backend pointer
- Provide level and backend switching functions
- Format log messages before passing to backend
- Implement Span RAII class for timing

**Key Data Structures:**

```cpp
namespace lumberjack {
    enum LogLevel {
        LOG_LEVEL_NONE = 0,
        LOG_LEVEL_ERROR = 1,
        LOG_LEVEL_WARN = 2,
        LOG_LEVEL_INFO = 3,
        LOG_LEVEL_DEBUG = 4,
        LOG_COUNT = 5
    };

    struct LogBackend {
        const char* name;
        void (*init)();
        void (*shutdown)();
        void (*log_write)(LogLevel level, const char* message);
        void* (*span_begin)(LogLevel level, const char* name);
        void (*span_end)(void* handle, LogLevel level, const char* name, long long elapsed_us);
    };
}
```

**Public API Functions:**

```cpp
namespace lumberjack {
    // Level management
    void set_level(LogLevel level);
    LogLevel get_level();

    // Backend management
    void set_backend(LogBackend* backend);
    LogBackend* get_backend();

    // Backend accessors
    LogBackend* builtin_backend();
    
    // Initialization (optional, library uses safe defaults)
    void init();
}
```

**Internal Functions:**

```cpp
namespace lumberjack {
    // Dispatch functions
    void log_noop(LogLevel level, const char* fmt, ...);
    void log_dispatch(LogLevel level, const char* fmt, ...);

    // Helper functions
    const char* level_to_string(LogLevel level);
}
```

**Span RAII Class (Optional Feature):**

```cpp
namespace lumberjack {
    class Span {
    public:
        Span(LogLevel level, const char* name);
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
        bool m_active;  // false if level is insufficient
    };
}
```

### Built-in Backend (builtin.cpp)

**Responsibilities:**
- Provide zero-dependency logging using fprintf
- Format messages with timestamp, level, and content
- Support thread-safe writes using mutex protection
- Implement span timing with begin/end callbacks

**Implementation Details:**

```cpp
namespace lumberjack {
    // Global state
    static FILE* g_output = stderr;
    static std::mutex g_mutex;  // Optional, for thread safety

    // Backend implementation
    void builtin_init() {
        // No initialization needed
    }

    void builtin_shutdown() {
        if (g_output != stderr && g_output != stdout) {
            fclose(g_output);
            g_output = stderr;
        }
    }

    void builtin_log_write(LogLevel level, const char* message) {
        std::lock_guard<std::mutex> lock(g_mutex);
        
        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        // Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                      std::localtime(&time_t_now));
        
        const char* level_str = level_to_string(level);
        fprintf(g_output, "[%s.%03lld] [%s] %s\n", 
                timestamp, ms.count(), level_str, message);
        fflush(g_output);
    }

    SpanHandle builtin_span_begin(LogLevel level, const char* name) {
        // For built-in backend, we don't need to track spans
        // The Span class handles timing
        return nullptr;
    }

    void builtin_span_end(SpanHandle handle, LogLevel level, 
                         const char* name, long long elapsed_us) {
        char message[256];
        snprintf(message, sizeof(message), "SPAN '%s' took %lld μs", 
                 name, elapsed_us);
        builtin_log_write(level, message);
    }

    void builtin_set_output(FILE* file) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_output != stderr && g_output != stdout) {
            fclose(g_output);
        }
        g_output = file;
    }
}
```



**Implementation Details:**

```cpp
namespace lumberjack {
    // Global state
    static FILE* g_output = stderr;
    static std::mutex g_mutex;  // For thread-safe writes

    // Backend implementation
    void builtin_init() {
        // No initialization needed
    }

    void builtin_shutdown() {
        // Reset to stderr on shutdown
        std::lock_guard<std::mutex> lock(g_mutex);
        g_output = stderr;
    }

    void builtin_log_write(LogLevel level, const char* message) {
        std::lock_guard<std::mutex> lock(g_mutex);
        
        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        // Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                      std::localtime(&time_t_now));
        
        const char* level_str = level_to_string(level);
        fprintf(g_output, "[%s.%03lld] [%s] %s\n", 
                timestamp, ms.count(), level_str, message);
        fflush(g_output);
    }

    void* builtin_span_begin(LogLevel level, const char* name) {
        // For built-in backend, we don't need to track spans
        // The Span class handles timing
        return nullptr;
    }

    void builtin_span_end(void* handle, LogLevel level, 
                         const char* name, long long elapsed_us) {
        char message[256];
        snprintf(message, sizeof(message), "SPAN '%s' took %lld μs", 
                 name, elapsed_us);
        builtin_log_write(level, message);
    }
}
```

### Public Header Structure

**lumberjack/lumberjack.h:**
```cpp
#ifndef LUMBERJACK_H
#define LUMBERJACK_H

#include <cstdio>
#include <chrono>

namespace lumberjack {

// Log levels
enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_COUNT = 5
};

// Backend interface
struct LogBackend {
    const char* name;
    void (*init)();
    void (*shutdown)();
    void (*log_write)(LogLevel level, const char* message);
    void* (*span_begin)(LogLevel level, const char* name);
    void (*span_end)(void* handle, LogLevel level, const char* name, long long elapsed_us);
};

// Core API
void init();  // Optional explicit initialization
void set_level(LogLevel level);
LogLevel get_level();
void set_backend(LogBackend* backend);
LogBackend* get_backend();

// Backend accessors
LogBackend* builtin_backend();

// Span class (optional feature)
class Span {
public:
    Span(LogLevel level, const char* name);
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
    bool m_active;
};

// Internal function pointer array (exposed for macro use)
using LogFunction = void (*)(LogLevel, const char*, ...);
extern LogFunction g_logFunctions[LOG_COUNT];

} // namespace lumberjack

// Logging macros (global namespace for convenience)
#define LOG_ERROR(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_ERROR](lumberjack::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_WARN](lumberjack::LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_INFO](lumberjack::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    lumberjack::g_logFunctions[lumberjack::LOG_LEVEL_DEBUG](lumberjack::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_SPAN(level, name) lumberjack::Span _log_span_##__LINE__(level, name)

#endif // LUMBERJACK_H
```



## Data Models

### LogLevel Enumeration

```cpp
namespace lumberjack {
    enum LogLevel {
        LOG_LEVEL_NONE = 0,   // No logging
        LOG_LEVEL_ERROR = 1,  // Critical errors only
        LOG_LEVEL_WARN = 2,   // Warnings and errors
        LOG_LEVEL_INFO = 3,   // Informational messages
        LOG_LEVEL_DEBUG = 4,  // Verbose debug output
        LOG_COUNT = 5         // Total count for array sizing
    };
}
```

The enumeration values are ordered by verbosity, allowing simple comparison operations. The LOG_COUNT value is used to size the function pointer arrays.

### LogBackend Structure

```cpp
namespace lumberjack {
    struct LogBackend {
        const char* name;                    // Backend identifier (e.g., "builtin", "otel")
        void (*init)();                      // Called when backend is activated
        void (*shutdown)();                  // Called when backend is deactivated
        void (*log_write)(LogLevel level, const char* message);  // Write formatted message
        SpanHandle (*span_begin)(LogLevel level, const char* name);  // Start timing span
        void (*span_end)(SpanHandle handle, LogLevel level, const char* name, long long elapsed_us);  // End span
    };
}
```

All function pointers must be non-null. The backend is responsible for thread safety if needed.

### Span Class (Optional Feature)

```cpp
namespace lumberjack {
    class Span {
    public:
        Span(LogLevel level, const char* name)
            : m_level(level)
            , m_name(name)
            , m_handle(nullptr)
            , m_active(false)
        {
            // Check if this level is active
            if (level <= get_level()) {
                m_active = true;
                m_start = std::chrono::steady_clock::now();
                LogBackend* backend = get_backend();
                if (backend && backend->span_begin) {
                    m_handle = backend->span_begin(level, name);
                }
            }
        }
        
        ~Span() {
            if (m_active) {
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    end - m_start).count();
                
                LogBackend* backend = get_backend();
                if (backend && backend->span_end) {
                    backend->span_end(m_handle, m_level, m_name, elapsed);
                }
            }
        }
        
        Span(const Span&) = delete;
        Span& operator=(const Span&) = delete;
        Span(Span&&) = delete;
        Span& operator=(Span&&) = delete;
        
    private:
        LogLevel m_level;
        const char* m_name;
        void* m_handle;
        std::chrono::steady_clock::time_point m_start;
        bool m_active;
    };
}
```

### Global State

```cpp
// In core.cpp
namespace lumberjack {
    // Function pointer array for branchless dispatch
    LogFunction g_logFunctions[LOG_COUNT];
    
    // Active log level
    LogLevel g_currentLevel = LOG_LEVEL_INFO;
    
    // Active backend
    LogBackend* g_activeBackend = &g_builtinBackend;
    
    // Built-in backend instance
    LogBackend g_builtinBackend = {
        "builtin",
        builtin_init,
        builtin_shutdown,
        builtin_log_write,
        builtin_span_begin,
        builtin_span_end
    };
} // namespace lumberjack
```


## Correctness Properties

A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.

### Property 1: Log Level Gating

*For any* log level setting and any message with a specific level, the message SHALL be emitted if and only if the message level is less than or equal to the active log level.

**Validates: Requirements 1.3, 1.4, 2.3, 2.4, 2.5, 2.6, 2.7**

This property captures the core filtering behavior: ERROR (1) <= WARN (2) <= INFO (3) <= DEBUG (4). When the logger is set to WARN, only ERROR and WARN messages should appear. This should hold regardless of which backend is active.

### Property 2: Backend Round-Trip Consistency

*For any* valid LogBackend pointer, setting the backend via lumberjack::set_backend and then calling lumberjack::get_backend SHALL return the same backend pointer.

**Validates: Requirements 3.3, 3.6**

This ensures that backend switching works correctly and the logger maintains a consistent view of the active backend.

### Property 3: Backend Lifecycle Sequencing

*For any* sequence of backend switches, the logger SHALL call shutdown on the previous backend before calling init on the new backend, and SHALL call init on a custom backend before it receives any log messages.

**Validates: Requirements 3.4, 7.3, 7.4**

This property ensures proper resource management and initialization ordering. Backends can rely on init being called before use and shutdown being called when replaced.

### Property 4: Backend Message Delivery

*For any* log message that passes level gating, the active backend's log_write function SHALL be called with a pre-formatted string containing the message content.

**Validates: Requirements 3.5, 7.5**

This ensures that all messages that should be logged actually reach the backend, and that the logger handles formatting before passing to the backend.

### Property 5: Span Lifecycle Callbacks (Optional)

*For any* Span object created at a sufficient log level, the backend's span_begin SHALL be called at construction and span_end SHALL be called at destruction.

**Validates: Requirements 6.2, 6.3**

This tests the RAII pattern for spans and ensures backends receive proper lifecycle notifications.

### Property 6: Span Level Gating (Optional)

*For any* Span object created at a level above the active log level, no backend callbacks SHALL be invoked.

**Validates: Requirements 6.5**

This ensures spans respect log level filtering and don't incur overhead when disabled.

### Property 7: Thread Safety of Concurrent Logging

*For any* number of threads concurrently calling logging macros, the built-in backend SHALL produce output without corruption, data races, or crashes.

**Validates: Requirements 11.1, 11.2**

This ensures that the mutex protection in the built-in backend correctly serializes writes from multiple threads.

## Error Handling

### Invalid Inputs

**Null Backend Pointer:**
- Calling `lumberjack::set_backend(nullptr)` is undefined behavior
- The library will not check for null backends for performance reasons
- Users must ensure valid backend pointers

**Invalid Log Level:**
- Log levels outside the range [LOG_LEVEL_NONE, LOG_LEVEL_DEBUG] are undefined behavior
- The enum should be used directly to prevent invalid values

**Invalid FILE Pointer:**
- Calling `lumberjack::builtin_set_output(nullptr)` is undefined behavior
- Users must ensure FILE pointers are valid and open

### Backend Errors

**Backend Initialization Failure:**
- If a backend's init() function fails, it should handle the error internally
- The logger does not check return values from backend functions
- Backends should fail gracefully and log errors through alternative means

**Backend Write Failure:**
- If log_write() fails (e.g., disk full), the backend should handle it internally
- The logger will not retry or buffer messages
- Backends may choose to drop messages or use fallback mechanisms

### Resource Exhaustion

**Memory Allocation:**
- The core logger uses no dynamic allocation in the hot path
- Backends are responsible for their own memory management
- If a backend cannot allocate memory, it should fail gracefully

**File Descriptor Limits:**
- The built-in backend uses a single FILE pointer
- Users opening many log files should manage descriptor limits
- The library does not track or limit open files

### Thread Safety

**Concurrent Logging:**
- Multiple threads may call logging macros concurrently
- The built-in backend uses a mutex (`std::mutex g_mutex`) for thread-safe writes
- Custom backends must implement their own thread safety if needed

**Level and Backend Changes:**
- Calling `lumberjack::set_level()` concurrently with logging is not thread-safe
- Calling `lumberjack::set_backend()` concurrently with logging is not thread-safe
- Users should synchronize level/backend changes with application-level coordination
- Recommended pattern: change level/backend during initialization or when logging is paused

**Function Pointer Array Reads:**
- Reading from `g_logFunctions` array during logging is safe
- The array is only written during `set_level()` calls
- Users must ensure no concurrent `set_level()` calls occur during logging

## Testing Strategy

### Dual Testing Approach

The lumberjack library will use both unit tests and property-based tests for comprehensive coverage:

**Unit Tests:**
- Specific examples demonstrating correct behavior
- Edge cases (e.g., LOG_LEVEL_NONE suppresses all output)
- Integration between components
- Error conditions and boundary cases
- Backend-specific behavior

**Property-Based Tests:**
- Universal properties that hold across all inputs
- Level gating behavior across all level combinations
- Backend switching with various backends
- Span timing with random durations
- Format string handling with generated inputs

Together, these approaches provide comprehensive coverage: unit tests catch concrete bugs and verify specific scenarios, while property tests verify general correctness across a wide input space.

### Property-Based Testing Configuration

**Framework Selection:**
- Use RapidCheck for C++ property-based testing
- RapidCheck provides generators for built-in types and composable generators for custom types
- Integrates with standard C++ testing frameworks (Google Test, Catch2)

**Test Configuration:**
- Each property test SHALL run a minimum of 100 iterations
- Use random seeds for reproducibility
- Each test SHALL reference its design document property via comment
- Tag format: `// Feature: lumberjack, Property N: [property text]`

**Generator Strategy:**
- LogLevel generator: produces random values from LOG_LEVEL_NONE to LOG_LEVEL_DEBUG
- Message generator: produces random strings with various lengths and characters
- Format string generator: produces valid printf-style format strings with matching arguments
- Backend generator: produces mock backends with configurable behavior

### Unit Testing Strategy

**Test Organization:**
- `test_levels.cpp`: Level gating, level changes, LOG_LEVEL_NONE edge case
- `test_backend_swap.cpp`: Backend switching, init/shutdown sequencing, get/set round-trip
- `test_spans.cpp`: Span lifecycle, timing accuracy, level gating for spans (optional feature)
- `test_output.cpp`: Format validation, output capture
- `test_threads.cpp`: Thread safety of concurrent logging

**Testing Framework:**
- Use Google Test for unit testing
- Provides clear test organization, assertions, and test fixtures
- Wide adoption and good CMake integration

**Mock Backends:**
- Create mock backends that track calls to init, shutdown, log_write, span_begin, span_end
- Use call counters and argument capture for verification
- Provide configurable behavior for error testing

### Integration Testing

**Example Programs as Tests:**
- Each example program should compile and run successfully
- Examples serve as integration tests for real-world usage patterns
- Automated testing can verify examples produce expected output

**CMake Testing:**
- Use CTest for test execution
- Separate test targets for unit tests and property tests
- Conditional tests for OTEL backend when available

### Performance Testing

While not part of automated testing, performance characteristics should be manually verified:

**Benchmarks:**
- Measure overhead of disabled log calls (should be ~1-2 CPU cycles)
- Measure throughput of enabled logging with various backends
- Measure span timing overhead
- Compare against virtual dispatch and branch-based alternatives

**Profiling:**
- Verify no heap allocations in hot path for disabled logs
- Verify function pointer dispatch has no branch mispredictions
- Measure backend-specific performance characteristics
