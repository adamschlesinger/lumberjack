#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>

// Feature: pretty-backend, Property 1: Log Level Color Mapping
// Validates: Requirements 1.1, 1.2, 1.3, 1.4
// Property: For any log level (ERROR, WARN, INFO, DEBUG) and any message,
// when logged at that level, the output SHALL contain the correct ANSI color
// code for that level (red for ERROR, yellow for WARN, green for INFO, blue for DEBUG).

namespace {

// Expected ANSI color codes per level
const char* expected_color(lumberjack::LogLevel level) {
    switch (level) {
        case lumberjack::LOG_LEVEL_ERROR: return "\033[31m"; // red
        case lumberjack::LOG_LEVEL_WARN:  return "\033[33m"; // yellow
        case lumberjack::LOG_LEVEL_INFO:  return "\033[32m"; // green
        case lumberjack::LOG_LEVEL_DEBUG: return "\033[34m"; // blue
        default: return "\033[0m";
    }
}

// Capture stderr output from a callable
std::string capture_stderr(std::function<void()> fn) {
    // Flush any pending stderr
    fflush(stderr);

    // Save original stderr fd
    int saved_fd = dup(STDERR_FILENO);

    // Create temp file
    char temp[] = "/tmp/lj_color_test_XXXXXX";
    int tmp_fd = mkstemp(temp);

    // Redirect stderr to temp file
    dup2(tmp_fd, STDERR_FILENO);

    fn();

    // Flush and restore
    fflush(stderr);
    dup2(saved_fd, STDERR_FILENO);
    close(saved_fd);

    // Read captured output
    lseek(tmp_fd, 0, SEEK_SET);
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(tmp_fd, buf, sizeof(buf))) > 0) {
        output.append(buf, n);
    }
    close(tmp_fd);
    unlink(temp);

    return output;
}

} // anonymous namespace

int main() {
    bool success = true;

    // Get the pretty backend's log_write function pointer
    lumberjack::LogBackend* backend = lumberjack::pretty_backend();
    auto log_write = backend->log_write;

    std::cout << "Testing Property 1: Log Level Color Mapping..." << std::endl;

    bool result = rc::check(
        "Each log level produces output with the correct ANSI color code",
        [&]() {
            // Generate a random loggable level
            auto level = *rc::gen::element(
                lumberjack::LOG_LEVEL_ERROR,
                lumberjack::LOG_LEVEL_WARN,
                lumberjack::LOG_LEVEL_INFO,
                lumberjack::LOG_LEVEL_DEBUG
            );

            // Generate a non-empty printable message (avoid format-string issues)
            auto msg = *rc::gen::nonEmpty(
                rc::gen::container<std::string>(
                    rc::gen::inRange('a', 'z')
                )
            );

            // Call log_write directly and capture stderr
            std::string output = capture_stderr([&]() {
                log_write(level, msg.c_str());
            });

            // The output must contain the expected color code for this level
            const char* color = expected_color(level);
            RC_ASSERT(output.find(color) != std::string::npos);
        }
    );

    if (!result) {
        std::cout << "FAILED: Log Level Color Mapping" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Log Level Color Mapping" << std::endl;
    }

    // Feature: pretty-backend, Property 2: Color Reset After Output
    // Validates: Requirements 1.5
    // Property: For any log message or span marker output, the output SHALL
    // end with the ANSI reset code to prevent color bleeding into subsequent
    // terminal output.

    std::cout << "Testing Property 2: Color Reset After Output..." << std::endl;

    bool result2 = rc::check(
        "Every log output line ends with ANSI reset code before the newline",
        [&]() {
            // Generate a random loggable level
            auto level = *rc::gen::element(
                lumberjack::LOG_LEVEL_ERROR,
                lumberjack::LOG_LEVEL_WARN,
                lumberjack::LOG_LEVEL_INFO,
                lumberjack::LOG_LEVEL_DEBUG
            );

            // Generate a non-empty printable message
            auto msg = *rc::gen::nonEmpty(
                rc::gen::container<std::string>(
                    rc::gen::inRange('a', 'z')
                )
            );

            // Call log_write directly and capture stderr
            std::string output = capture_stderr([&]() {
                log_write(level, msg.c_str());
            });

            // The output must contain the ANSI reset code
            std::string reset = "\033[0m";
            RC_ASSERT(output.find(reset) != std::string::npos);

            // The reset code must appear after the color code (i.e., at the end
            // of the colored content, before the trailing newline)
            auto reset_pos = output.rfind(reset);
            // Everything after the reset should only be a newline
            std::string after_reset = output.substr(reset_pos + reset.size());
            RC_ASSERT(after_reset == "\n");
        }
    );

    if (!result2) {
        std::cout << "FAILED: Color Reset After Output" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Color Reset After Output" << std::endl;
    }

    return success ? 0 : 1;
}
