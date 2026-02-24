#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

// Feature: lumberjack, Property 4: Backend Message Delivery
// Validates: Requirements 3.5, 7.5
// Property: For any log message that passes level gating, the active backend's
// log_write function SHALL be called with a pre-formatted string containing
// the message content.

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
    int log_write_call_count = 0;
    
    static void init() {}
    static void shutdown() {}
    
    static void log_write(lumberjack::LogLevel level, const char* message) {
        auto& instance = getInstance();
        instance.log_write_call_count++;
        instance.messages.push_back({level, std::string(message)});
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
        log_write_call_count = 0;
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
    
    std::cout << "Testing Property 4: Backend Message Delivery..." << std::endl;
    
    // Property: Messages that pass level gating are delivered to backend's log_write
    bool result = rc::check("Messages passing level gating are delivered to backend", []() {
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
        
        // Check if message was delivered to backend
        auto& instance = MockBackend::getInstance();
        bool wasDelivered = (instance.log_write_call_count > 0);
        
        // Message should be delivered if and only if messageLevel <= activeLevel
        bool shouldBeDelivered = (messageLevel <= activeLevel);
        
        RC_ASSERT(wasDelivered == shouldBeDelivered);
        
        // If delivered, verify the message content was passed correctly
        if (wasDelivered) {
            RC_ASSERT(instance.messages.size() == 1);
            RC_ASSERT(instance.messages[0].first == messageLevel);
            // The message should contain the original content
            RC_ASSERT(instance.messages[0].second == std::string(testMessage));
        }
    });
    
    if (!result) {
        std::cout << "FAILED: Messages passing level gating are delivered to backend" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Messages passing level gating are delivered to backend" << std::endl;
    }
    
    // Property: Backend receives pre-formatted messages with format string expansion
    std::cout << "Testing Property: Backend receives pre-formatted messages..." << std::endl;
    
    bool result2 = rc::check("Backend receives pre-formatted string messages", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Generate random values for format string
        auto intValue = *rc::gen::inRange(0, 1000);
        auto stringValue = *rc::gen::string<std::string>();
        
        // Log with format string
        LOG_INFO("Value: %d, String: %s", intValue, stringValue.c_str());
        
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == 1);
        
        // The backend should receive a pre-formatted string, not the format string
        std::string received = messages[0].second;
        
        // Verify the message contains the formatted values
        RC_ASSERT(received.find(std::to_string(intValue)) != std::string::npos);
        if (!stringValue.empty()) {
            RC_ASSERT(received.find(stringValue) != std::string::npos);
        }
        
        // Verify it doesn't contain the format specifiers
        RC_ASSERT(received.find("%d") == std::string::npos);
        RC_ASSERT(received.find("%s") == std::string::npos);
    });
    
    if (!result2) {
        std::cout << "FAILED: Backend receives pre-formatted string messages" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Backend receives pre-formatted string messages" << std::endl;
    }
    
    // Property: Multiple messages are all delivered
    std::cout << "Testing Property: Multiple messages are all delivered..." << std::endl;
    
    bool result3 = rc::check("Multiple messages are all delivered to backend", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Generate a random number of messages
        auto numMessages = *rc::gen::inRange(1, 11);
        
        // Log multiple messages
        for (int i = 0; i < numMessages; i++) {
            LOG_INFO("Message %d", i);
        }
        
        // All messages should be delivered
        auto& messages = MockBackend::getInstance().messages;
        RC_ASSERT(messages.size() == static_cast<size_t>(numMessages));
        
        // Verify each message was delivered correctly
        for (int i = 0; i < numMessages; i++) {
            RC_ASSERT(messages[i].first == lumberjack::LOG_LEVEL_INFO);
            RC_ASSERT(messages[i].second.find(std::to_string(i)) != std::string::npos);
        }
    });
    
    if (!result3) {
        std::cout << "FAILED: Multiple messages are all delivered to backend" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Multiple messages are all delivered to backend" << std::endl;
    }
    
    // Property: Messages below threshold are NOT delivered
    std::cout << "Testing Property: Messages below threshold are not delivered..." << std::endl;
    
    bool result4 = rc::check("Messages below threshold are not delivered", []() {
        MockBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_ERROR);
        
        // Try to log messages above ERROR level
        LOG_WARN("warn message");
        LOG_INFO("info message");
        LOG_DEBUG("debug message");
        
        // No messages should be delivered to backend
        RC_ASSERT(MockBackend::getInstance().log_write_call_count == 0);
        RC_ASSERT(MockBackend::getInstance().messages.empty());
    });
    
    if (!result4) {
        std::cout << "FAILED: Messages below threshold are not delivered" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Messages below threshold are not delivered" << std::endl;
    }
    
    return success ? 0 : 1;
}
