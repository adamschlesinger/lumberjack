// Basic logging example demonstrating all log levels and runtime level changes
// This example shows how to use the lumberjack logging library for simple logging tasks

#include <lumberjack/lumberjack.h>

int main() {
    // Initialize the logger (optional - library uses safe defaults)
    // Default level is INFO, default backend is builtin (stderr)
    lumberjack::init();
    
    // Demonstrate logging at all levels with default INFO level
    LOG_ERROR("This is an error message - always visible at INFO level");
    LOG_WARN("This is a warning message - visible at INFO level");
    LOG_INFO("This is an info message - visible at INFO level");
    LOG_DEBUG("This is a debug message - NOT visible at INFO level");
    
    // Change log level to DEBUG to see all messages
    LOG_INFO("Changing log level to DEBUG...");
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
    
    LOG_ERROR("Error message - visible at DEBUG level");
    LOG_WARN("Warning message - visible at DEBUG level");
    LOG_INFO("Info message - visible at DEBUG level");
    LOG_DEBUG("Debug message - NOW visible at DEBUG level");
    
    // Change log level to WARN to see only errors and warnings
    LOG_INFO("Changing log level to WARN...");
    lumberjack::set_level(lumberjack::LOG_LEVEL_WARN);
    
    LOG_ERROR("Error message - visible at WARN level");
    LOG_WARN("Warning message - visible at WARN level");
    LOG_INFO("Info message - NOT visible at WARN level");
    LOG_DEBUG("Debug message - NOT visible at WARN level");
    
    // Change log level to ERROR to see only errors
    LOG_WARN("Changing log level to ERROR...");
    lumberjack::set_level(lumberjack::LOG_LEVEL_ERROR);
    
    LOG_ERROR("Error message - visible at ERROR level");
    LOG_WARN("Warning message - NOT visible at ERROR level");
    LOG_INFO("Info message - NOT visible at ERROR level");
    LOG_DEBUG("Debug message - NOT visible at ERROR level");
    
    // Demonstrate printf-style formatting
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    LOG_INFO("Printf-style formatting: integer=%d, float=%.2f, string=%s", 
             42, 3.14159, "hello");
    
    // Demonstrate LOG_LEVEL_NONE suppresses all output
    LOG_INFO("Changing log level to NONE - all logging disabled");
    lumberjack::set_level(lumberjack::LOG_LEVEL_NONE);
    
    LOG_ERROR("This error will NOT be visible");
    LOG_WARN("This warning will NOT be visible");
    LOG_INFO("This info will NOT be visible");
    LOG_DEBUG("This debug will NOT be visible");
    
    // Re-enable logging to confirm we're done
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    LOG_INFO("Example complete!");
    
    return 0;
}
