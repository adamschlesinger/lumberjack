# Property-based tests using RapidCheck
include(FetchContent)

FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)

FetchContent_MakeAvailable(rapidcheck)

add_executable(test_log_level_ordering test_log_level_ordering.cpp)
target_link_libraries(test_log_level_ordering PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_log_level_gating test_log_level_gating.cpp)
target_link_libraries(test_log_level_gating PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_backend_roundtrip test_backend_roundtrip.cpp)
target_link_libraries(test_backend_roundtrip PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_builtin_output test_builtin_output.cpp)
target_link_libraries(test_builtin_output PRIVATE lumberjack::lumberjack)

add_executable(test_backend_message_delivery test_backend_message_delivery.cpp)
target_link_libraries(test_backend_message_delivery PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_span_callbacks test_span_callbacks.cpp)
target_link_libraries(test_span_callbacks PRIVATE lumberjack::lumberjack)

add_executable(test_span_lifecycle test_span_lifecycle.cpp)
target_link_libraries(test_span_lifecycle PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_span_level_gating test_span_level_gating.cpp)
target_link_libraries(test_span_level_gating PRIVATE lumberjack::lumberjack rapidcheck)

add_executable(test_thread_safety test_thread_safety.cpp)
target_link_libraries(test_thread_safety PRIVATE lumberjack::lumberjack rapidcheck)

enable_testing()
add_test(NAME LogLevelOrdering COMMAND test_log_level_ordering)
add_test(NAME LogLevelGating COMMAND test_log_level_gating)
add_test(NAME BackendRoundtrip COMMAND test_backend_roundtrip)
add_test(NAME BuiltinOutput COMMAND test_builtin_output)
add_test(NAME BackendMessageDelivery COMMAND test_backend_message_delivery)
add_test(NAME SpanCallbacks COMMAND test_span_callbacks)
add_test(NAME SpanLifecycle COMMAND test_span_lifecycle)
add_test(NAME SpanLevelGating COMMAND test_span_level_gating)
add_test(NAME ThreadSafety COMMAND test_thread_safety)

# Additional tests will be added in task 12
