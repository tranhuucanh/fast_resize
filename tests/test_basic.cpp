#include <fastresize.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

// Simple test framework
int tests_run = 0;
int tests_passed = 0;

#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        std::cout << "Running test: " << #name << "..." << std::flush; \
        tests_run++; \
        try { \
            test_##name(); \
            tests_passed++; \
            std::cout << " PASSED" << std::endl; \
        } catch (const std::exception& e) { \
            std::cout << " FAILED: " << e.what() << std::endl; \
        } catch (...) { \
            std::cout << " FAILED: Unknown exception" << std::endl; \
        } \
    } \
    void test_##name()

#define ASSERT(condition, message) \
    if (!(condition)) { \
        throw std::runtime_error(message); \
    }

#define ASSERT_EQ(a, b, message) \
    if ((a) != (b)) { \
        throw std::runtime_error(std::string(message) + " (expected " + std::to_string(b) + ", got " + std::to_string(a) + ")"); \
    }

// ============================================
// Test Cases
// ============================================

TEST(format_detection) {
    // This test would require actual image files
    // For now, just verify the API exists
    fastresize::ImageInfo info = fastresize::get_image_info("nonexistent.jpg");
    ASSERT(info.width == 0, "Non-existent file should return zero dimensions");
}

TEST(resize_options_defaults) {
    fastresize::ResizeOptions opts;
    ASSERT_EQ(opts.mode, fastresize::ResizeOptions::EXACT_SIZE, "Default mode");
    ASSERT_EQ(opts.quality, 85, "Default quality");
    ASSERT(opts.keep_aspect_ratio == true, "Default keep_aspect_ratio");
    ASSERT(opts.overwrite_input == false, "Default overwrite_input");
}

TEST(batch_result_structure) {
    std::vector<std::string> empty_paths;
    fastresize::ResizeOptions opts;
    fastresize::BatchOptions batch_opts;

    fastresize::BatchResult result = fastresize::batch_resize(
        empty_paths, "/tmp", opts, batch_opts
    );

    ASSERT_EQ(result.total, 0, "Empty batch should have 0 total");
    ASSERT_EQ(result.success, 0, "Empty batch should have 0 success");
    ASSERT_EQ(result.failed, 0, "Empty batch should have 0 failed");
}

TEST(error_handling) {
    // Test error handling with invalid input
    fastresize::ResizeOptions opts;
    opts.target_width = 100;
    opts.target_height = 100;

    bool result = fastresize::resize("nonexistent.jpg", "output.jpg", opts);
    ASSERT(result == false, "Should fail on non-existent file");

    std::string error = fastresize::get_last_error();
    ASSERT(!error.empty(), "Error message should not be empty");
}

// ============================================
// Main Test Runner
// ============================================

int main() {
    std::cout << "FastResize Phase 1 - Basic Tests" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;

    // Run all tests
    run_test_format_detection();
    run_test_resize_options_defaults();
    run_test_batch_result_structure();
    run_test_error_handling();

    // Print summary
    std::cout << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Tests run:    " << tests_run << std::endl;
    std::cout << "  Tests passed: " << tests_passed << std::endl;
    std::cout << "  Tests failed: " << (tests_run - tests_passed) << std::endl;
    std::cout << "=================================" << std::endl;

    return (tests_run == tests_passed) ? 0 : 1;
}
