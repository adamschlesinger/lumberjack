#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <vector>
#include <atomic>

// Feature: lumberjack, Property 3: Backend Lifecycle Sequencing
// Validates: Requirements 3.4, 7.3, 7.4
// Property: For any sequence of backend switches, the logger SHALL call
// shutdown on the previous backend before calling init on the new backend,
// and SHALL call init on a custom backend before it receives any log messages.

// Tracking structure for lifecycle events
struct LifecycleEvent {
    enum Type { INIT, SHUTDOWN, LOG_WRITE };
    Type type;
    const char* backend_name;
    int sequence_number;
};

// Global event log
static std::vector<LifecycleEvent> g_eventLog;
static std::atomic<int> g_sequenceCounter{0};

// Mock backend 1
struct MockBackend1 {
    static void init() {
        g_eventLog.push_back({LifecycleEvent::INIT, "backend1", g_sequenceCounter++});
    }
    
    static void shutdown() {
        g_eventLog.push_back({LifecycleEvent::SHUTDOWN, "backend1", g_sequenceCounter++});
    }
    
    static void log_write(lumberjack::LogLevel level, const char* message) {
        g_eventLog.push_back({LifecycleEvent::LOG_WRITE, "backend1", g_sequenceCounter++});
    }
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        return nullptr;
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level,
                        const char* name, long long elapsed_us) {}
};

// Mock backend 2
struct MockBackend2 {
    static void init() {
        g_eventLog.push_back({LifecycleEvent::INIT, "backend2", g_sequenceCounter++});
    }
    
    static void shutdown() {
        g_eventLog.push_back({LifecycleEvent::SHUTDOWN, "backend2", g_sequenceCounter++});
    }
    
    static void log_write(lumberjack::LogLevel level, const char* message) {
        g_eventLog.push_back({LifecycleEvent::LOG_WRITE, "backend2", g_sequenceCounter++});
    }
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        return nullptr;
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level,
                        const char* name, long long elapsed_us) {}
};

// Mock backend 3
struct MockBackend3 {
    static void init() {
        g_eventLog.push_back({LifecycleEvent::INIT, "backend3", g_sequenceCounter++});
    }
    
    static void shutdown() {
        g_eventLog.push_back({LifecycleEvent::SHUTDOWN, "backend3", g_sequenceCounter++});
    }
    
    static void log_write(lumberjack::LogLevel level, const char* message) {
        g_eventLog.push_back({LifecycleEvent::LOG_WRITE, "backend3", g_sequenceCounter++});
    }
    
    static void* span_begin(lumberjack::LogLevel level, const char* name) {
        return nullptr;
    }
    
    static void span_end(void* handle, lumberjack::LogLevel level,
                        const char* name, long long elapsed_us) {}
};

static lumberjack::LogBackend g_mockBackend1 = {
    "backend1",
    MockBackend1::init,
    MockBackend1::shutdown,
    MockBackend1::log_write,
    MockBackend1::span_begin,
    MockBackend1::span_end
};

static lumberjack::LogBackend g_mockBackend2 = {
    "backend2",
    MockBackend2::init,
    MockBackend2::shutdown,
    MockBackend2::log_write,
    MockBackend2::span_begin,
    MockBackend2::span_end
};

static lumberjack::LogBackend g_mockBackend3 = {
    "backend3",
    MockBackend3::init,
    MockBackend3::shutdown,
    MockBackend3::log_write,
    MockBackend3::span_begin,
    MockBackend3::span_end
};

// Generator for backend pointers
namespace rc {
    template<>
    struct Arbitrary<lumberjack::LogBackend*> {
        static Gen<lumberjack::LogBackend*> arbitrary() {
            return gen::element<lumberjack::LogBackend*>(
                &g_mockBackend1,
                &g_mockBackend2,
                &g_mockBackend3
            );
        }
    };
}

// Helper function to reset state
void reset_state() {
    g_eventLog.clear();
    g_sequenceCounter = 0;
}

// Helper function to find events for a backend
std::vector<LifecycleEvent> find_events_for_backend(const char* backend_name) {
    std::vector<LifecycleEvent> result;
    for (const auto& event : g_eventLog) {
        if (std::strcmp(event.backend_name, backend_name) == 0) {
            result.push_back(event);
        }
    }
    return result;
}

int main() {
    bool success = true;
    
    std::cout << "Testing Property 3: Backend Lifecycle Sequencing..." << std::endl;
    
    // Property 1: Shutdown called before init on backend switch
    std::cout << "Testing: Shutdown called before init on backend switch..." << std::endl;
    
    bool result1 = rc::check("Shutdown before init on backend switch", []() {
        reset_state();
        
        // Set first backend
        auto backend1 = *rc::gen::arbitrary<lumberjack::LogBackend*>();
        lumberjack::set_backend(backend1);
        
        // Clear events from first backend setup
        size_t first_backend_events = g_eventLog.size();
        
        // Set second backend (different from first)
        auto backend2 = *rc::gen::arbitrary<lumberjack::LogBackend*>();
        lumberjack::set_backend(backend2);
        
        // Check that we have events after the switch
        RC_ASSERT(g_eventLog.size() > first_backend_events);
        
        // Find the shutdown and init events after the first backend setup
        bool found_shutdown = false;
        bool found_init = false;
        int shutdown_seq = -1;
        int init_seq = -1;
        
        for (size_t i = first_backend_events; i < g_eventLog.size(); i++) {
            const auto& event = g_eventLog[i];
            
            if (event.type == LifecycleEvent::SHUTDOWN) {
                found_shutdown = true;
                shutdown_seq = event.sequence_number;
            }
            
            if (event.type == LifecycleEvent::INIT) {
                found_init = true;
                init_seq = event.sequence_number;
            }
        }
        
        // If both backends are different, we should see shutdown then init
        if (backend1 != backend2) {
            RC_ASSERT(found_shutdown);
            RC_ASSERT(found_init);
            // Shutdown must come before init
            RC_ASSERT(shutdown_seq < init_seq);
        }
    });
    
    if (!result1) {
        std::cout << "FAILED: Shutdown before init on backend switch" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Shutdown before init on backend switch" << std::endl;
    }
    
    // Property 2: Init called before first log message
    std::cout << "Testing: Init called before first log message..." << std::endl;
    
    bool result2 = rc::check("Init before first log message", []() {
        reset_state();
        
        // Set a backend
        auto backend = *rc::gen::arbitrary<lumberjack::LogBackend*>();
        lumberjack::set_backend(backend);
        
        // Set log level to allow messages
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Send a log message
        LOG_INFO("Test message");
        
        // Find init and log_write events for this backend
        auto events = find_events_for_backend(backend->name);
        
        // Should have at least init and log_write
        RC_ASSERT(events.size() >= 2);
        
        // Find first init and first log_write
        int first_init_seq = -1;
        int first_log_seq = -1;
        
        for (const auto& event : events) {
            if (event.type == LifecycleEvent::INIT && first_init_seq == -1) {
                first_init_seq = event.sequence_number;
            }
            if (event.type == LifecycleEvent::LOG_WRITE && first_log_seq == -1) {
                first_log_seq = event.sequence_number;
            }
        }
        
        // Init must come before first log message
        RC_ASSERT(first_init_seq != -1);
        RC_ASSERT(first_log_seq != -1);
        RC_ASSERT(first_init_seq < first_log_seq);
    });
    
    if (!result2) {
        std::cout << "FAILED: Init before first log message" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Init before first log message" << std::endl;
    }
    
    // Property 3: Multiple backend switches maintain proper sequencing
    std::cout << "Testing: Multiple backend switches maintain sequencing..." << std::endl;
    
    bool result3 = rc::check("Multiple switches maintain sequencing", []() {
        reset_state();
        
        // Generate a sequence of backend switches
        auto numSwitches = *rc::gen::inRange(2, 6);
        
        lumberjack::LogBackend* prev_backend = nullptr;
        
        for (int i = 0; i < numSwitches; i++) {
            auto backend = *rc::gen::arbitrary<lumberjack::LogBackend*>();
            
            size_t events_before = g_eventLog.size();
            lumberjack::set_backend(backend);
            
            // Check events after this switch
            if (prev_backend != nullptr && prev_backend != backend) {
                // Should have shutdown of previous, then init of new
                bool found_shutdown = false;
                bool found_init = false;
                int shutdown_seq = -1;
                int init_seq = -1;
                
                for (size_t j = events_before; j < g_eventLog.size(); j++) {
                    const auto& event = g_eventLog[j];
                    
                    if (event.type == LifecycleEvent::SHUTDOWN &&
                        std::strcmp(event.backend_name, prev_backend->name) == 0) {
                        found_shutdown = true;
                        shutdown_seq = event.sequence_number;
                    }
                    
                    if (event.type == LifecycleEvent::INIT &&
                        std::strcmp(event.backend_name, backend->name) == 0) {
                        found_init = true;
                        init_seq = event.sequence_number;
                    }
                }
                
                RC_ASSERT(found_shutdown);
                RC_ASSERT(found_init);
                RC_ASSERT(shutdown_seq < init_seq);
            }
            
            prev_backend = backend;
        }
    });
    
    if (!result3) {
        std::cout << "FAILED: Multiple switches maintain sequencing" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Multiple switches maintain sequencing" << std::endl;
    }
    
    // Property 4: No log messages between shutdown and init
    std::cout << "Testing: No log messages between shutdown and init..." << std::endl;
    
    bool result4 = rc::check("No log messages between shutdown and init", []() {
        reset_state();
        
        // Set first backend
        lumberjack::set_backend(&g_mockBackend1);
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        
        // Log a message
        LOG_INFO("Before switch");
        
        size_t events_before_switch = g_eventLog.size();
        
        // Switch to second backend
        lumberjack::set_backend(&g_mockBackend2);
        
        // Check events during the switch
        bool found_shutdown = false;
        bool found_init = false;
        int shutdown_seq = -1;
        int init_seq = -1;
        
        for (size_t i = events_before_switch; i < g_eventLog.size(); i++) {
            const auto& event = g_eventLog[i];
            
            if (event.type == LifecycleEvent::SHUTDOWN) {
                found_shutdown = true;
                shutdown_seq = event.sequence_number;
            }
            
            if (event.type == LifecycleEvent::INIT) {
                found_init = true;
                init_seq = event.sequence_number;
            }
            
            // No log messages should occur between shutdown and init
            if (found_shutdown && !found_init) {
                RC_ASSERT(event.type != LifecycleEvent::LOG_WRITE);
            }
        }
        
        RC_ASSERT(found_shutdown);
        RC_ASSERT(found_init);
        RC_ASSERT(shutdown_seq < init_seq);
    });
    
    if (!result4) {
        std::cout << "FAILED: No log messages between shutdown and init" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: No log messages between shutdown and init" << std::endl;
    }
    
    return success ? 0 : 1;
}
