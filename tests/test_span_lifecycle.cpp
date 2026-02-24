#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <vector>
#include <string>

// Feature: lumberjack, Property 5: Span Lifecycle Callbacks
// Validates: Requirements 6.2, 6.3
// Property: For any Span object created at a sufficient log level,
// the backend's span_begin SHALL be called at construction and
// span_end SHALL be called at destruction.

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

// Mock backend to track span lifecycle calls
struct SpanTrackingBackend {
    struct SpanCall {
        enum Type { BEGIN, END };
        Type type;
        lumberjack::LogLevel level;
        std::string name;
        long long elapsed_us;  // Only for END calls
        void* handle;          // Only for END calls
    };
    
    std::vector<SpanCall> calls;
    int handleCounter = 0;
    
    static void init() {}
    static void shutdown() {}
    
    static void log_write(lumberjack::LogLevel level, const char* message) {}
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        auto& instance = getInstance();
        SpanCall call;
        call.type = SpanCall::BEGIN;
        call.level = level;
        call.name = std::string(name);
        call.elapsed_us = 0;
        call.handle = nullptr;
        instance.calls.push_back(call);
        
        // Return a unique handle
        instance.handleCounter++;
        return reinterpret_cast<void*>(static_cast<intptr_t>(instance.handleCounter));
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level, 
                        const char* name, long long elapsed_us) {
        auto& instance = getInstance();
        SpanCall call;
        call.type = SpanCall::END;
        call.level = level;
        call.name = std::string(name);
        call.elapsed_us = elapsed_us;
        call.handle = handle;
        instance.calls.push_back(call);
    }
    
    static SpanTrackingBackend& getInstance() {
        static SpanTrackingBackend instance;
        return instance;
    }
    
    void clear() {
        calls.clear();
        handleCounter = 0;
    }
};

static lumberjack::LogBackend g_spanTrackingBackend = {
    "span_tracking",
    SpanTrackingBackend::init,
    SpanTrackingBackend::shutdown,
    SpanTrackingBackend::log_write,
    SpanTrackingBackend::span_begin,
    SpanTrackingBackend::span_end
};

int main() {
    bool success = true;
    
    // Set up span tracking backend
    lumberjack::set_backend(&g_spanTrackingBackend);
    
    std::cout << "Testing Property 5: Span Lifecycle Callbacks..." << std::endl;
    
    // Property: Span objects at sufficient log level call span_begin and span_end
    bool result = rc::check("Span lifecycle calls span_begin and span_end", []() {
        auto activeLevel = *rc::gen::arbitrary<lumberjack::LogLevel>();
        auto spanLevel = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO,
            lumberjack::LOG_LEVEL_DEBUG
        );
        auto spanName = *rc::gen::string<std::string>();
        
        // Clear previous calls
        SpanTrackingBackend::getInstance().clear();
        
        // Set the active log level
        lumberjack::set_level(activeLevel);
        
        // Create and destroy a span
        {
            lumberjack::Span span(spanLevel, spanName.c_str());
        }
        
        auto& calls = SpanTrackingBackend::getInstance().calls;
        
        // If span level is sufficient, we should see BEGIN and END calls
        if (spanLevel <= activeLevel) {
            RC_ASSERT(calls.size() == 2);
            
            // First call should be BEGIN
            RC_ASSERT(calls[0].type == SpanTrackingBackend::SpanCall::BEGIN);
            RC_ASSERT(calls[0].level == spanLevel);
            RC_ASSERT(calls[0].name == spanName);
            
            // Second call should be END
            RC_ASSERT(calls[1].type == SpanTrackingBackend::SpanCall::END);
            RC_ASSERT(calls[1].level == spanLevel);
            RC_ASSERT(calls[1].name == spanName);
            
            // Elapsed time should be non-negative
            RC_ASSERT(calls[1].elapsed_us >= 0);
            
            // Handle from BEGIN should be passed to END
            // Note: We store the handle but don't compare void* directly due to RapidCheck limitations
            // The handle is verified in a separate test below
        } else {
            // If span level is insufficient, no callbacks should be made
            RC_ASSERT(calls.empty());
        }
    });
    
    if (!result) {
        std::cout << "FAILED: Span lifecycle calls span_begin and span_end" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Span lifecycle calls span_begin and span_end" << std::endl;
    }
    
    // Property: Multiple spans create multiple callback pairs
    std::cout << "Testing Property: Multiple spans create multiple callback pairs..." << std::endl;
    
    bool result2 = rc::check("Multiple spans create multiple callback pairs", []() {
        auto numSpans = *rc::gen::inRange(1, 6);
        
        SpanTrackingBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Create multiple spans sequentially
        for (int i = 0; i < numSpans; i++) {
            std::string name = "span_" + std::to_string(i);
            lumberjack::Span span(lumberjack::LOG_LEVEL_INFO, name.c_str());
        }
        
        auto& calls = SpanTrackingBackend::getInstance().calls;
        
        // Should have 2 calls per span (BEGIN and END)
        RC_ASSERT(calls.size() == static_cast<size_t>(numSpans * 2));
        
        // Verify each pair
        for (int i = 0; i < numSpans; i++) {
            int beginIdx = i * 2;
            int endIdx = i * 2 + 1;
            
            RC_ASSERT(calls[beginIdx].type == SpanTrackingBackend::SpanCall::BEGIN);
            RC_ASSERT(calls[endIdx].type == SpanTrackingBackend::SpanCall::END);
            
            // Names should match
            RC_ASSERT(calls[beginIdx].name == calls[endIdx].name);
            RC_ASSERT(calls[beginIdx].name == "span_" + std::to_string(i));
        }
    });
    
    if (!result2) {
        std::cout << "FAILED: Multiple spans create multiple callback pairs" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Multiple spans create multiple callback pairs" << std::endl;
    }
    
    // Property: Nested spans maintain proper ordering
    std::cout << "Testing Property: Nested spans maintain proper ordering..." << std::endl;
    
    bool result3 = rc::check("Nested spans maintain proper ordering", []() {
        SpanTrackingBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Create nested spans
        {
            lumberjack::Span outer(lumberjack::LOG_LEVEL_INFO, "outer");
            {
                lumberjack::Span inner(lumberjack::LOG_LEVEL_INFO, "inner");
            }
        }
        
        auto& calls = SpanTrackingBackend::getInstance().calls;
        
        // Should have 4 calls: outer_begin, inner_begin, inner_end, outer_end
        RC_ASSERT(calls.size() == 4);
        
        RC_ASSERT(calls[0].type == SpanTrackingBackend::SpanCall::BEGIN);
        RC_ASSERT(calls[0].name == "outer");
        
        RC_ASSERT(calls[1].type == SpanTrackingBackend::SpanCall::BEGIN);
        RC_ASSERT(calls[1].name == "inner");
        
        RC_ASSERT(calls[2].type == SpanTrackingBackend::SpanCall::END);
        RC_ASSERT(calls[2].name == "inner");
        
        RC_ASSERT(calls[3].type == SpanTrackingBackend::SpanCall::END);
        RC_ASSERT(calls[3].name == "outer");
    });
    
    if (!result3) {
        std::cout << "FAILED: Nested spans maintain proper ordering" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Nested spans maintain proper ordering" << std::endl;
    }
    
    // Property: Span handles are passed correctly from begin to end
    std::cout << "Testing Property: Span handles are passed correctly..." << std::endl;
    
    bool result4 = rc::check("Span handles are passed correctly from begin to end", []() {
        SpanTrackingBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        auto spanName = *rc::gen::string<std::string>();
        
        {
            lumberjack::Span span(lumberjack::LOG_LEVEL_INFO, spanName.c_str());
        }
        
        auto& calls = SpanTrackingBackend::getInstance().calls;
        
        RC_ASSERT(calls.size() == 2);
        
        // The handle returned by span_begin should be passed to span_end
        // In our mock, span_begin returns a non-null handle (counter value)
        // We verify by converting to intptr_t for comparison
        intptr_t beginHandle = reinterpret_cast<intptr_t>(calls[0].handle);
        intptr_t endHandle = reinterpret_cast<intptr_t>(calls[1].handle);
        
        // The mock backend doesn't set handle in BEGIN call, but END should receive
        // the handle returned by span_begin (which is 1 for the first span)
        RC_ASSERT(endHandle == 1);
    });
    
    if (!result4) {
        std::cout << "FAILED: Span handles are passed correctly from begin to end" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Span handles are passed correctly from begin to end" << std::endl;
    }
    
    return success ? 0 : 1;
}
