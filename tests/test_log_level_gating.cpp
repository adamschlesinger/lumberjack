#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

// Feature: lumberjack, Property 1: Log Level Gating
// Validates: Requirements 1.3, 1.4, 2.3, 2.4, 2.5, 2.6, 2.7
// Property: For any log level setting and any message with a specific level,
// the message SHALL be emitted if and only if the message level is less than
// or equal to the active log level.

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

// Mock backend to track log_write calls
struct MockBackend {
    std::vector<std::pair<lumberjack::LogLevel, std::string>> messages;
    
    static void init() {}
    static void shutdown() {}
    
    static void log_write(lumberjack::LogLevel level, const char* message) {
        getInstance().messages.push_back({level, std::string(message)});
    }
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        return nullptr;
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level, 
                        const char* name, long long elapsed_us) {}
    
    static MockBackend& getInstance() {
        static MockBackend instance;
        return instance;
    }
    
    void clear() {
        messages.clear();
    }
};

static lumberjack::LogBackend g_mockBackend = {
    "mock",
    MockBackend::init,
    MockBackend::shutdown,
    MockBackend::log_write,
    MockBackend::span_begin,
    MockBackend::span_end
};

int main() {
    bool success = true;
    
    // Set up mock backend
    lumberjack::set_backend(&g_mockBackend);
    
    std::cout << "Testing Property 1: Log Level Gating..." << std::endl;
    
    // Property: Messages are emitted if and only if message level <= active level
    bool result = rc::check("Log level gating filters messages correctly", []() {
        auto activeLevel = *rc::gen::arbitrary<lumberjack::LogLevel>();
        // Message levels should only be ERROR, WARN, INFO, or DEBUG (not NONE)
        auto messageLevel = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO,
            lumberjack::LOG_LEVEL_DEBUG
        );
        
        // Clear previous messages
        MockBackend::getInstance().clear();
        
        // Set the active log level
        lumberjack::set_level(activeLevel);
        
        // Emit a message at messageLevel
        const char* testMessage = "test message";
        lumberjack::g_logFunctions[messageLevel](messageLevel, testMessage);
        
        // Check if message was logged
        auto& messages = MockBackend::getInstance().messages;
        bool wasLogged = !messages.empty();
        
        // Message should be logged if and only if messageLevel <= activeLevel
        bool shouldBeLogged = (messageLevel <= activeLevel);
        
        RC_ASSERT(wasLogged == shouldBeLogged);
        
        // If logged, verify it was logged at the correct level
        if (wasLogged) {
            RC_ASSERT(messages.size() == 1);
            RC_ASSERT(messages[0].first == messageLevel);
            RC_ASSERT(messages[0].second == std::string(testMessage));
        }
    });
    
    if (!result) {
        std::cout << "FAILED: Log level gating filters messages correctly" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Log Level Gating" << std::endl;
    }
    
    // Additional property: LOG_LEVEL_NONE suppresses all output
    std::cout << "Testing Property: LOG_LEVEL_NONE suppresses all output..." << std::endl;
    
    bool result2 = rc::check("LOG_LEVEL_NONE suppresses all messages", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_NONE);
        
        // Try to log at all levels
        LOG_ERROR("error message");
        LOG_WARN("warn message");
        LOG_INFO("info message");
        LOG_DEBUG("debug message");
        
        // No messages should be logged
        RC_ASSERT(MockBackend::getInstance().messages.empty());
    });
    
    if (!result2) {
        std::cout << "FAILED: LOG_LEVEL_NONE suppresses all messages" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: LOG_LEVEL_NONE suppresses all output" << std::endl;
    }
    
    // Property: Verify specific level combinations
    std::cout << "Testing Property: Specific level combinations..." << std::endl;
    
    bool result3 = rc::check("ERROR level only logs ERROR", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_ERROR);
        
        LOG_ERROR("error");
        LOG_WARN("warn");
        LOG_INFO("info");
        LOG_DEBUG("debug");
        
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == 1);
        RC_ASSERT(messages[0].first == lumberjack::LOG_LEVEL_ERROR);
    });
    
    bool result4 = rc::check("WARN level logs ERROR and WARN", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_WARN);
        
        LOG_ERROR("error");
        LOG_WARN("warn");
        LOG_INFO("info");
        LOG_DEBUG("debug");
        
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == 2);
        RC_ASSERT(messages[0].first == lumberjack::LOG_LEVEL_ERROR);
        RC_ASSERT(messages[1].first == lumberjack::LOG_LEVEL_WARN);
    });
    
    bool result5 = rc::check("INFO level logs ERROR, WARN, and INFO", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
        
        LOG_ERROR("error");
        LOG_WARN("warn");
        LOG_INFO("info");
        LOG_DEBUG("debug");
        
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == 3);
        RC_ASSERT(messages[0].first == lumberjack::LOG_LEVEL_ERROR);
        RC_ASSERT(messages[1].first == lumberjack::LOG_LEVEL_WARN);
        RC_ASSERT(messages[2].first == lumberjack::LOG_LEVEL_INFO);
    });
    
    bool result6 = rc::check("DEBUG level logs all messages", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        LOG_ERROR("error");
        LOG_WARN("warn");
        LOG_INFO("info");
        LOG_DEBUG("debug");
        
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == 4);
        RC_ASSERT(messages[0].first == lumberjack::LOG_LEVEL_ERROR);
        RC_ASSERT(messages[1].first == lumberjack::LOG_LEVEL_WARN);
        RC_ASSERT(messages[2].first == lumberjack::LOG_LEVEL_INFO);
        RC_ASSERT(messages[3].first == lumberjack::LOG_LEVEL_DEBUG);
    });
    
    if (!result3 || !result4 || !result5 || !result6) {
        std::cout << "FAILED: Specific level combinations" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Specific level combinations" << std::endl;
    }
    
    return success ? 0 : 1;
}
