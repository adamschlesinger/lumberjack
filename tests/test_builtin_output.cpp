#include <lumberjack/lumberjack.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <regex>
#include <iostream>
#include <unistd.h>

// Unit tests for built-in backend output format
// Validates: Requirements 4.4, 13.4
// Tests:
// - Timestamp format: [YYYY-MM-DD HH:MM:SS.mmm]
// - Level string formatting: [ERROR], [WARN], [INFO], [DEBUG]
// - Message content preservation

// Helper to read file content
std::string read_file(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return "";
    
    std::string content;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f)) {
        content += buffer;
    }
    fclose(f);
    return content;
}

// Helper function to capture log output to a file
std::string capture_log_to_file(void (*test_func)()) {
    // Create a temporary file
    char temp_filename[] = "/tmp/lumberjack_test_XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd == -1) {
        std::cerr << "Failed to create temporary file" << std::endl;
        return "";
    }
    
    FILE* temp_file = fdopen(fd, "w+");
    if (!temp_file) {
        std::cerr << "Failed to open temporary file" << std::endl;
        close(fd);
        unlink(temp_filename);
        return "";
    }
    
    // Redirect builtin backend output to temp file
    lumberjack::builtin_set_output(temp_file);
    
    // Run the test function
    test_func();
    
    // Flush the file
    fflush(temp_file);
    
    // Reset to stderr
    lumberjack::builtin_set_output(stderr);
    
    // Read the captured output
    rewind(temp_file);
    std::string output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), temp_file)) {
        output += buffer;
    }
    
    // Clean up
    fclose(temp_file);
    unlink(temp_filename);
    
    return output;
}

bool test_timestamp_format() {
    std::cout << "Testing timestamp format..." << std::endl;
    
    // Capture a log message
    std::string output = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
        LOG_INFO("test message");
    });
    
    // Expected format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
    // Timestamp regex: [YYYY-MM-DD HH:MM:SS.mmm]
    std::regex timestamp_pattern(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\])");
    
    if (!std::regex_search(output, timestamp_pattern)) {
        std::cerr << "FAILED: Timestamp format incorrect" << std::endl;
        std::cerr << "Output: '" << output << "'" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Timestamp format correct" << std::endl;
    return true;
}

bool test_level_string_formatting() {
    std::cout << "Testing level string formatting..." << std::endl;
    
    // Test ERROR level
    std::string output_error = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        LOG_ERROR("test");
    });
    
    if (output_error.find("[ERROR]") == std::string::npos) {
        std::cerr << "FAILED: ERROR level string not found" << std::endl;
        std::cerr << "Output: '" << output_error << "'" << std::endl;
        return false;
    }
    
    // Test WARN level
    std::string output_warn = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        LOG_WARN("test");
    });
    
    if (output_warn.find("[WARN]") == std::string::npos) {
        std::cerr << "FAILED: WARN level string not found" << std::endl;
        std::cerr << "Output: '" << output_warn << "'" << std::endl;
        return false;
    }
    
    // Test INFO level
    std::string output_info = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        LOG_INFO("test");
    });
    
    if (output_info.find("[INFO]") == std::string::npos) {
        std::cerr << "FAILED: INFO level string not found" << std::endl;
        std::cerr << "Output: '" << output_info << "'" << std::endl;
        return false;
    }
    
    // Test DEBUG level
    std::string output_debug = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
        LOG_DEBUG("test");
    });
    
    if (output_debug.find("[DEBUG]") == std::string::npos) {
        std::cerr << "FAILED: DEBUG level string not found" << std::endl;
        std::cerr << "Output: '" << output_debug << "'" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Level string formatting correct for all levels" << std::endl;
    return true;
}

bool test_message_content_preservation() {
    std::cout << "Testing message content preservation..." << std::endl;
    
    // Test simple message
    std::string output1 = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
        LOG_INFO("Simple message");
    });
    
    if (output1.find("Simple message") == std::string::npos) {
        std::cerr << "FAILED: Simple message not preserved" << std::endl;
        std::cerr << "Output: '" << output1 << "'" << std::endl;
        return false;
    }
    
    // Test message with numbers
    std::string output2 = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
        LOG_INFO("Message with numbers: 12345");
    });
    
    if (output2.find("Message with numbers: 12345") == std::string::npos) {
        std::cerr << "FAILED: Message with numbers not preserved" << std::endl;
        std::cerr << "Output: '" << output2 << "'" << std::endl;
        return false;
    }
    
    // Test message with special characters
    std::string output3 = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
        LOG_INFO("Special chars: !@#$%%");
    });
    
    if (output3.find("Special chars: !@#$%") == std::string::npos) {
        std::cerr << "FAILED: Message with special chars not preserved" << std::endl;
        std::cerr << "Output: '" << output3 << "'" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Message content preserved for all test cases" << std::endl;
    return true;
}

bool test_complete_format() {
    std::cout << "Testing complete output format..." << std::endl;
    
    std::string output = capture_log_to_file([]() {
        lumberjack::set_level(lumberjack::LOG_LEVEL_ERROR);
        LOG_ERROR("test error");
    });
    
    // Complete format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message\n
    std::regex complete_pattern(
        R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] \[ERROR\] test error\n)"
    );
    
    if (!std::regex_match(output, complete_pattern)) {
        std::cerr << "FAILED: Complete format incorrect" << std::endl;
        std::cerr << "Output: '" << output << "'" << std::endl;
        return false;
    }
    
    std::cout << "PASSED: Complete output format correct" << std::endl;
    return true;
}

int main() {
    bool success = true;
    
    // Initialize logger once at the start
    lumberjack::init();
    
    // Run all tests
    success &= test_timestamp_format();
    success &= test_level_string_formatting();
    success &= test_message_content_preservation();
    success &= test_complete_format();
    
    if (success) {
        std::cout << "\nAll built-in backend output format tests PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome built-in backend output format tests FAILED" << std::endl;
        return 1;
    }
}
