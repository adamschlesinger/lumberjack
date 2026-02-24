#include <lumberjack/lumberjack.h>
#include <iostream>
#include <cstring>

// Unit tests for pretty backend API integration
// Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5

int main() {
    bool success = true;
    
    std::cout << "Testing pretty backend API integration..." << std::endl;
    
    // Test 1: pretty_backend() returns non-null pointer
    std::cout << "Test 1: pretty_backend() returns non-null pointer..." << std::endl;
    {
        lumberjack::LogBackend* backend = lumberjack::pretty_backend();
        if (backend == nullptr) {
            std::cout << "FAILED: pretty_backend() returned null" << std::endl;
            success = false;
        } else {
            std::cout << "PASSED: pretty_backend() returns non-null pointer" << std::endl;
        }
    }
    
    // Test 2: Backend name field is "pretty"
    std::cout << "Test 2: Backend name field is 'pretty'..." << std::endl;
    {
        lumberjack::LogBackend* backend = lumberjack::pretty_backend();
        if (backend == nullptr) {
            std::cout << "FAILED: Cannot test name - backend is null" << std::endl;
            success = false;
        } else if (backend->name == nullptr) {
            std::cout << "FAILED: Backend name is null" << std::endl;
            success = false;
        } else if (std::strcmp(backend->name, "pretty") != 0) {
            std::cout << "FAILED: Backend name is '" << backend->name 
                     << "' instead of 'pretty'" << std::endl;
            success = false;
        } else {
            std::cout << "PASSED: Backend name is 'pretty'" << std::endl;
        }
    }
    
    // Test 3: All callback function pointers are non-null
    std::cout << "Test 3: All callback function pointers are non-null..." << std::endl;
    {
        lumberjack::LogBackend* backend = lumberjack::pretty_backend();
        if (backend == nullptr) {
            std::cout << "FAILED: Cannot test callbacks - backend is null" << std::endl;
            success = false;
        } else {
            bool all_non_null = true;
            
            if (backend->init == nullptr) {
                std::cout << "FAILED: init callback is null" << std::endl;
                all_non_null = false;
            }
            
            if (backend->shutdown == nullptr) {
                std::cout << "FAILED: shutdown callback is null" << std::endl;
                all_non_null = false;
            }
            
            if (backend->log_write == nullptr) {
                std::cout << "FAILED: log_write callback is null" << std::endl;
                all_non_null = false;
            }
            
            if (backend->span_begin == nullptr) {
                std::cout << "FAILED: span_begin callback is null" << std::endl;
                all_non_null = false;
            }
            
            if (backend->span_end == nullptr) {
                std::cout << "FAILED: span_end callback is null" << std::endl;
                all_non_null = false;
            }
            
            if (all_non_null) {
                std::cout << "PASSED: All callback function pointers are non-null" << std::endl;
            } else {
                success = false;
            }
        }
    }
    
    // Test 4: set_backend(pretty_backend()) successfully switches backend
    std::cout << "Test 4: set_backend(pretty_backend()) successfully switches backend..." << std::endl;
    {
        // Save current backend name to restore later
        lumberjack::LogBackend* original_backend = lumberjack::get_backend();
        const char* original_name = original_backend ? original_backend->name : nullptr;
        
        // Switch to pretty backend
        lumberjack::LogBackend* pretty = lumberjack::pretty_backend();
        lumberjack::set_backend(pretty);
        
        // Verify the switch by checking the backend name
        // Note: set_backend copies the backend by value, so we check the name
        lumberjack::LogBackend* current_backend = lumberjack::get_backend();
        
        if (current_backend == nullptr) {
            std::cout << "FAILED: get_backend() returned null after set_backend()" << std::endl;
            success = false;
        } else if (current_backend->name == nullptr) {
            std::cout << "FAILED: Current backend name is null" << std::endl;
            success = false;
        } else if (std::strcmp(current_backend->name, "pretty") != 0) {
            std::cout << "FAILED: Current backend name is '" << current_backend->name 
                     << "' instead of 'pretty'" << std::endl;
            success = false;
        } else {
            std::cout << "PASSED: set_backend(pretty_backend()) successfully switches backend" << std::endl;
        }
        
        // Restore original backend (switch back to builtin)
        if (original_name && std::strcmp(original_name, "builtin") == 0) {
            lumberjack::set_backend(lumberjack::builtin_backend());
        }
    }
    
    std::cout << std::endl;
    if (success) {
        std::cout << "All tests PASSED" << std::endl;
    } else {
        std::cout << "Some tests FAILED" << std::endl;
    }
    
    return success ? 0 : 1;
}
