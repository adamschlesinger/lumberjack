#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <vector>
#include <string>

// Feature: lumberjack, Property 6: Span Level Gating
// Validates: Requirements 6.5
// Property: For any Span object created at a level above the active log level,
// no backend callbacks SHALL be invoked.

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

// Mock backend to track span callback invocations
struct SpanGatingBackend {
    struct SpanCall {
        enum Type { BEGIN, END };
        Type type;
        lumberjack::LogLevel level;
        std::string name;
    };
    
    std::vector<SpanCall> calls;
    
    static void init() {}
    static void shutdown() {}
    
    static void log_write(lumberjack::LogLevel level, const char* message) {}
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        auto& instance = getInstance();
        SpanCall call;
        call.type = SpanCall::BEGIN;
        call.level = level;
        call.name = std::string(name);
        instance.calls.push_back(call);
        return reinterpret_cast<void*>(1);  // Return dummy handle
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level, 
                        const char* name, long long elapsed_us) {
        auto& instance = getInstance();
        SpanCall call;
        call.type = SpanCall::END;
        call.level = level;
        call.name = std::string(name);
        instance.calls.push_back(call);
    }
    
    static SpanGatingBackend& getInstance() {
        static SpanGatingBackend instance;
        return instance;
    }
    
    void clear() {
        calls.clear();
    }
};

static lumberjack::LogBackend g_spanGatingBackend = {
    "span_gating",
    SpanGatingBackend::init,
    SpanGatingBackend::shutdown,
    SpanGatingBackend::log_write,
    SpanGatingBackend::span_begin,
    SpanGatingBackend::span_end
};

int main() {
    bool success = true;
    
    // Set up span gating backend
    lumberjack::set_backend(&g_spanGatingBackend);
    
    std::cout << "Testing Property 6: Span Level Gating..." << std::endl;
    
    // Property: Spans above active level invoke no callbacks
    bool result = rc::check("Spans above active level invoke no callbacks", []() {
        auto activeLevel = *rc::gen::arbitrary<lumberjack::LogLevel>();
        auto spanLevel = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO,
            lumberjack::LOG_LEVEL_DEBUG
        );
        auto spanName = *rc::gen::string<std::string>();
        
        // Clear previous calls
        SpanGatingBackend::getInstance().clear();
        
        // Set the active log level
        lumberjack::set_level(activeLevel);
        
        // Create and destroy a span
        {
            lumberjack::Span span(spanLevel, spanName.c_str());
        }
        
        auto& calls = SpanGatingBackend::getInstance().calls;
        
        // If span level is above active level, no callbacks should be invoked
        if (spanLevel > activeLevel) {
            RC_ASSERT(calls.empty());
        } else {
            // If span level is at or below active level, callbacks should be invoked
            RC_ASSERT(calls.size() == 2);
            RC_ASSERT(calls[0].type == SpanGatingBackend::SpanCall::BEGIN);
            RC_ASSERT(calls[1].type == SpanGatingBackend::SpanCall::END);
        }
    });
    
    if (!result) {
        std::cout << "FAILED: Spans above active level invoke no callbacks" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Spans above active level invoke no callbacks" << std::endl;
    }
    
    // Property: LOG_LEVEL_NONE suppresses all span callbacks
    std::cout << "Testing Property: LOG_LEVEL_NONE suppresses all span callbacks..." << std::endl;
    
    bool result2 = rc::check("LOG_LEVEL_NONE suppresses all span callbacks", []() {
        auto spanLevel = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO,
            lumberjack::LOG_LEVEL_DEBUG
        );
        auto spanName = *rc::gen::string<std::string>();
        
        SpanGatingBackend::getInstance().clear();
        lumberjack::set_level(lumberjack::LOG_LEVEL_NONE);
        
        {
            lumberjack::Span span(spanLevel, spanName.c_str());
        }
        
        auto& calls = SpanGatingBackend::getInstance().calls;
        
        // LOG_LEVEL_NONE should suppress all spans
        RC_ASSERT(calls.empty());
    });
    
    if (!result2) {
        std::cout << "FAILED: LOG_LEVEL_NONE suppresses all span callbacks" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: LOG_LEVEL_NONE suppresses all span callbacks" << std::endl;
    }
    
    // Property: Span level exactly matching active level invokes callbacks
    std::cout << "Testing Property: Span level exactly matching active level invokes callbacks..." << std::endl;
    
    bool result3 = rc::check("Span level exactly matching active level invokes callbacks", []() {
        auto level = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO,
            lumberjack::LOG_LEVEL_DEBUG
        );
        auto spanName = *rc::gen::string<std::string>();
        
        SpanGatingBackend::getInstance().clear();
        lumberjack::set_level(level);
        
        {
            lumberjack::Span span(level, spanName.c_str());
        }
        
        auto& calls = SpanGatingBackend::getInstance().calls;
        
        // Exact match should invoke callbacks
        RC_ASSERT(calls.size() == 2);
        RC_ASSERT(calls[0].type == SpanGatingBackend::SpanCall::BEGIN);
        RC_ASSERT(calls[0].level == level);
        RC_ASSERT(calls[1].type == SpanGatingBackend::SpanCall::END);
        RC_ASSERT(calls[1].level == level);
    });
    
    if (!result3) {
        std::cout << "FAILED: Span level exactly matching active level invokes callbacks" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Span level exactly matching active level invokes callbacks" << std::endl;
    }
    
    // Property: Multiple spans respect level gating independently
    std::cout << "Testing Property: Multiple spans respect level gating independently..." << std::endl;
    
    bool result4 = rc::check("Multiple spans respect level gating independently", []() {
        auto activeLevel = *rc::gen::element(
            lumberjack::LOG_LEVEL_ERROR,
            lumberjack::LOG_LEVEL_WARN,
            lumberjack::LOG_LEVEL_INFO
        );
        
        SpanGatingBackend::getInstance().clear();
        lumberjack::set_level(activeLevel);
        
        // Create spans at different levels
        {
            lumberjack::Span span1(lumberjack::LOG_LEVEL_ERROR, "error_span");
        }
        {
            lumberjack::Span span2(lumberjack::LOG_LEVEL_WARN, "warn_span");
        }
        {
            lumberjack::Span span3(lumberjack::LOG_LEVEL_INFO, "info_span");
        }
        {
            lumberjack::Span span4(lumberjack::LOG_LEVEL_DEBUG, "debug_span");
        }
        
        auto& calls = SpanGatingBackend::getInstance().calls;
        
        // Count how many spans should be active
        int expectedSpans = 0;
        if (lumberjack::LOG_LEVEL_ERROR <= activeLevel) expectedSpans++;
        if (lumberjack::LOG_LEVEL_WARN <= activeLevel) expectedSpans++;
        if (lumberjack::LOG_LEVEL_INFO <= activeLevel) expectedSpans++;
        if (lumberjack::LOG_LEVEL_DEBUG <= activeLevel) expectedSpans++;
        
        // Each active span should have 2 calls (BEGIN and END)
        RC_ASSERT(calls.size() == static_cast<size_t>(expectedSpans * 2));
        
        // Verify all calls are properly paired
        for (size_t i = 0; i < calls.size(); i += 2) {
            RC_ASSERT(calls[i].type == SpanGatingBackend::SpanCall::BEGIN);
            RC_ASSERT(calls[i + 1].type == SpanGatingBackend::SpanCall::END);
            RC_ASSERT(calls[i].name == calls[i + 1].name);
        }
    });
    
    if (!result4) {
        std::cout << "FAILED: Multiple spans respect level gating independently" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Multiple spans respect level gating independently" << std::endl;
    }
    
    return success ? 0 : 1;
}
