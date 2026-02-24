# Implementation Plan: Pretty Backend

## Overview

This plan implements the pretty backend for lumberjack in incremental steps, starting with basic structure and building up to full hierarchical span tracking with thread safety. Each step validates functionality through tests before proceeding.

## Tasks

- [x] 1. Create pretty backend file structure and basic scaffolding
  - Create `src/pretty.cpp` with namespace and includes
  - Define ANSI color code constants (COLOR_RESET, COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE)
  - Implement `level_to_color()` helper function to map LogLevel to color codes
  - Implement `level_to_string()` helper function to map LogLevel to string names
  - Implement stub functions for all LogBackend callbacks (init, shutdown, log_write, span_begin, span_end)
  - Implement `pretty_backend()` accessor function returning static LogBackend struct
  - Add `pretty_backend()` declaration to `include/lumberjack/lumberjack.h`
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 7.1, 7.2, 7.3, 7.4_

- [x] 1.1 Write unit tests for API integration
  - Test that `pretty_backend()` returns non-null pointer
  - Test that backend name field is "pretty"
  - Test that all callback function pointers are non-null
  - Test that `set_backend(pretty_backend())` successfully switches backend
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

- [ ] 2. Implement basic log_write with color coding
  - [ ] 2.1 Implement `pretty_log_write()` function
    - Add global mutex `g_output_mutex` for thread-safe output
    - Format output as "[LEVEL] message" without timestamps
    - Apply color codes based on log level using `level_to_color()`
    - Always append COLOR_RESET after message
    - Write to stderr with fflush
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 8.1, 8.4, 8.5_
  
  - [ ]* 2.2 Write property tests for color coding
    - **Property 1: Log Level Color Mapping**
    - **Validates: Requirements 1.1, 1.2, 1.3, 1.4**
  
  - [ ]* 2.3 Write property test for color reset
    - **Property 2: Color Reset After Output**
    - **Validates: Requirements 1.5**
  
  - [ ]* 2.4 Write property test for message format
    - **Property 10: Log Message Format**
    - **Validates: Requirements 8.1, 8.4**

- [ ] 3. Checkpoint - Ensure basic logging works
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 4. Implement span handle structure and basic span operations
  - [ ] 4.1 Define SpanHandle struct
    - Add fields: `int depth`, `SpanHandle* parent`, `std::thread::id thread_id`
    - _Requirements: 4.2, 9.1, 9.3, 9.4_
  
  - [ ] 4.2 Add thread-local span stack
    - Declare `thread_local std::vector<SpanHandle*> g_span_stack`
    - _Requirements: 4.1, 4.4, 4.5_
  
  - [ ] 4.3 Implement `pretty_span_begin()` function
    - Get current depth from `g_span_stack.size()`
    - Allocate new SpanHandle with current depth, parent pointer, and thread ID
    - Push handle to `g_span_stack`
    - Output BEGIN marker: "▶ BEGIN {name}" with color coding
    - Return handle pointer
    - _Requirements: 2.1, 2.4, 3.4, 8.2, 9.1_
  
  - [ ] 4.4 Implement `pretty_span_end()` function
    - Cast void* handle to SpanHandle*
    - Verify handle's thread_id matches current thread (defensive check)
    - Pop handle from `g_span_stack`
    - Output END marker: "◀ END {name} ({elapsed_us} μs)" with color coding at handle's depth
    - Delete handle
    - _Requirements: 2.2, 2.5, 3.5, 8.3, 9.2, 9.5_
  
  - [ ]* 4.5 Write property tests for span markers
    - **Property 3: Span BEGIN Marker Format**
    - **Validates: Requirements 2.1, 2.4**
  
  - [ ]* 4.6 Write property test for span END markers
    - **Property 4: Span END Marker Format**
    - **Validates: Requirements 2.2, 2.5**
  
  - [ ]* 4.7 Write property test for span color consistency
    - **Property 5: Span Marker Color Consistency**
    - **Validates: Requirements 2.3**
  
  - [ ]* 4.8 Write property test for span handle allocation
    - **Property 13: Span Handle Allocation**
    - **Validates: Requirements 9.1**
  
  - [ ]* 4.9 Write property test for BEGIN format pattern
    - **Property 11: Span BEGIN Format Pattern**
    - **Validates: Requirements 8.2**
  
  - [ ]* 4.10 Write property test for END format pattern
    - **Property 12: Span END Format Pattern**
    - **Validates: Requirements 8.3**

- [ ] 5. Checkpoint - Ensure basic spans work
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Implement hierarchical indentation
  - [ ] 6.1 Update `pretty_log_write()` to add indentation
    - Get current depth from `g_span_stack.size()`
    - Create indent string with `depth * 2` spaces
    - Prepend indent to output
    - _Requirements: 3.3_
  
  - [ ] 6.2 Update `pretty_span_begin()` to add indentation
    - Use handle's depth (before incrementing) for indentation
    - Create indent string with `depth * 2` spaces
    - Prepend indent to BEGIN marker
    - _Requirements: 3.1_
  
  - [ ] 6.3 Update `pretty_span_end()` to add indentation
    - Use handle's stored depth for indentation
    - Create indent string with `depth * 2` spaces
    - Prepend indent to END marker
    - _Requirements: 3.2_
  
  - [ ]* 6.4 Write property test for hierarchical indentation
    - **Property 6: Hierarchical Indentation**
    - **Validates: Requirements 3.1, 3.2, 3.3**
  
  - [ ]* 6.5 Write property test for span depth round-trip
    - **Property 7: Span Depth Round-Trip**
    - **Validates: Requirements 3.4, 3.5**

- [ ] 7. Implement lifecycle management
  - [ ] 7.1 Implement `pretty_init()` function
    - Add comment that thread-local storage is automatically initialized
    - No explicit initialization needed
    - _Requirements: 6.1_
  
  - [ ] 7.2 Implement `pretty_shutdown()` function
    - Iterate through `g_span_stack` and delete all remaining handles
    - Clear `g_span_stack`
    - _Requirements: 6.2, 6.3_
  
  - [ ]* 7.3 Write unit tests for lifecycle
    - Test that init() can be called successfully
    - Test that shutdown() clears span stack
    - Test that init() can be called multiple times safely
    - Test that shutdown() without init() doesn't crash (edge case)
    - _Requirements: 6.1, 6.3, 6.4, 6.5_

- [ ] 8. Implement thread safety and multi-threading support
  - [ ] 8.1 Verify mutex protection in all output operations
    - Ensure `g_output_mutex` is locked in `pretty_log_write()`
    - Ensure `g_output_mutex` is locked in `pretty_span_begin()` output section
    - Ensure `g_output_mutex` is locked in `pretty_span_end()` output section
    - _Requirements: 5.1, 5.4_
  
  - [ ]* 8.2 Write property test for thread-isolated span tracking
    - **Property 8: Thread-Isolated Span Tracking**
    - **Validates: Requirements 4.1, 4.2**
  
  - [ ]* 8.3 Write property test for thread-safe output serialization
    - **Property 9: Thread-Safe Output Serialization**
    - **Validates: Requirements 5.1**

- [ ] 9. Implement error handling and edge cases
  - [ ] 9.1 Add null handle check in `pretty_span_end()`
    - Return early if handle_ptr is null
    - _Requirements: Error Handling - Invalid Span Handle_
  
  - [ ] 9.2 Add cross-thread span detection in `pretty_span_end()`
    - Compare handle's thread_id with current thread
    - Log error message if mismatch, but don't crash
    - Return early without modifying span stack
    - _Requirements: Error Handling - Cross-Thread Span Ending_
  
  - [ ]* 9.3 Write unit tests for error handling
    - Test that span_end() with null handle doesn't crash
    - Test that ending span on wrong thread logs error but doesn't crash
    - Test that shutdown() with active spans cleans up properly
    - _Requirements: Error Handling sections_

- [ ] 10. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 11. Integration and documentation
  - [ ] 11.1 Add example usage to examples directory
    - Create `examples/pretty_backend.cpp` demonstrating:
      - Switching to pretty backend with `set_backend(pretty_backend())`
      - Colored log output at different levels
      - Nested spans with hierarchical indentation
      - Multi-threaded logging
    - Update `examples/CMakeLists.txt` to build the new example
    - _Requirements: All requirements (integration test)_
  
  - [ ] 11.2 Update build system
    - Add `src/pretty.cpp` to CMakeLists.txt if not automatically included
    - Verify library builds successfully with new backend
    - _Requirements: Build system integration_

- [ ] 12. Final verification checkpoint
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each property test should run minimum 100 iterations with randomized inputs
- Use Catch2's GENERATE feature for property-based testing
- Thread safety tests should create multiple threads (4-8) performing concurrent operations
- Output capture for tests can be done by redirecting stderr to a stringstream or file
- The SpanHandle struct should be defined in an anonymous namespace in pretty.cpp (not exposed in header)
