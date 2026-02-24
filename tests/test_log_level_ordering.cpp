#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>

// Feature: lumberjack, Property: Log Level Ordering Invariant
// Validates: Requirements 2.1
// Property: Log levels SHALL be ordered NONE < ERROR < WARN < INFO < DEBUG

namespace rc {
    template<>
    struct Arbitrary<lumberjack::LogLevel> {
        static Gen<lumberjack::LogLevel> arbitrary() {
            return gen::element(
                lumberjack::LOG_LEVEL_NONE,
                lumberjack::LOG_LEVEL_ERROR,
                lumberjack::LOG_LEVEL_WARN,
                lumberjack::LOG_LEVEL_INFO,
                lumberjack::LOG_LEVEL_DEBUG
            );
        }
    };
}

int main() {
    bool success = true;

    // Property 1: For any two distinct log levels, their enum values maintain ordering
    std::cout << "Testing Property: Log Level Ordering Invariant..." << std::endl;
    
    bool result = rc::check("Log levels are strictly ordered", []() {
        auto level1 = *rc::gen::arbitrary<lumberjack::LogLevel>();
        auto level2 = *rc::gen::arbitrary<lumberjack::LogLevel>();
        
        // If level1 is "less verbose" than level2, its numeric value should be smaller
        if (level1 == lumberjack::LOG_LEVEL_NONE && level2 != lumberjack::LOG_LEVEL_NONE) {
            RC_ASSERT(level1 < level2);
        }
        if (level1 == lumberjack::LOG_LEVEL_ERROR && 
            (level2 == lumberjack::LOG_LEVEL_WARN || 
             level2 == lumberjack::LOG_LEVEL_INFO || 
             level2 == lumberjack::LOG_LEVEL_DEBUG)) {
            RC_ASSERT(level1 < level2);
        }
        if (level1 == lumberjack::LOG_LEVEL_WARN && 
            (level2 == lumberjack::LOG_LEVEL_INFO || 
             level2 == lumberjack::LOG_LEVEL_DEBUG)) {
            RC_ASSERT(level1 < level2);
        }
        if (level1 == lumberjack::LOG_LEVEL_INFO && 
            level2 == lumberjack::LOG_LEVEL_DEBUG) {
            RC_ASSERT(level1 < level2);
        }
    });

    if (!result) {
        std::cout << "FAILED: Log levels are strictly ordered" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Log Level Ordering Invariant" << std::endl;
    }

    // Property 2: Verify specific ordering constraints
    bool result2 = rc::check("Specific level ordering constraints", []() {
        RC_ASSERT(lumberjack::LOG_LEVEL_NONE < lumberjack::LOG_LEVEL_ERROR);
        RC_ASSERT(lumberjack::LOG_LEVEL_ERROR < lumberjack::LOG_LEVEL_WARN);
        RC_ASSERT(lumberjack::LOG_LEVEL_WARN < lumberjack::LOG_LEVEL_INFO);
        RC_ASSERT(lumberjack::LOG_LEVEL_INFO < lumberjack::LOG_LEVEL_DEBUG);
        RC_ASSERT(lumberjack::LOG_LEVEL_DEBUG < lumberjack::LOG_COUNT);
    });

    if (!result2) {
        std::cout << "FAILED: Specific level ordering constraints" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Specific level ordering constraints" << std::endl;
    }

    return success ? 0 : 1;
}
