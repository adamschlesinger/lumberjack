# Requirements Document: Lumberjack Logging Library

## Introduction

Lumberjack is a high-performance, branchless logging library designed for performance-critical C++17 applications such as games and real-time systems. The library eliminates branching overhead at call sites through function pointer arrays indexed by log level, provides runtime-switchable backends, and supports RAII-based span timing for performance profiling.

## Glossary

- **Logger**: The core logging system that manages log levels, backends, and message dispatch
- **LogLevel**: Enumeration of logging severity levels (NONE, ERROR, WARN, INFO, DEBUG)
- **LogBackend**: Interface for pluggable logging output destinations
- **Builtin_Backend**: Default lightweight backend using fprintf with zero external dependencies
- **OTEL_Backend**: Optional OpenTelemetry backend for distributed tracing integration
- **Span**: RAII object for automatic timing measurement with begin/end callbacks
- **Function_Pointer_Array**: Array of function pointers indexed by log level for branchless dispatch
- **Hot_Path**: Performance-critical code path where logging occurs frequently

## Requirements

### Requirement 1: Branchless Logging Dispatch

**User Story:** As a game developer, I want logging calls to have minimal CPU overhead, so that I can leave debug logging in production code without performance impact.

#### Acceptance Criteria

1. THE Logger SHALL maintain function pointer arrays g_logFunctions[LOG_COUNT] indexed by LogLevel
2. WHEN a log message is emitted, THE Logger SHALL dispatch through function pointer array without conditional branching
3. WHEN the log level is set below a message's level, THE Logger SHALL point to no-op functions that return immediately
4. WHEN the log level is set at or above a message's level, THE Logger SHALL point to active dispatch functions
5. THE Logger SHALL update all function pointers when the log level changes

### Requirement 2: Runtime Log Level Management

**User Story:** As a developer, I want to change log verbosity at runtime, so that I can increase detail when debugging without recompiling.

#### Acceptance Criteria

1. THE Logger SHALL support five log levels: NONE, ERROR, WARN, INFO, DEBUG in ascending verbosity order
2. WHEN set_level is called with a valid LogLevel, THE Logger SHALL update the active log level immediately
3. WHEN the log level is set to NONE, THE Logger SHALL suppress all log output
4. WHEN the log level is set to ERROR, THE Logger SHALL emit only ERROR messages
5. WHEN the log level is set to WARN, THE Logger SHALL emit ERROR and WARN messages
6. WHEN the log level is set to INFO, THE Logger SHALL emit ERROR, WARN, and INFO messages
7. WHEN the log level is set to DEBUG, THE Logger SHALL emit all messages including DEBUG

### Requirement 3: Backend Abstraction and Swapping

**User Story:** As a developer, I want to switch logging backends at runtime, so that I can route logs to different destinations without restarting my application.

#### Acceptance Criteria

1. THE Logger SHALL define a LogBackend interface with function pointers for init, shutdown, log_write, span_begin, and span_end
2. THE LogBackend interface SHALL include a name field for backend identification
3. WHEN set_backend is called with a valid LogBackend pointer, THE Logger SHALL switch to the new backend immediately
4. WHEN switching backends, THE Logger SHALL call shutdown on the previous backend before calling init on the new backend
5. THE Logger SHALL call the active backend's log_write function for each emitted log message
6. WHEN get_backend is called, THE Logger SHALL return a pointer to the currently active backend

### Requirement 4: Built-in Lightweight Backend

**User Story:** As a developer, I want a zero-dependency logging backend, so that I can use the library without external dependencies.

#### Acceptance Criteria

1. THE Builtin_Backend SHALL use fprintf for output with no external dependencies beyond C standard library
2. WHEN builtin_backend is called, THE Logger SHALL return a pointer to the built-in backend instance
3. THE Builtin_Backend SHALL default to stderr for output
4. THE Builtin_Backend SHALL format messages with timestamp, log level, and message content
5. THE Builtin_Backend SHALL implement all LogBackend interface functions including span timing
6. THE Builtin_Backend SHALL use a mutex to ensure thread-safe writes from multiple threads

### Requirement 5: Library Initialization

**User Story:** As a developer, I want the library to be ready to use with sensible defaults, so that I can start logging immediately without complex setup.

#### Acceptance Criteria

1. THE Logger SHALL initialize with the built-in backend active by default
2. THE Logger SHALL initialize with log level set to INFO by default
3. THE Logger SHALL ensure all global state is initialized before first use
4. THE Logger SHALL provide an init function that can be called explicitly for guaranteed initialization
5. THE Logger SHALL be safe to use without calling init if static initialization is sufficient

### Requirement 6: RAII Span Support (Optional)

**User Story:** As a performance engineer, I want automatic timing of code sections, so that I can identify performance bottlenecks without manual timing code.

#### Acceptance Criteria

1. THE Logger SHALL provide a LOG_SPAN macro that creates an RAII Span object
2. WHEN a Span object is constructed, THE Logger SHALL call the backend's span_begin function
3. WHEN a Span object is destroyed, THE Logger SHALL call the backend's span_end function with elapsed time in microseconds
4. THE Span SHALL measure elapsed time from construction to destruction
5. THE Span SHALL respect the current log level and become a no-op when the level is insufficient
6. THE Span SHALL pass a SpanHandle from span_begin to span_end for backend correlation

### Requirement 7: Custom Backend Support

**User Story:** As a library user, I want to implement custom logging backends, so that I can integrate with proprietary logging systems.

#### Acceptance Criteria

1. THE Logger SHALL accept any LogBackend pointer that implements the required interface
2. THE LogBackend interface SHALL be defined in a public header file
3. WHEN a custom backend is registered, THE Logger SHALL call its init function before first use
4. WHEN a custom backend is unregistered, THE Logger SHALL call its shutdown function
5. THE Logger SHALL pass pre-formatted string messages to the backend's log_write function
6. THE Logger SHALL document the threading model and synchronization requirements for custom backends

### Requirement 8: C++17 Standard Compliance

**User Story:** As a C++ developer, I want the library to use modern C++ features, so that I can integrate it with contemporary codebases.

#### Acceptance Criteria

1. THE Logger SHALL compile with C++17 standard or later
2. THE Logger SHALL not require C++20 or later features
3. THE Logger SHALL not use exceptions in the hot path for performance
4. THE Logger SHALL not require RTTI (Run-Time Type Information)
5. THE Logger SHALL minimize STL allocations in the logging hot path
6. THE Logger SHALL use standard C++17 features for RAII, templates, and constexpr where appropriate
7. THE Logger SHALL place all types, functions, and constants within the lumberjack namespace

### Requirement 9: CMake Build System

**User Story:** As a build engineer, I want CMake-based builds, so that I can integrate the library into existing CMake projects.

#### Acceptance Criteria

1. THE Logger SHALL provide a root CMakeLists.txt for building the library
2. THE Logger SHALL create a lumberjack::lumberjack static library target
3. THE Logger SHALL provide build options lumberjack_BUILD_EXAMPLES and lumberjack_BUILD_TESTS
4. THE Logger SHALL install a CMake config file for find_package integration
5. THE Logger SHALL organize headers under include/lumberjack/ for proper include paths

### Requirement 10: Public API Macros

**User Story:** As a developer, I want convenient logging macros, so that I can log messages with minimal boilerplate.

#### Acceptance Criteria

1. THE Logger SHALL provide LOG_ERROR macro for error-level messages
2. THE Logger SHALL provide LOG_WARN macro for warning-level messages
3. THE Logger SHALL provide LOG_INFO macro for info-level messages
4. THE Logger SHALL provide LOG_DEBUG macro for debug-level messages
5. THE Logger SHALL provide LOG_SPAN macro for creating timed spans
6. THE logging macros SHALL support printf-style format strings and variadic arguments

### Requirement 11: Thread Safety

**User Story:** As a game developer with multiple threads (render, physics, AI, audio), I want thread-safe logging, so that all threads can log without corruption or crashes.

#### Acceptance Criteria

1. THE Logger SHALL support concurrent log message emission from multiple threads
2. THE Builtin_Backend SHALL use mutex protection to ensure thread-safe writes
3. THE Logger SHALL document that set_level and set_backend are not thread-safe and should be called during initialization or from a single thread
4. THE Logger SHALL document that custom backends are responsible for their own thread safety
5. THE Logger SHALL ensure that reading function pointers from g_logFunctions array is safe during concurrent logging

### Requirement 12: Example Programs

**User Story:** As a new user, I want example programs, so that I can quickly understand how to use the library.

#### Acceptance Criteria

1. THE Logger SHALL provide a basic.cpp example demonstrating simple logging
2. THE Logger SHALL provide a spans.cpp example demonstrating RAII span timing
3. THE Logger SHALL provide a custom_backend.cpp example demonstrating custom backend implementation
4. THE examples SHALL be buildable via CMake when lumberjack_BUILD_EXAMPLES is enabled
5. THE examples SHALL include comments explaining key concepts

### Requirement 13: Testing Suite

**User Story:** As a library maintainer, I want comprehensive tests, so that I can verify correctness and prevent regressions.

#### Acceptance Criteria

1. THE Logger SHALL provide tests for log level gating behavior
2. THE Logger SHALL provide tests for backend swapping functionality
3. THE Logger SHALL provide tests for span timing and callback invocation
4. THE Logger SHALL provide tests for output format validation
5. THE Logger SHALL provide tests for thread safety of concurrent logging
6. THE tests SHALL be buildable via CMake when lumberjack_BUILD_TESTS is enabled
7. THE tests SHALL use a standard C++ testing framework

### Requirement 14: Performance Characteristics

**User Story:** As a performance-critical application developer, I want predictable low overhead, so that logging doesn't cause frame drops or latency spikes.

#### Acceptance Criteria

1. WHEN a log message is below the active level, THE Logger SHALL execute only a function pointer call and immediate return
2. THE Logger SHALL not perform heap allocations in the hot path for disabled log levels
3. THE Logger SHALL not use virtual function dispatch for log level checks
4. THE Builtin_Backend SHALL minimize allocations and use stack buffers where possible
5. THE Span timing SHALL use high-resolution monotonic clock for accurate microsecond measurements

### Requirement 15: Header Organization

**User Story:** As a library user, I want clear header organization, so that I can include only what I need.

#### Acceptance Criteria

1. THE Logger SHALL provide lumberjack/lumberjack.h as the main header with core API
2. THE lumberjack.h header SHALL include LogBackend interface definition
3. THE lumberjack.h header SHALL include all logging macros and level management functions
4. THE lumberjack.h header SHALL include the Span class definition
5. THE lumberjack.h header SHALL be the single include needed for all core functionality
