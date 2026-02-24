// Basic logging example demonstrating all log levels and runtime level changes
// This example shows how to use the lumberjack logging library for simple logging tasks

#include <lumberjack/lumberjack.h>

int main() {
    // Initialize the logger (optional - library uses safe defaults)
    // Default level is INFO, default backend is builtin (stderr)
    lumberjack::init();
    
    // Demonstrate logging at all levels with default INFO level
    ERROR("This is an error message - always visible at INFO level");
    WARN("This is a warning message - visible at INFO level");
    INFO("This is an info message - visible at INFO level");
    DEBUG("This is a debug message - NOT visible at INFO level");
    
    // Change log level to DEBUG using the Level:: alias
    INFO("Changing log level to DEBUG...");
    lumberjack::set_level(lumberjack::Level::Debug);
    
    ERROR("Error message - visible at DEBUG level");
    WARN("Warning message - visible at DEBUG level");
    INFO("Info message - visible at DEBUG level");
    DEBUG("Debug message - NOW visible at DEBUG level");
    
    // Change log level to WARN to see only errors and warnings
    INFO("Changing log level to WARN...");
    lumberjack::set_level(lumberjack::Level::Warn);
    
    ERROR("Error message - visible at WARN level");
    WARN("Warning message - visible at WARN level");
    INFO("Info message - NOT visible at WARN level");
    DEBUG("Debug message - NOT visible at WARN level");
    
    // Change log level to ERROR to see only errors
    WARN("Changing log level to ERROR...");
    lumberjack::set_level(lumberjack::Level::Error);
    
    ERROR("Error message - visible at ERROR level");
    WARN("Warning message - NOT visible at ERROR level");
    INFO("Info message - NOT visible at ERROR level");
    DEBUG("Debug message - NOT visible at ERROR level");
    
    // Demonstrate printf-style formatting
    lumberjack::set_level(lumberjack::Level::Info);
    INFO("Printf-style formatting: integer=%d, float=%.2f, string=%s", 
         42, 3.14159, "hello");
    
    // Demonstrate LOG_LEVEL_NONE suppresses all output
    INFO("Changing log level to NONE - all logging disabled");
    lumberjack::set_level(lumberjack::LOG_LEVEL_NONE);
    
    ERROR("This error will NOT be visible");
    WARN("This warning will NOT be visible");
    INFO("This info will NOT be visible");
    DEBUG("This debug will NOT be visible");
    
    // Re-enable logging to confirm we're done
    lumberjack::set_level(lumberjack::Level::Info);
    INFO("Example complete!");
    
    return 0;
}
