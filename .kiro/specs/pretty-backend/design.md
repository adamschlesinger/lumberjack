# Design Document: Pretty Backend

## Overview

The pretty backend is a new built-in logging backend for lumberjack that provides human-friendly, colored terminal output with hierarchical visualization of nested spans. It implements the LogBackend interface and will be exposed via a `pretty_backend()` accessor function similar to the existing `builtin_backend()`.

The key design challenge is tracking span nesting depth per-thread to enable proper indentation while maintaining thread safety. We'll use thread-local storage for per-thread span stacks and a mutex for protecting shared output resources.

## Architecture

The pretty backend follows the same architectural pattern as the builtin backend:

```
┌─────────────────────────────────────┐
│   lumberjack Core (core.cpp)       │
│   - Manages active backend pointer  │
│   - Dispatches to backend callbacks │
└──────────────┬──────────────────────┘
               │
               ├─────────────────┬──────────────────┐
               │                 │                  │
        ┌──────▼──────┐   ┌─────▼──────┐   ┌──────▼──────┐
        │   builtin   │   │   pretty   │   │   custom    │
        │  backend    │   │  backend   │   │  backends   │
        └─────────────┘   └────────────┘   └─────────────┘
```

The pretty backend will be implemented in `src/pretty.cpp` and exposed through `include/lumberjack/lumberjack.h`.

## Components and Interfaces

### 1. ANSI Color Codes

We'll define ANSI escape sequences as constants:

```cpp
namespace {
    // ANSI color codes
    const char* COLOR_RESET = "\033[0m";
    const char* COLOR_RED = "\033[31m";      // ERROR
    const char* COLOR_YELLOW = "\033[33m";   // WARN
    const char* COLOR_GREEN = "\033[32m";    // INFO
    const char* COLOR_BLUE = "\033[34m";     // DEBUG
}
```

Helper function to map log levels to colors:

```cpp
const char* level_to_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return COLOR_RED;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_INFO:  return COLOR_GREEN;
        case LOG_LEVEL_DEBUG: return COLOR_BLUE;
        default:              return COLOR_RESET;
    }
}
```

### 2. Span Handle Structure

Each span handle tracks its nesting depth and parent relationship:

```cpp
struct SpanHandle {
    int depth;              // Nesting depth when span was created
    SpanHandle* parent;     // Parent span (nullptr for root spans)
    std::thread::id thread_id;  // Thread that owns this span
};
```

### 3. Per-Thread Span Stack

We'll use thread-local storage to maintain a stack of active spans per thread:

```cpp
thread_local std::vector<SpanHandle*> g_span_stack;
```

This allows each thread to independently track its span nesting without interference.

### 4. Thread-Safe Output

A global mutex protects stderr writes:

```cpp
static std::mutex g_output_mutex;
```

All output operations (log_write, span_begin output, span_end output) acquire this mutex before writing.

### 5. Backend Implementation Functions

#### pretty_init()
```cpp
static void pretty_init() {
    // No global initialization needed
    // Thread-local storage is automatically initialized per-thread
}
```

#### pretty_shutdown()
```cpp
static void pretty_shutdown() {
    // Clean up any remaining spans in the current thread
    // (Defensive: spans should normally be balanced)
    for (auto* handle : g_span_stack) {
        delete handle;
    }
    g_span_stack.clear();
}
```

#### pretty_log_write()
```cpp
static void pretty_log_write(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(g_output_mutex);
    
    // Get current nesting depth
    int depth = g_span_stack.size();
    
    // Build indentation string
    std::string indent(depth * 2, ' ');
    
    // Get color for level
    const char* color = level_to_color(level);
    const char* level_str = level_to_string(level);
    
    // Output: [LEVEL] message (with color and indentation)
    fprintf(stderr, "%s%s[%s] %s%s\n", 
            indent.c_str(), color, level_str, message, COLOR_RESET);
    fflush(stderr);
}
```

#### pretty_span_begin()
```cpp
static void* pretty_span_begin(LogLevel level, const char* name) {
    // Get current depth before creating handle
    int current_depth = g_span_stack.size();
    
    // Create span handle
    SpanHandle* handle = new SpanHandle{
        current_depth,
        g_span_stack.empty() ? nullptr : g_span_stack.back(),
        std::this_thread::get_id()
    };
    
    // Push to stack (increases depth for nested operations)
    g_span_stack.push_back(handle);
    
    // Output BEGIN marker
    {
        std::lock_guard<std::mutex> lock(g_output_mutex);
        std::string indent(current_depth * 2, ' ');
        const char* color = level_to_color(level);
        fprintf(stderr, "%s%s▶ BEGIN %s%s\n", 
                indent.c_str(), color, name, COLOR_RESET);
        fflush(stderr);
    }
    
    return handle;
}
```

#### pretty_span_end()
```cpp
static void pretty_span_end(void* handle_ptr, LogLevel level, 
                            const char* name, long long elapsed_us) {
    if (!handle_ptr) return;
    
    SpanHandle* handle = static_cast<SpanHandle*>(handle_ptr);
    
    // Verify this span belongs to current thread (defensive)
    if (handle->thread_id != std::this_thread::get_id()) {
        // Log error but don't crash
        fprintf(stderr, "ERROR: Span ended on different thread than it began\n");
        return;
    }
    
    // Pop from stack
    if (!g_span_stack.empty() && g_span_stack.back() == handle) {
        g_span_stack.pop_back();
    }
    
    // Output END marker at the depth where span began
    {
        std::lock_guard<std::mutex> lock(g_output_mutex);
        std::string indent(handle->depth * 2, ' ');
        const char* color = level_to_color(level);
        fprintf(stderr, "%s%s◀ END %s (%lld μs)%s\n", 
                indent.c_str(), color, name, elapsed_us, COLOR_RESET);
        fflush(stderr);
    }
    
    // Clean up handle
    delete handle;
}
```

### 6. Backend Accessor

```cpp
LogBackend* pretty_backend() {
    static LogBackend backend = {
        "pretty",
        pretty_init,
        pretty_shutdown,
        pretty_log_write,
        pretty_span_begin,
        pretty_span_end
    };
    return &backend;
}
```

### 7. Public API Declaration

Add to `include/lumberjack/lumberjack.h`:

```cpp
// Backend accessors
LogBackend* builtin_backend();
LogBackend* pretty_backend();  // NEW
```

## Data Models

### SpanHandle
- `depth: int` - Nesting depth when span was created (0 for root spans)
- `parent: SpanHandle*` - Pointer to parent span (nullptr for root spans)
- `thread_id: std::thread::id` - Thread that created this span

### Thread-Local Span Stack
- Type: `std::vector<SpanHandle*>`
- Scope: Thread-local
- Purpose: Track active spans in current thread for depth calculation
- Lifecycle: Automatically initialized per-thread, cleared on shutdown

## Correctness Properties

A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.

### Property 1: Log Level Color Mapping

*For any* log level (ERROR, WARN, INFO, DEBUG) and any message, when logged at that level, the output SHALL contain the correct ANSI color code for that level (red for ERROR, yellow for WARN, green for INFO, blue for DEBUG).

**Validates: Requirements 1.1, 1.2, 1.3, 1.4**

### Property 2: Color Reset After Output

*For any* log message or span marker output, the output SHALL end with the ANSI reset code to prevent color bleeding into subsequent terminal output.

**Validates: Requirements 1.5**

### Property 3: Span BEGIN Marker Format

*For any* span name and log level, when span_begin() is called, the output SHALL contain "BEGIN" followed by the span name, with appropriate color coding for the level.

**Validates: Requirements 2.1, 2.4**

### Property 4: Span END Marker Format

*For any* span name and elapsed time, when span_end() is called, the output SHALL contain "END" followed by the span name and elapsed microseconds, with appropriate color coding.

**Validates: Requirements 2.2, 2.5**

### Property 5: Span Marker Color Consistency

*For any* span at a given log level, both the BEGIN and END markers SHALL use the same color code as regular log messages at that level.

**Validates: Requirements 2.3**

### Property 6: Hierarchical Indentation

*For any* nesting depth N (where N is the number of active parent spans), all output (log messages, BEGIN markers, END markers) SHALL be indented by exactly N*2 spaces.

**Validates: Requirements 3.1, 3.2, 3.3**

### Property 7: Span Depth Round-Trip

*For any* sequence of span operations, creating a span then ending it SHALL restore the nesting depth to its previous value (span_begin increments depth, span_end decrements depth).

**Validates: Requirements 3.4, 3.5**

### Property 8: Thread-Isolated Span Tracking

*For any* two threads T1 and T2, span operations on T1 SHALL NOT affect the nesting depth or span stack of T2 (each thread maintains independent span state).

**Validates: Requirements 4.1, 4.2**

### Property 9: Thread-Safe Output Serialization

*For any* concurrent log_write() or span operations from multiple threads, each complete output line SHALL appear atomically without interleaving of characters from different threads.

**Validates: Requirements 5.1**

### Property 10: Log Message Format

*For any* log message and level, the output format SHALL match the pattern: "[LEVEL] message" (without timestamps), where LEVEL is the string representation of the log level.

**Validates: Requirements 8.1, 8.4**

### Property 11: Span BEGIN Format Pattern

*For any* span name, the BEGIN marker SHALL match the pattern: "▶ BEGIN {span_name}" (using the right-pointing triangle character).

**Validates: Requirements 8.2**

### Property 12: Span END Format Pattern

*For any* span name and elapsed time, the END marker SHALL match the pattern: "◀ END {span_name} ({elapsed_us} μs)" (using the left-pointing triangle character and microsecond symbol).

**Validates: Requirements 8.3**

### Property 13: Span Handle Allocation

*For any* call to span_begin(), the returned handle SHALL be non-null and valid for use in the corresponding span_end() call.

**Validates: Requirements 9.1**

## Error Handling

### Invalid Span Handle
If span_end() is called with a null handle, the backend SHALL safely ignore the call without crashing.

### Cross-Thread Span Ending
If a span is ended on a different thread than it was created, the backend SHALL log an error message but not crash. This is a defensive check for user errors.

### Unbalanced Spans
If shutdown() is called while spans are still active, the backend SHALL clean up all remaining span handles to prevent memory leaks.

### Output Errors
If writing to stderr fails (e.g., file descriptor closed), the backend SHALL not crash but may lose log messages. This is acceptable as stderr failures are rare and typically indicate severe system issues.

## Testing Strategy

The pretty backend will be tested using both unit tests and property-based tests to ensure comprehensive coverage.

### Unit Testing Approach

Unit tests will focus on:
- **Lifecycle management**: Testing init(), shutdown(), and re-initialization scenarios
- **API integration**: Verifying pretty_backend() returns correct structure with all callbacks initialized
- **Edge cases**: Testing null handles, cross-thread span ending, shutdown with active spans
- **Output destination**: Verifying output goes to stderr

Unit tests will use output redirection to capture stderr and verify output contents.

### Property-Based Testing Approach

Property-based tests will verify universal correctness properties across randomized inputs:
- **Color mapping**: Generate random log levels and messages, verify correct color codes
- **Format patterns**: Generate random span names and messages, verify format compliance
- **Indentation**: Generate random span nesting hierarchies, verify indentation formula
- **Thread isolation**: Generate concurrent span operations across multiple threads, verify independence
- **Round-trip properties**: Verify span_begin/span_end depth restoration

### Property Test Configuration

- **Testing library**: We'll use Catch2 with its built-in GENERATE feature for property-based testing in C++
- **Iterations**: Each property test will run minimum 100 iterations with randomized inputs
- **Tagging**: Each property test will be tagged with a comment referencing its design property
- **Tag format**: `// Feature: pretty-backend, Property N: {property_text}`

### Test Organization

Tests will be organized in `tests/test_pretty_backend.cpp` with sections for:
- Lifecycle tests (unit tests)
- API tests (unit tests)
- Color and format tests (property tests)
- Indentation tests (property tests)
- Thread safety tests (property tests)
- Edge case tests (unit tests)

### Coverage Goals

- All 13 correctness properties must have corresponding property-based tests
- All edge cases identified in error handling must have unit tests
- Thread safety must be tested under concurrent load (multiple threads)
- Output format must be validated against exact patterns (no fuzzy matching)
