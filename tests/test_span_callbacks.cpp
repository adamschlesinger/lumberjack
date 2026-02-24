#include "lumberjack/lumberjack.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

// Simple test to verify span callbacks work
int main() {
    // Redirect output to a temporary file for testing
    FILE* temp = tmpfile();
    if (!temp) {
        fprintf(stderr, "Failed to create temp file\n");
        return 1;
    }
    
    // Initialize logger and set output
    lumberjack::init();
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    lumberjack::builtin_set_output(temp);
    
    // Test 1: Create a span and verify it logs
    {
        lumberjack::Span span(lumberjack::LOG_LEVEL_INFO, "test_operation");
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // Read back the output
    rewind(temp);
    char buffer[512];
    bool found_span = false;
    while (fgets(buffer, sizeof(buffer), temp)) {
        if (strstr(buffer, "SPAN 'test_operation' took") && strstr(buffer, "us")) {
            found_span = true;
            printf("Found span output: %s", buffer);
            break;
        }
    }
    
    fclose(temp);
    
    if (!found_span) {
        fprintf(stderr, "ERROR: Span output not found\n");
        return 1;
    }
    
    printf("SUCCESS: Span callbacks work correctly\n");
    return 0;
}
