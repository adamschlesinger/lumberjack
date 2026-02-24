// Span timing example demonstrating RAII-based performance measurement
// This example shows how to use level-specific span macros for automatic
// timing of code sections.

#include <lumberjack/lumberjack.h>
#include <thread>
#include <chrono>

// Simulate some work with a sleep
void simulate_work(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Function demonstrating nested spans
void process_data() {
    // Outer span tracks the entire function
    INFO_SPAN("process_data");
    
    simulate_work(50);
    
    {
        // Inner span tracks a specific section
        INFO_SPAN("data_validation");
        simulate_work(30);
    }
    
    {
        // Another inner span
        INFO_SPAN("data_transformation");
        simulate_work(40);
    }
    
    simulate_work(20);
}

// Function demonstrating span level gating
void debug_analysis() {
    // This span only activates when log level is DEBUG or higher
    DEBUG_SPAN("debug_analysis");
    
    DEBUG("Performing detailed analysis...");
    simulate_work(100);
}

int main() {
    // Initialize with INFO level
    lumberjack::init();
    INFO("=== Span Timing Example ===\n");
    
    // Demonstrate basic span timing
    INFO("--- Basic Span Timing ---");
    {
        INFO_SPAN("basic_operation");
        simulate_work(100);
    }
    
    // Demonstrate nested spans
    INFO("\n--- Nested Spans ---");
    process_data();
    
    // Demonstrate span level gating - span is ACTIVE at INFO level
    INFO("\n--- Span Level Gating (INFO level) ---");
    {
        INFO_SPAN("info_level_span");
        INFO("This span is active because level is INFO");
        simulate_work(50);
    }
    
    // This DEBUG span will NOT be active at INFO level
    INFO("Calling debug_analysis() at INFO level - span will be inactive");
    debug_analysis();  // Span won't activate, no timing output
    
    // Change to DEBUG level and try again
    INFO("\n--- Span Level Gating (DEBUG level) ---");
    lumberjack::set_level(lumberjack::Level::Debug);
    DEBUG("Log level changed to DEBUG");
    
    DEBUG("Calling debug_analysis() at DEBUG level - span will be active");
    debug_analysis();  // Span will activate and show timing
    
    // Demonstrate multiple spans at different levels
    INFO("\n--- Multiple Spans at Different Levels ---");
    {
        ERROR_SPAN("critical_operation");
        simulate_work(30);
    }
    {
        WARN_SPAN("warning_operation");
        simulate_work(30);
    }
    {
        INFO_SPAN("info_operation");
        simulate_work(30);
    }
    {
        DEBUG_SPAN("debug_operation");
        simulate_work(30);
    }
    
    // Demonstrate that spans respect log level changes
    INFO("\n--- Span Gating with WARN Level ---");
    lumberjack::set_level(lumberjack::Level::Warn);
    WARN("Log level changed to WARN - only ERROR and WARN spans will be active");
    
    {
        ERROR_SPAN("error_span_active");
        simulate_work(30);
    }
    {
        WARN_SPAN("warn_span_active");
        simulate_work(30);
    }
    {
        INFO_SPAN("info_span_inactive");
        simulate_work(30);  // This span won't produce output
    }
    {
        DEBUG_SPAN("debug_span_inactive");
        simulate_work(30);  // This span won't produce output
    }
    
    lumberjack::set_level(lumberjack::Level::Info);
    INFO("\nExample complete!");
    
    return 0;
}
