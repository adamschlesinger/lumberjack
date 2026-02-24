# Implementation Plan: Lumberjack Logging Library

## Overview

This implementation plan breaks down the lumberjack logging library into incremental coding tasks. The library is a high-performance C++17 logging system with branchless dispatch through function pointer arrays, runtime-switchable backends, and optional RAII span timing. The implementation follows a bottom-up approach: core infrastructure first, then backends, then optional features, and finally examples and tests.

## Tasks

- [x] 1. Set up project structure and build system
  - Create directory structure: `include/lumberjack/`, `src/`, `examples/`, `tests/`
  - Create root `CMakeLists.txt` with library target `lumberjack::lumberjack`
  - Add build options: `lumberjack_BUILD_EXAMPLES`, `lumberjack_BUILD_TESTS`
  - Configure C++17 standard requirement
  - Set up install targets and CMake config file generation
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 8.7_

- [ ] 2. Implement core header with types and interfaces
  - [x] 2.1 Create `include/lumberjack/lumberjack.h` with namespace and enums
    - Define `lumberjack` namespace
    - Define `LogLevel` enum (NONE, ERROR, WARN, INFO, DEBUG, LOG_COUNT)
    - Define `LogBackend` struct with function pointers (init, shutdown, log_write, span_begin, span_end)
    - Declare public API functions: `init()`, `set_level()`, `get_level()`, `set_backend()`, `get_backend()`, `builtin_backend()`
    - Declare `LogFunction` type alias and extern `g_logFunctions` array
    - _Requirements: 1.1, 2.1, 3.1, 3.2, 5.4, 8.7, 15.1, 15.2, 15.3_

  - [x] 2.2 Write property test for LogLevel enum ordering
    - **Property: Log Level Ordering Invariant**
    - **Validates: Requirements 2.1**

- [x] 3. Implement logging macros
  - Create macros in `lumberjack.h`: `LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `LOG_DEBUG`
  - Each macro expands to `g_logFunctions[LOG_LEVEL_X](LOG_LEVEL_X, fmt, ##__VA_ARGS__)`
  - Ensure macros support printf-style format strings and variadic arguments
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.6, 1.2_

- [ ] 4. Implement core logger dispatch mechanism
  - [x] 4.1 Create `src/core.cpp` with global state
    - Define `g_logFunctions` array initialized to `log_noop`
    - Define `g_currentLevel` initialized to `LOG_LEVEL_INFO`
    - Define `g_activeBackend` pointer
    - Implement `log_noop()` function (empty, immediate return)
    - Implement `log_dispatch()` function (formats message, calls backend)
    - Implement `level_to_string()` helper function
    - _Requirements: 1.1, 1.3, 5.2, 5.3, 8.5_

  - [x] 4.2 Implement `set_level()` function
    - Update `g_logFunctions` array based on level threshold
    - For levels <= active level: point to `log_dispatch`
    - For levels > active level: point to `log_noop`
    - _Requirements: 1.4, 1.5, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7_

  - [x] 4.3 Implement `get_level()`, `set_backend()`, `get_backend()` functions
    - `get_level()`: return `g_currentLevel`
    - `set_backend()`: call shutdown on old backend, call init on new backend, update pointer
    - `get_backend()`: return `g_activeBackend`
    - _Requirements: 3.3, 3.4, 3.6, 7.3, 7.4_

  - [ ] 4.4 Implement `init()` function
    - Call `set_level(LOG_LEVEL_INFO)`
    - Call `set_backend(builtin_backend())`
    - _Requirements: 5.1, 5.2, 5.4, 5.5_

  - [ ]* 4.5 Write property test for log level gating
    - **Property 1: Log Level Gating**
    - **Validates: Requirements 1.3, 1.4, 2.3, 2.4, 2.5, 2.6, 2.7**

  - [ ]* 4.6 Write property test for backend round-trip consistency
    - **Property 2: Backend Round-Trip Consistency**
    - **Validates: Requirements 3.3, 3.6**

- [ ] 5. Checkpoint - Ensure core dispatch compiles
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Implement built-in backend
  - [ ] 6.1 Create `src/builtin.cpp` with backend implementation
    - Define static `g_output` (default stderr) and `g_mutex` for thread safety
    - Implement `builtin_init()` (no-op)
    - Implement `builtin_shutdown()` (reset to stderr)
    - Implement `builtin_log_write()` with timestamp formatting and mutex protection
    - Format: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message`
    - Use `std::chrono` for timestamps, `fprintf` for output
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.6, 11.2_

  - [ ] 6.2 Implement `builtin_backend()` accessor
    - Return pointer to static `LogBackend` struct with builtin function pointers
    - Set name field to "builtin"
    - _Requirements: 3.2, 4.2, 5.1_

  - [ ]* 6.3 Write unit tests for built-in backend output format
    - Test timestamp format
    - Test level string formatting
    - Test message content preservation
    - _Requirements: 4.4, 13.4_

  - [ ]* 6.4 Write property test for backend message delivery
    - **Property 4: Backend Message Delivery**
    - **Validates: Requirements 3.5, 7.5**

- [ ] 7. Implement RAII Span class (optional feature)
  - [ ] 7.1 Add Span class to `lumberjack.h`
    - Constructor: takes LogLevel and name, checks if level is active, calls backend span_begin
    - Destructor: calculates elapsed time, calls backend span_end
    - Use `std::chrono::steady_clock` for timing
    - Delete copy/move constructors and assignment operators
    - Store: level, name, handle, start time, active flag
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 14.5, 15.4_

  - [ ] 7.2 Create `LOG_SPAN` macro
    - Expand to: `lumberjack::Span _log_span_##__LINE__(level, name)`
    - _Requirements: 6.1, 10.5_

  - [ ] 7.3 Implement span callbacks in built-in backend
    - `builtin_span_begin()`: return nullptr (no tracking needed)
    - `builtin_span_end()`: format and log span message with elapsed time
    - _Requirements: 4.5, 6.2, 6.3_

  - [ ]* 7.4 Write property test for span lifecycle callbacks
    - **Property 5: Span Lifecycle Callbacks**
    - **Validates: Requirements 6.2, 6.3**

  - [ ]* 7.5 Write property test for span level gating
    - **Property 6: Span Level Gating**
    - **Validates: Requirements 6.5**

- [ ] 8. Checkpoint - Ensure core library is functional
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 9. Implement thread safety tests
  - [ ]* 9.1 Write property test for concurrent logging thread safety
    - **Property 7: Thread Safety of Concurrent Logging**
    - **Validates: Requirements 11.1, 11.2**
    - Create multiple threads calling logging macros concurrently
    - Verify no crashes, data races, or output corruption

  - [ ]* 9.2 Write unit tests for thread safety documentation
    - Test that set_level and set_backend are documented as not thread-safe
    - Verify documentation mentions custom backend thread safety responsibility
    - _Requirements: 11.3, 11.4, 11.5_

- [ ] 10. Implement property test for backend lifecycle sequencing
  - [ ]* 10.1 Write property test for backend lifecycle sequencing
    - **Property 3: Backend Lifecycle Sequencing**
    - **Validates: Requirements 3.4, 7.3, 7.4**
    - Test shutdown called before init on backend switch
    - Test init called before first log message

- [ ] 11. Create example programs
  - [ ] 11.1 Create `examples/basic.cpp`
    - Demonstrate simple logging with all log levels
    - Show level changes at runtime
    - Include explanatory comments
    - _Requirements: 12.1, 12.4, 12.5_

  - [ ] 11.2 Create `examples/spans.cpp`
    - Demonstrate RAII span timing
    - Show nested spans
    - Show span level gating
    - Include explanatory comments
    - _Requirements: 12.2, 12.4, 12.5_

  - [ ] 11.3 Create `examples/custom_backend.cpp`
    - Implement a simple custom backend (e.g., in-memory buffer)
    - Demonstrate backend registration and switching
    - Show backend lifecycle (init/shutdown)
    - Include explanatory comments
    - _Requirements: 12.3, 12.4, 12.5_

  - [ ] 11.4 Add examples to CMake build
    - Create `examples/CMakeLists.txt`
    - Conditionally build examples when `lumberjack_BUILD_EXAMPLES` is ON
    - Link examples against `lumberjack::lumberjack`
    - _Requirements: 9.3, 12.4_

- [ ] 12. Create comprehensive test suite
  - [ ] 12.1 Create `tests/test_levels.cpp`
    - Test level gating for all level combinations
    - Test LOG_LEVEL_NONE suppresses all output
    - Test level changes update function pointers correctly
    - _Requirements: 13.1_

  - [ ] 12.2 Create `tests/test_backend_swap.cpp`
    - Test backend switching functionality
    - Test init/shutdown call ordering
    - Test get_backend returns correct pointer
    - _Requirements: 13.2_

  - [ ] 12.3 Create `tests/test_spans.cpp`
    - Test span timing accuracy
    - Test span callback invocation
    - Test span level gating
    - _Requirements: 13.3_

  - [ ] 12.4 Create `tests/test_output.cpp`
    - Test output format validation
    - Test timestamp formatting
    - Test level string formatting
    - _Requirements: 13.4_

  - [ ] 12.5 Create `tests/test_threads.cpp`
    - Test concurrent logging from multiple threads
    - Verify no crashes or corruption
    - _Requirements: 13.5_

  - [ ] 12.6 Add tests to CMake build
    - Create `tests/CMakeLists.txt`
    - Conditionally build tests when `lumberjack_BUILD_TESTS` is ON
    - Link tests against `lumberjack::lumberjack` and Google Test
    - Enable CTest integration
    - _Requirements: 9.3, 13.6, 13.7_

- [ ] 13. Final checkpoint - Verify complete implementation
  - Ensure all tests pass, ask the user if questions arise.
  - Verify all examples compile and run
  - Verify CMake install target works correctly

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- The implementation follows a bottom-up approach: core infrastructure, backends, optional features, then examples and tests
- Property tests validate universal correctness properties across all inputs
- Unit tests validate specific examples, edge cases, and integration points
- Thread safety is critical: the built-in backend uses mutex protection, but set_level/set_backend are not thread-safe
- The branchless dispatch mechanism is the core innovation: function pointer arrays eliminate branching at call sites
