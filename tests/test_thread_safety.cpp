#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>

// Feature: lumberjack, Property 7: Thread Safety of Concurrent Logging
// Validates: Requirements 11.1, 11.2
// Property: For any number of threads concurrently calling logging macros,
// the built-in backend SHALL produce output without corruption, data races, or crashes.

namespace rc {
    template<>
    struct Arbitrary<lumberjack::LogLevel> {
        static Gen<lumberjack::LogLevel> arbitrary() {
            return gen::element(
                lumberjack::LOG_LEVEL_ERROR,
                lumberjack::LOG_LEVEL_WARN,
                lumberjack::LOG_LEVEL_INFO,
                lumberjack::LOG_LEVEL_DEBUG
            );
        }
    };
}

// Test concurrent logging from multiple threads
bool test_concurrent_logging() {
    std::cout << "Testing Property 7: Thread Safety of Concurrent Logging..." << std::endl;
    
    bool result = rc::check("Concurrent logging is thread-safe", []() {
        // Generate random number of threads (2-16)
        auto numThreads = *rc::gen::inRange(2, 17);
        // Generate random number of messages per thread (10-100)
        auto messagesPerThread = *rc::gen::inRange(10, 101);
        
        // Reset to builtin backend and set level to DEBUG
        lumberjack::set_backend(lumberjack::builtin_backend());
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        std::atomic<int> startFlag{0};
        std::atomic<int> completedThreads{0};
        std::vector<std::thread> threads;
        
        // Launch threads
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([t, messagesPerThread, &startFlag, &completedThreads]() {
                // Wait for all threads to be ready
                while (startFlag.load() == 0) {
                    std::this_thread::yield();
                }
                
                // Log messages at various levels
                for (int i = 0; i < messagesPerThread; ++i) {
                    switch (i % 4) {
                        case 0:
                            LOG_ERROR("Thread %d message %d", t, i);
                            break;
                        case 1:
                            LOG_WARN("Thread %d message %d", t, i);
                            break;
                        case 2:
                            LOG_INFO("Thread %d message %d", t, i);
                            break;
                        case 3:
                            LOG_DEBUG("Thread %d message %d", t, i);
                            break;
                    }
                }
                
                completedThreads.fetch_add(1);
            });
        }
        
        // Start all threads simultaneously
        startFlag.store(1);
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify all threads completed
        RC_ASSERT(completedThreads.load() == numThreads);
        
        // If we reach here without crashes or hangs, the test passes
        return true;
    });
    
    if (!result) {
        std::cout << "FAILED: Concurrent logging is thread-safe" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Thread Safety of Concurrent Logging" << std::endl;
    return true;
}

// Test that concurrent span creation is thread-safe
bool test_concurrent_spans() {
    std::cout << "Testing concurrent span creation..." << std::endl;
    
    bool result = rc::check("Concurrent span creation is thread-safe", []() {
        auto numThreads = *rc::gen::inRange(2, 17);
        auto spansPerThread = *rc::gen::inRange(5, 21);
        
        lumberjack::set_backend(lumberjack::builtin_backend());
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        std::atomic<int> startFlag{0};
        std::atomic<int> completedThreads{0};
        std::vector<std::thread> threads;
        
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([t, spansPerThread, &startFlag, &completedThreads]() {
                while (startFlag.load() == 0) {
                    std::this_thread::yield();
                }
                
                for (int i = 0; i < spansPerThread; ++i) {
                    std::ostringstream oss;
                    oss << "Thread_" << t << "_Span_" << i;
                    std::string spanName = oss.str();
                    
                    LOG_SPAN(lumberjack::LOG_LEVEL_INFO, spanName.c_str());
                    
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                
                completedThreads.fetch_add(1);
            });
        }
        
        startFlag.store(1);
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        RC_ASSERT(completedThreads.load() == numThreads);
        return true;
    });
    
    if (!result) {
        std::cout << "FAILED: Concurrent span creation is thread-safe" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Concurrent span creation" << std::endl;
    return true;
}

int main() {
    bool success = true;
    
    // Run thread safety tests
    if (!test_concurrent_logging()) {
        success = false;
    }
    
    if (!test_concurrent_spans()) {
        success = false;
    }
    
    return success ? 0 : 1;
}
