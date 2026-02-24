// Custom backend example demonstrating backend implementation and switching
// This example shows how to create a custom logging backend with in-memory buffering

#include <lumberjack/lumberjack.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

// Custom backend that stores log messages in memory
namespace custom {
    // In-memory buffer to store log messages
    static std::vector<std::string> g_messageBuffer;
    static bool g_initialized = false;
    
    // Backend initialization - called when backend is activated
    void init() {
        printf("[CustomBackend] init() called - clearing buffer\n");
        g_messageBuffer.clear();
        g_initialized = true;
    }
    
    // Backend shutdown - called when backend is deactivated
    void shutdown() {
        printf("[CustomBackend] shutdown() called - had %zu messages in buffer\n", 
               g_messageBuffer.size());
        g_initialized = false;
    }
    
    // Write a log message to the in-memory buffer
    void log_write(lumberjack::LogLevel level, const char* message) {
        if (!g_initialized) {
            return;
        }
        
        // Format: [LEVEL] message
        const char* level_str = "";
        switch (level) {
            case lumberjack::LOG_LEVEL_ERROR: level_str = "ERROR"; break;
            case lumberjack::LOG_LEVEL_WARN:  level_str = "WARN";  break;
            case lumberjack::LOG_LEVEL_INFO:  level_str = "INFO";  break;
            case lumberjack::LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
            default: level_str = "UNKNOWN"; break;
        }
        
        char formatted[512];
        snprintf(formatted, sizeof(formatted), "[%s] %s", level_str, message);
        g_messageBuffer.push_back(formatted);
    }
    
    // Span begin callback - return a handle for correlation
    void* span_begin(lumberjack::LogLevel level, const char* name) {
        // For this simple backend, we don't need to track spans
        // Just return a dummy handle
        return (void*)0x1;
    }
    
    // Span end callback - log the span timing
    void span_end(void* handle, lumberjack::LogLevel level, 
                  const char* name, long long elapsed_us) {
        char message[256];
        snprintf(message, sizeof(message), "SPAN '%s' took %lld μs", 
                 name, elapsed_us);
        log_write(level, message);
    }
    
    // Helper function to dump all buffered messages
    void dump_buffer() {
        printf("\n[CustomBackend] Buffer contents (%zu messages):\n", 
               g_messageBuffer.size());
        for (size_t i = 0; i < g_messageBuffer.size(); i++) {
            printf("  %zu: %s\n", i + 1, g_messageBuffer[i].c_str());
        }
        printf("\n");
    }
    
    // Helper function to clear the buffer
    void clear_buffer() {
        g_messageBuffer.clear();
    }
    
    // Backend structure definition
    lumberjack::LogBackend backend = {
        "custom_memory",  // Backend name
        init,             // init function
        shutdown,         // shutdown function
        log_write,        // log_write function
        span_begin,       // span_begin function
        span_end          // span_end function
    };
}

int main() {
    // Start with the default builtin backend
    lumberjack::init();
    LOG_INFO("=== Custom Backend Example ===");
    LOG_INFO("Starting with builtin backend (stderr)\n");
    
    // Log some messages with builtin backend
    LOG_INFO("Message 1 - using builtin backend");
    LOG_WARN("Message 2 - using builtin backend");
    LOG_ERROR("Message 3 - using builtin backend");
    
    // Switch to custom backend
    printf("\n--- Switching to Custom Backend ---\n");
    lumberjack::set_backend(&custom::backend);
    
    // Log messages with custom backend (stored in memory, not printed)
    LOG_INFO("Message 4 - using custom backend (buffered)");
    LOG_WARN("Message 5 - using custom backend (buffered)");
    LOG_ERROR("Message 6 - using custom backend (buffered)");
    
    // Dump the buffer to see what was captured
    custom::dump_buffer();
    
    // Clear buffer and log more messages
    printf("--- Logging More Messages ---\n");
    custom::clear_buffer();
    
    LOG_INFO("Message 7 - after buffer clear");
    LOG_DEBUG("Message 8 - debug level (not visible at INFO level)");
    
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
    LOG_DEBUG("Message 9 - debug level (now visible)");
    
    custom::dump_buffer();
    
    // Demonstrate span timing with custom backend
    printf("--- Span Timing with Custom Backend ---\n");
    custom::clear_buffer();
    
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "custom_backend_span");
        // Simulate some work
        for (volatile int i = 0; i < 1000000; i++) {}
    }
    
    custom::dump_buffer();
    
    // Switch back to builtin backend
    printf("--- Switching Back to Builtin Backend ---\n");
    lumberjack::set_backend(lumberjack::builtin_backend());
    
    LOG_INFO("Message 10 - back to builtin backend (stderr)");
    LOG_INFO("Notice that shutdown() was called on custom backend");
    
    // Verify we're using builtin backend
    lumberjack::LogBackend* current = lumberjack::get_backend();
    printf("\nCurrent backend name: %s\n", current->name);
    
    LOG_INFO("\nExample complete!");
    
    return 0;
}
