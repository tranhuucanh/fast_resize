/**
 * FastResize - Phase 4 Tests
 * Comprehensive tests for batch processing and threading
 */

#include <fastresize.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// Don't define IMPLEMENTATION - already in library
#include "../include/stb_image.h"
#include "../include/stb_image_write.h"

// Test framework
struct Test {
    const char* name;
    bool (*func)();
};

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "  FAILED: " << message << std::endl; \
            return false; \
        } \
    } while (0)

// Helper functions
static bool create_test_image(const char* path, int width, int height) {
    int channels = 3;
    unsigned char* pixels = new unsigned char[width * height * channels];

    // Create gradient pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * channels;
            pixels[idx + 0] = (x * 255) / width;      // R
            pixels[idx + 1] = (y * 255) / height;     // G
            pixels[idx + 2] = 128;                     // B
        }
    }

    int result = stbi_write_jpg(path, width, height, channels, pixels, 85);
    delete[] pixels;
    return result != 0;
}

static bool file_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

static bool directory_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

static bool create_directory(const char* path) {
    return mkdir(path, 0755) == 0 || directory_exists(path);
}

static void cleanup_directory(const char* path) {
    if (!directory_exists(path)) return;

    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string filepath = std::string(path) + "/" + entry->d_name;
        remove(filepath.c_str());
    }

    closedir(dir);
    rmdir(path);
}

static int count_files_in_directory(const char* path) {
    if (!directory_exists(path)) return 0;

    int count = 0;
    DIR* dir = opendir(path);
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        count++;
    }

    closedir(dir);
    return count;
}

// ============================================
// Test 1: Basic Batch Resize (Small Batch)
// ============================================

bool test_batch_resize_small() {
    cleanup_directory("test_batch_small_input");
    cleanup_directory("test_batch_small_output");

    create_directory("test_batch_small_input");
    create_directory("test_batch_small_output");

    // Create 10 test images
    std::vector<std::string> input_paths;
    for (int i = 0; i < 10; i++) {
        std::string path = "test_batch_small_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), 400, 300), "Failed to create test image");
        input_paths.push_back(path);
    }

    // Batch resize
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 150;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 4;

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_small_output", opts, batch_opts);

    ASSERT(result.total == 10, "Total should be 10");
    ASSERT(result.success == 10, "All 10 should succeed");
    ASSERT(result.failed == 0, "None should fail");
    ASSERT(result.errors.empty(), "No errors expected");

    // Verify output files exist
    int output_count = count_files_in_directory("test_batch_small_output");
    ASSERT(output_count == 10, "Should have 10 output files");

    // Verify one output image
    fastresize::ImageInfo info = fastresize::get_image_info("test_batch_small_output/img0.jpg");
    ASSERT(info.width == 200, "Output width should be 200");
    ASSERT(info.height == 150, "Output height should be 150");

    cleanup_directory("test_batch_small_input");
    cleanup_directory("test_batch_small_output");
    return true;
}

// ============================================
// Test 2: Medium Batch (100 images)
// ============================================

bool test_batch_resize_medium() {
    cleanup_directory("test_batch_medium_input");
    cleanup_directory("test_batch_medium_output");

    create_directory("test_batch_medium_input");
    create_directory("test_batch_medium_output");

    // Create 100 test images
    std::vector<std::string> input_paths;
    for (int i = 0; i < 100; i++) {
        std::string path = "test_batch_medium_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), 800, 600), "Failed to create test image");
        input_paths.push_back(path);
    }

    // Batch resize
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 8;

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_medium_output", opts, batch_opts);

    ASSERT(result.total == 100, "Total should be 100");
    ASSERT(result.success == 100, "All 100 should succeed");
    ASSERT(result.failed == 0, "None should fail");

    // Verify output
    int output_count = count_files_in_directory("test_batch_medium_output");
    ASSERT(output_count == 100, "Should have 100 output files");

    cleanup_directory("test_batch_medium_input");
    cleanup_directory("test_batch_medium_output");
    return true;
}

// ============================================
// Test 3: Custom Batch (Different Options)
// ============================================

bool test_batch_resize_custom() {
    cleanup_directory("test_batch_custom_input");
    cleanup_directory("test_batch_custom_output");

    create_directory("test_batch_custom_input");
    create_directory("test_batch_custom_output");

    // Create test images with different sizes
    ASSERT(create_test_image("test_batch_custom_input/img1.jpg", 400, 300),
           "Failed to create test image 1");
    ASSERT(create_test_image("test_batch_custom_input/img2.jpg", 800, 600),
           "Failed to create test image 2");
    ASSERT(create_test_image("test_batch_custom_input/img3.jpg", 1024, 768),
           "Failed to create test image 3");

    // Create custom batch items with different resize options
    std::vector<fastresize::BatchItem> items;

    fastresize::BatchItem item1;
    item1.input_path = "test_batch_custom_input/img1.jpg";
    item1.output_path = "test_batch_custom_output/out1.jpg";
    item1.options.mode = fastresize::ResizeOptions::EXACT_SIZE;
    item1.options.target_width = 200;
    item1.options.target_height = 150;
    items.push_back(item1);

    fastresize::BatchItem item2;
    item2.input_path = "test_batch_custom_input/img2.jpg";
    item2.output_path = "test_batch_custom_output/out2.jpg";
    item2.options.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    item2.options.scale_percent = 0.5f;
    items.push_back(item2);

    fastresize::BatchItem item3;
    item3.input_path = "test_batch_custom_input/img3.jpg";
    item3.output_path = "test_batch_custom_output/out3.jpg";
    item3.options.mode = fastresize::ResizeOptions::FIT_WIDTH;
    item3.options.target_width = 512;
    items.push_back(item3);

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 4;

    fastresize::BatchResult result = fastresize::batch_resize_custom(items, batch_opts);

    ASSERT(result.total == 3, "Total should be 3");
    ASSERT(result.success == 3, "All 3 should succeed");
    ASSERT(result.failed == 0, "None should fail");

    // Verify each output
    fastresize::ImageInfo info1 = fastresize::get_image_info("test_batch_custom_output/out1.jpg");
    ASSERT(info1.width == 200 && info1.height == 150, "Output 1 dimensions wrong");

    fastresize::ImageInfo info2 = fastresize::get_image_info("test_batch_custom_output/out2.jpg");
    ASSERT(info2.width == 400 && info2.height == 300, "Output 2 dimensions wrong");

    fastresize::ImageInfo info3 = fastresize::get_image_info("test_batch_custom_output/out3.jpg");
    ASSERT(info3.width == 512 && info3.height == 384, "Output 3 dimensions wrong");

    cleanup_directory("test_batch_custom_input");
    cleanup_directory("test_batch_custom_output");
    return true;
}

// ============================================
// Test 4: Error Handling (Missing Files)
// ============================================

bool test_batch_error_handling() {
    cleanup_directory("test_batch_error_output");
    create_directory("test_batch_error_output");

    // Mix of existing and non-existing files
    std::vector<std::string> input_paths;
    input_paths.push_back("test_batch_error_input/nonexistent1.jpg");
    input_paths.push_back("test_batch_error_input/nonexistent2.jpg");
    input_paths.push_back("test_batch_error_input/nonexistent3.jpg");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 100;
    opts.target_height = 100;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 4;
    batch_opts.stop_on_error = false;  // Continue on error

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_error_output", opts, batch_opts);

    ASSERT(result.total == 3, "Total should be 3");
    ASSERT(result.success == 0, "None should succeed");
    ASSERT(result.failed == 3, "All 3 should fail");
    ASSERT(result.errors.size() == 3, "Should have 3 error messages");

    cleanup_directory("test_batch_error_output");
    return true;
}

// ============================================
// Test 5: Stop on Error
// ============================================

bool test_batch_stop_on_error() {
    cleanup_directory("test_batch_stop_input");
    cleanup_directory("test_batch_stop_output");

    create_directory("test_batch_stop_input");
    create_directory("test_batch_stop_output");

    // Create a mix of valid and invalid files
    std::vector<std::string> input_paths;

    // First valid file
    ASSERT(create_test_image("test_batch_stop_input/img0.jpg", 400, 300),
           "Failed to create test image");
    input_paths.push_back("test_batch_stop_input/img0.jpg");

    // Invalid file (doesn't exist)
    input_paths.push_back("test_batch_stop_input/nonexistent.jpg");

    // More files that shouldn't be processed
    for (int i = 1; i < 10; i++) {
        std::string path = "test_batch_stop_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), 400, 300), "Failed to create test image");
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 150;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 2;
    batch_opts.stop_on_error = true;  // Stop on first error

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_stop_output", opts, batch_opts);

    ASSERT(result.total == 11, "Total should be 11");
    ASSERT(result.failed > 0, "Should have at least one failure");
    // Note: Due to parallel processing, we may process more than one before stopping
    ASSERT(result.success + result.failed <= result.total, "Sum should not exceed total");

    cleanup_directory("test_batch_stop_input");
    cleanup_directory("test_batch_stop_output");
    return true;
}

// ============================================
// Test 6: Thread Pool Scaling (1, 2, 4, 8 threads)
// ============================================

bool test_thread_pool_scaling() {
    cleanup_directory("test_thread_input");
    cleanup_directory("test_thread_output");

    create_directory("test_thread_input");

    // Create 20 test images
    std::vector<std::string> input_paths;
    for (int i = 0; i < 20; i++) {
        std::string path = "test_thread_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), 800, 600), "Failed to create test image");
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 300;

    // Test with different thread counts
    int thread_counts[] = {1, 2, 4, 8};

    for (int thread_count : thread_counts) {
        cleanup_directory("test_thread_output");
        create_directory("test_thread_output");

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = thread_count;

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, "test_thread_output", opts, batch_opts);

        ASSERT(result.total == 20, "Total should be 20");
        ASSERT(result.success == 20, "All 20 should succeed");
        ASSERT(result.failed == 0, "None should fail");

        int output_count = count_files_in_directory("test_thread_output");
        ASSERT(output_count == 20, "Should have 20 output files");
    }

    cleanup_directory("test_thread_input");
    cleanup_directory("test_thread_output");
    return true;
}

// ============================================
// Test 7: Empty Batch
// ============================================

bool test_batch_empty() {
    std::vector<std::string> empty_paths;

    fastresize::ResizeOptions opts;
    fastresize::BatchOptions batch_opts;

    fastresize::BatchResult result = fastresize::batch_resize(
        empty_paths, "test_output", opts, batch_opts);

    ASSERT(result.total == 0, "Total should be 0");
    ASSERT(result.success == 0, "Success should be 0");
    ASSERT(result.failed == 0, "Failed should be 0");
    ASSERT(result.errors.empty(), "Errors should be empty");

    return true;
}

// ============================================
// Test 8: Large Batch Performance (Scaled)
// ============================================

bool test_batch_large() {
    cleanup_directory("test_batch_large_input");
    cleanup_directory("test_batch_large_output");

    create_directory("test_batch_large_input");
    create_directory("test_batch_large_output");

    // Create 50 test images (scaled down from 300 for faster testing)
    std::vector<std::string> input_paths;
    for (int i = 0; i < 50; i++) {
        std::string path = "test_batch_large_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), 1000, 1000), "Failed to create test image");
        input_paths.push_back(path);
    }

    // Batch resize
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 400;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 8;

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_large_output", opts, batch_opts);

    ASSERT(result.total == 50, "Total should be 50");
    ASSERT(result.success == 50, "All 50 should succeed");
    ASSERT(result.failed == 0, "None should fail");

    int output_count = count_files_in_directory("test_batch_large_output");
    ASSERT(output_count == 50, "Should have 50 output files");

    cleanup_directory("test_batch_large_input");
    cleanup_directory("test_batch_large_output");
    return true;
}

// ============================================
// Test 9: Different Image Sizes in Batch
// ============================================

bool test_batch_mixed_sizes() {
    cleanup_directory("test_batch_mixed_input");
    cleanup_directory("test_batch_mixed_output");

    create_directory("test_batch_mixed_input");
    create_directory("test_batch_mixed_output");

    // Create images of different sizes
    std::vector<std::string> input_paths;

    int sizes[][2] = {
        {100, 100}, {200, 150}, {400, 300}, {800, 600},
        {1024, 768}, {1920, 1080}, {320, 240}, {640, 480}
    };

    for (int i = 0; i < 8; i++) {
        std::string path = "test_batch_mixed_input/img" + std::to_string(i) + ".jpg";
        ASSERT(create_test_image(path.c_str(), sizes[i][0], sizes[i][1]),
               "Failed to create test image");
        input_paths.push_back(path);
    }

    // Batch resize to same size
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 300;
    opts.target_height = 300;
    opts.keep_aspect_ratio = false;  // Force exact dimensions

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 4;

    fastresize::BatchResult result = fastresize::batch_resize(
        input_paths, "test_batch_mixed_output", opts, batch_opts);

    ASSERT(result.total == 8, "Total should be 8");
    ASSERT(result.success == 8, "All 8 should succeed");
    ASSERT(result.failed == 0, "None should fail");

    // Verify all outputs are 300x300
    for (int i = 0; i < 8; i++) {
        std::string path = "test_batch_mixed_output/img" + std::to_string(i) + ".jpg";
        fastresize::ImageInfo info = fastresize::get_image_info(path);
        ASSERT(info.width == 300 && info.height == 300,
               "Output dimensions should be 300x300");
    }

    cleanup_directory("test_batch_mixed_input");
    cleanup_directory("test_batch_mixed_output");
    return true;
}

// ============================================
// Test 10: Batch with Different Quality Settings
// ============================================

bool test_batch_quality_settings() {
    cleanup_directory("test_batch_quality_input");
    cleanup_directory("test_batch_quality_output");

    create_directory("test_batch_quality_input");
    create_directory("test_batch_quality_output");

    // Create test image
    ASSERT(create_test_image("test_batch_quality_input/img.jpg", 800, 600),
           "Failed to create test image");

    // Create batch items with different quality settings
    std::vector<fastresize::BatchItem> items;

    int qualities[] = {50, 75, 90, 95};
    for (int i = 0; i < 4; i++) {
        fastresize::BatchItem item;
        item.input_path = "test_batch_quality_input/img.jpg";
        item.output_path = "test_batch_quality_output/img_q" + std::to_string(qualities[i]) + ".jpg";
        item.options.mode = fastresize::ResizeOptions::EXACT_SIZE;
        item.options.target_width = 400;
        item.options.target_height = 300;
        item.options.quality = qualities[i];
        items.push_back(item);
    }

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 4;

    fastresize::BatchResult result = fastresize::batch_resize_custom(items, batch_opts);

    ASSERT(result.total == 4, "Total should be 4");
    ASSERT(result.success == 4, "All 4 should succeed");
    ASSERT(result.failed == 0, "None should fail");

    // Verify all outputs exist
    int output_count = count_files_in_directory("test_batch_quality_output");
    ASSERT(output_count == 4, "Should have 4 output files");

    cleanup_directory("test_batch_quality_input");
    cleanup_directory("test_batch_quality_output");
    return true;
}

// ============================================
// Test Registry
// ============================================

static Test tests[] = {
    {"batch_resize_small", test_batch_resize_small},
    {"batch_resize_medium", test_batch_resize_medium},
    {"batch_resize_custom", test_batch_resize_custom},
    {"batch_error_handling", test_batch_error_handling},
    {"batch_stop_on_error", test_batch_stop_on_error},
    {"thread_pool_scaling", test_thread_pool_scaling},
    {"batch_empty", test_batch_empty},
    {"batch_large", test_batch_large},
    {"batch_mixed_sizes", test_batch_mixed_sizes},
    {"batch_quality_settings", test_batch_quality_settings},
};

// ============================================
// Main Test Runner
// ============================================

int main() {
    std::cout << "FastResize Phase 4 - Batch Processing & Threading Tests" << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << std::endl;

    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++) {
        tests_run++;
        std::cout << "Running test: " << tests[i].name << "... " << std::flush;

        if (tests[i].func()) {
            std::cout << "PASSED" << std::endl;
            tests_passed++;
        } else {
            std::cout << "FAILED" << std::endl;
            tests_failed++;
        }
    }

    std::cout << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Tests run:    " << tests_run << std::endl;
    std::cout << "  Tests passed: " << tests_passed << std::endl;
    std::cout << "  Tests failed: " << tests_failed << std::endl;
    std::cout << "========================================================" << std::endl;

    return (tests_failed == 0) ? 0 : 1;
}
