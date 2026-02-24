#include <lumberjack/lumberjack.h>
#include <rapidcheck.h>
#include <iostream>
#include <string>
#include <regex>
#include <cstdio>
#include <unistd.h>

// Feature: pretty-backend, Property 10: Log Message Format
// Validates: Requirements 8.1, 8.4
// Property: For any log message and level, the output format SHALL match
// the pattern: "[LEVEL] message" (without timestamps), where LEVEL is the
// string representation of the log level.

namespace {

const char* level_string(lumberjack::LogLevel level) {
    switch (level) {
        case lumberjack::LOG_LEVEL_ERROR: return "ERROR";
        case lumberjack::LOG_LEVEL_WARN:  return "WARN";
        case lumberjack::LOG_LEVEL_INFO:  return "INFO";
        case lumberjack::LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

std::string capture_stderr(std::function<void()> fn) {
    fflush(stderr);
    int saved_fd = dup(STDERR_FILENO);
    char temp[] = "/tmp/lj_fmt_test_XXXXXX";
    int tmp_fd = mkstemp(temp);
    dup2(tmp_fd, STDERR_FILENO);

    fn();

    fflush(stderr);
    dup2(saved_fd, STDERR_FILENO);
    close(saved_fd);

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

// Strip ANSI escape codes from a string
std::string strip_ansi(const std::string& input) {
    std::regex ansi_re("\033\\[[0-9;]*m");
    return std::regex_replace(input, ansi_re, "");
}

} // anonymous namespace

int main() {
    bool success = true;
    lumberjack::LogBackend* backend = lumberjack::pretty_backend();
    auto log_write = backend->log_write;

    std::cout << "Testing Property 10: Log Message Format..." << std::endl;

    bool result = rc::check(
        "Log output matches [LEVEL] message format without timestamps",
        [&]() {
            auto level = *rc::gen::element(
                lumberjack::LOG_LEVEL_ERROR,
                lumberjack::LOG_LEVEL_WARN,
                lumberjack::LOG_LEVEL_INFO,
                lumberjack::LOG_LEVEL_DEBUG
            );

            auto msg = *rc::gen::nonEmpty(
                rc::gen::container<std::string>(
                    rc::gen::inRange('a', 'z')
                )
            );

            std::string output = capture_stderr([&]() {
                log_write(level, msg.c_str());
            });

            // Strip ANSI codes to check plain format
            std::string plain = strip_ansi(output);

            // Must contain [LEVEL]
            std::string expected_tag = std::string("[") + level_string(level) + "]";
            RC_ASSERT(plain.find(expected_tag) != std::string::npos);

            // Must contain the message
            RC_ASSERT(plain.find(msg) != std::string::npos);

            // Full line should match: "[LEVEL] message\n" (no timestamp)
            std::string expected_line = expected_tag + " " + msg + "\n";
            RC_ASSERT(plain == expected_line);

            // Must NOT contain a timestamp pattern (YYYY-MM-DD or HH:MM:SS)
            std::regex timestamp_re(R"(\d{4}-\d{2}-\d{2})");
            RC_ASSERT(!std::regex_search(plain, timestamp_re));
        }
    );

    if (!result) {
        std::cout << "FAILED: Log Message Format" << std::endl;
        success = false;
    } else {
        std::cout << "PASSED: Log Message Format" << std::endl;
    }

    return success ? 0 : 1;
}
