# Requirements Document

## Introduction

The pretty backend is a new built-in logging backend for the lumberjack library that provides human-friendly, colored output with hierarchical span visualization. Unlike the existing builtin backend which outputs plain timestamped text, the pretty backend emphasizes readability through ANSI color coding, automatic indentation for nested spans, and explicit start/end markers for span operations.

## Glossary

- **Backend**: A pluggable logging destination implementing the LogBackend interface
- **Pretty_Backend**: The new human-friendly logging backend with colored output
- **Span**: A timed code section tracked via span_begin() and span_end() callbacks
- **Log_Level**: Severity level (ERROR, WARN, INFO, DEBUG) controlling output verbosity
- **ANSI_Escape_Code**: Terminal control sequences for text coloring and formatting
- **Nesting_Depth**: The hierarchical level of a span within parent spans
- **Span_Handle**: An opaque pointer returned by span_begin() to track span state
- **Thread_Safety**: Guarantee that concurrent operations from multiple threads produce correct results

## Requirements

### Requirement 1: ANSI Color Output

**User Story:** As a developer, I want log messages to be color-coded by severity level, so that I can quickly identify important messages in terminal output.

#### Acceptance Criteria

1. WHEN a log message is written at ERROR level, THEN THE Pretty_Backend SHALL output the message with red ANSI color codes
2. WHEN a log message is written at WARN level, THEN THE Pretty_Backend SHALL output the message with yellow ANSI color codes
3. WHEN a log message is written at INFO level, THEN THE Pretty_Backend SHALL output the message with green ANSI color codes
4. WHEN a log message is written at DEBUG level, THEN THE Pretty_Backend SHALL output the message with blue ANSI color codes
5. WHEN outputting colored text, THEN THE Pretty_Backend SHALL reset color codes after each message to prevent color bleeding

### Requirement 2: Explicit Span Start and End Markers

**User Story:** As a developer, I want to see when spans begin and end, so that I can understand the execution flow and timing of nested operations.

#### Acceptance Criteria

1. WHEN span_begin() is called, THEN THE Pretty_Backend SHALL immediately output a "BEGIN" marker with the span name
2. WHEN span_end() is called, THEN THE Pretty_Backend SHALL output an "END" marker with the span name and elapsed time
3. WHEN outputting span markers, THEN THE Pretty_Backend SHALL use the same color coding as regular log messages based on the span's log level
4. WHEN a span begins, THEN THE Pretty_Backend SHALL include the span name in the BEGIN marker
5. WHEN a span ends, THEN THE Pretty_Backend SHALL include both the span name and elapsed microseconds in the END marker

### Requirement 3: Hierarchical Span Indentation

**User Story:** As a developer, I want nested spans to be visually indented, so that I can see the hierarchical structure of my code execution.

#### Acceptance Criteria

1. WHEN a span begins at nesting depth N, THEN THE Pretty_Backend SHALL indent the BEGIN marker by N*2 spaces
2. WHEN a span ends at nesting depth N, THEN THE Pretty_Backend SHALL indent the END marker by N*2 spaces
3. WHEN a log message is written inside a span at depth N, THEN THE Pretty_Backend SHALL indent the message by N*2 spaces
4. WHEN a span begins, THEN THE Pretty_Backend SHALL increment the nesting depth for subsequent operations in that span
5. WHEN a span ends, THEN THE Pretty_Backend SHALL decrement the nesting depth for subsequent operations

### Requirement 4: Per-Thread Span Tracking

**User Story:** As a developer, I want span nesting to work correctly in multi-threaded applications, so that logs from different threads don't interfere with each other.

#### Acceptance Criteria

1. WHEN multiple threads create spans concurrently, THEN THE Pretty_Backend SHALL track nesting depth independently per thread
2. WHEN a span begins on thread T, THEN THE Pretty_Backend SHALL store the span handle with thread T's identifier
3. WHEN a span ends on thread T, THEN THE Pretty_Backend SHALL retrieve the correct nesting depth for thread T
4. WHEN a span handle is created, THEN THE Pretty_Backend SHALL associate it with the current thread's span stack
5. WHEN a span ends, THEN THE Pretty_Backend SHALL remove it from the current thread's span stack

### Requirement 5: Thread-Safe Output

**User Story:** As a developer, I want log output to be thread-safe, so that concurrent logging doesn't produce corrupted or interleaved output.

#### Acceptance Criteria

1. WHEN multiple threads call log_write() concurrently, THEN THE Pretty_Backend SHALL serialize output to prevent interleaving
2. WHEN multiple threads call span_begin() concurrently, THEN THE Pretty_Backend SHALL safely update per-thread state without data races
3. WHEN multiple threads call span_end() concurrently, THEN THE Pretty_Backend SHALL safely update per-thread state without data races
4. WHEN accessing shared output resources, THEN THE Pretty_Backend SHALL use mutex protection
5. WHEN accessing per-thread span state, THEN THE Pretty_Backend SHALL use thread-local storage or equivalent mechanisms

### Requirement 6: Backend Lifecycle Management

**User Story:** As a developer, I want the pretty backend to properly initialize and clean up resources, so that it integrates correctly with the lumberjack lifecycle.

#### Acceptance Criteria

1. WHEN init() is called, THEN THE Pretty_Backend SHALL initialize any required internal state
2. WHEN shutdown() is called, THEN THE Pretty_Backend SHALL clean up all allocated resources
3. WHEN shutdown() is called, THEN THE Pretty_Backend SHALL clear all per-thread span tracking state
4. WHEN init() is called multiple times, THEN THE Pretty_Backend SHALL handle re-initialization safely
5. WHEN shutdown() is called without prior init(), THEN THE Pretty_Backend SHALL handle the call safely

### Requirement 7: Public API Integration

**User Story:** As a developer, I want to access the pretty backend through a simple function call, so that I can easily switch to it in my application.

#### Acceptance Criteria

1. THE Pretty_Backend SHALL be accessible via a pretty_backend() function in the lumberjack namespace
2. WHEN pretty_backend() is called, THEN THE System SHALL return a pointer to a LogBackend struct
3. THE LogBackend struct returned by pretty_backend() SHALL have the name field set to "pretty"
4. THE LogBackend struct SHALL have all required callback functions (init, shutdown, log_write, span_begin, span_end) properly initialized
5. WHEN set_backend(pretty_backend()) is called, THEN THE System SHALL switch to using the pretty backend for all subsequent logging

### Requirement 8: Human-Friendly Message Formatting

**User Story:** As a developer, I want log messages to be formatted in a clean, readable way, so that I can quickly understand log output without visual clutter.

#### Acceptance Criteria

1. WHEN formatting log messages, THEN THE Pretty_Backend SHALL use a simple format: "[LEVEL] message"
2. WHEN formatting span BEGIN markers, THEN THE Pretty_Backend SHALL use the format: "▶ BEGIN span_name"
3. WHEN formatting span END markers, THEN THE Pretty_Backend SHALL use the format: "◀ END span_name (elapsed_us μs)"
4. WHEN outputting messages, THEN THE Pretty_Backend SHALL NOT include timestamps (unlike the builtin backend)
5. WHEN outputting messages, THEN THE Pretty_Backend SHALL write to stderr by default

### Requirement 9: Span Handle Memory Management

**User Story:** As a developer, I want span handles to be properly managed, so that the backend doesn't leak memory or corrupt state.

#### Acceptance Criteria

1. WHEN span_begin() is called, THEN THE Pretty_Backend SHALL allocate a span handle containing nesting depth and thread information
2. WHEN span_end() is called, THEN THE Pretty_Backend SHALL deallocate the span handle
3. WHEN a span handle is allocated, THEN THE Pretty_Backend SHALL store the current nesting depth in the handle
4. WHEN a span handle is allocated, THEN THE Pretty_Backend SHALL store the parent span reference in the handle
5. WHEN span_end() is called with a valid handle, THEN THE Pretty_Backend SHALL restore the nesting depth from the handle before deallocation
