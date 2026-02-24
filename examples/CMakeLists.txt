# Examples for lumberjack logging library
# These examples demonstrate various features of the library

# Basic logging example - demonstrates all log levels and runtime level changes
add_executable(example_basic basic.cpp)
target_link_libraries(example_basic PRIVATE lumberjack::lumberjack)

# Span timing example - demonstrates RAII-based performance measurement
add_executable(example_spans spans.cpp)
target_link_libraries(example_spans PRIVATE lumberjack::lumberjack)

# Custom backend example - demonstrates backend implementation and switching
add_executable(example_custom_backend custom_backend.cpp)
target_link_libraries(example_custom_backend PRIVATE lumberjack::lumberjack)
