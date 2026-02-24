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

enable_testing()
add_test(NAME LogLevelOrdering COMMAND test_log_level_ordering)

# Additional tests will be added in task 12
