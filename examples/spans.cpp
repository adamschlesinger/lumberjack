// Span timing example demonstrating RAII-based performance measurement
// This example shows how to use LOG_SPAN for automatic timing of code sections

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
    LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "process_data");
    
    simulate_work(50);
    
    {
        // Inner span tracks a specific section
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "data_validation");
        simulate_work(30);
    }
    
    {
        // Another inner span
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "data_transformation");
        simulate_work(40);
    }
    
    simulate_work(20);
}

// Function demonstrating span level gating
void debug_analysis() {
    // This span only activates when log level is DEBUG or higher
    LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "debug_analysis");
    
    LOG_DEBUG("Performing detailed analysis...");
    simulate_work(100);
}

int main() {
    // Initialize with INFO level
    lumberjack::init();
    LOG_INFO("=== Span Timing Example ===\n");
    
    // Demonstrate basic span timing
    LOG_INFO("--- Basic Span Timing ---");
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "basic_operation");
        simulate_work(100);
    }
    
    // Demonstrate nested spans
    LOG_INFO("\n--- Nested Spans ---");
    process_data();
    
    // Demonstrate span level gating - span is ACTIVE at INFO level
    LOG_INFO("\n--- Span Level Gating (INFO level) ---");
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "info_level_span");
        LOG_INFO("This span is active because level is INFO");
        simulate_work(50);
    }
    
    // This DEBUG span will NOT be active at INFO level
    LOG_INFO("Calling debug_analysis() at INFO level - span will be inactive");
    debug_analysis();  // Span won't activate, no timing output
    
    // Change to DEBUG level and try again
    LOG_INFO("\n--- Span Level Gating (DEBUG level) ---");
    lumberjack::set_level(lumberjack::LOG_LEVEL_DEBUG);
    LOG_DEBUG("Log level changed to DEBUG");
    
    LOG_DEBUG("Calling debug_analysis() at DEBUG level - span will be active");
    debug_analysis();  // Span will activate and show timing
    
    // Demonstrate multiple spans at different levels
    LOG_INFO("\n--- Multiple Spans at Different Levels ---");
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_ERROR, "critical_operation");
        simulate_work(30);
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_WARN, "warning_operation");
        simulate_work(30);
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "info_operation");
        simulate_work(30);
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "debug_operation");
        simulate_work(30);
    }
    
    // Demonstrate that spans respect log level changes
    LOG_INFO("\n--- Span Gating with WARN Level ---");
    lumberjack::set_level(lumberjack::LOG_LEVEL_WARN);
    LOG_WARN("Log level changed to WARN - only ERROR and WARN spans will be active");
    
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_ERROR, "error_span_active");
        simulate_work(30);
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_WARN, "warn_span_active");
        simulate_work(30);
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_INFO, "info_span_inactive");
        simulate_work(30);  // This span won't produce output
    }
    {
        LOG_SPAN(lumberjack::LOG_LEVEL_DEBUG, "debug_span_inactive");
        simulate_work(30);  // This span won't produce output
    }
    
    lumberjack::set_level(lumberjack::LOG_LEVEL_INFO);
    LOG_INFO("\nExample complete!");
    
    return 0;
}
