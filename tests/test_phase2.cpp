// test_phase2.cpp - Comprehensive Phase 2 Tests
// Tests for image resizing core functionality

#include <fastresize.h>
#include "internal.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

// Simple test framework
#define TEST(name) \
    bool test_##name(); \
    bool test_##name()

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  FAILED: %s\n", message); \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, message) \
    do { \
        if ((a) != (b)) { \
            printf("  FAILED: %s (expected %d, got %d)\n", message, (int)(b), (int)(a)); \
            return false; \
        } \
    } while(0)

#define ASSERT_NEAR(a, b, tolerance, message) \
    do { \
        if (std::abs((a) - (b)) > (tolerance)) { \
            printf("  FAILED: %s (expected %d, got %d, tolerance %d)\n", \
                   message, (int)(b), (int)(a), (int)(tolerance)); \
            return false; \
        } \
    } while(0)

#define ASSERT_TRUE(condition, message) ASSERT(condition, message)
#define ASSERT_FALSE(condition, message) ASSERT(!(condition), message)

// ============================================
// Test Image Generator
// ============================================

// Generate a simple test BMP image in memory
bool generate_test_bmp(const std::string& filename, int width, int height) {
    // BMP header for 24-bit RGB
    unsigned char bmp_header[54] = {
        'B', 'M',           // Signature
        0, 0, 0, 0,         // File size (filled later)
        0, 0, 0, 0,         // Reserved
        54, 0, 0, 0,        // Offset to pixel data
        40, 0, 0, 0,        // DIB header size
        0, 0, 0, 0,         // Width (filled later)
        0, 0, 0, 0,         // Height (filled later)
        1, 0,               // Planes
        24, 0,              // Bits per pixel
        0, 0, 0, 0,         // Compression (none)
        0, 0, 0, 0,         // Image size (can be 0 for uncompressed)
        0, 0, 0, 0,         // X pixels per meter
        0, 0, 0, 0,         // Y pixels per meter
        0, 0, 0, 0,         // Colors in palette
        0, 0, 0, 0          // Important colors
    };

    // Calculate row size (must be multiple of 4)
    int row_size = ((width * 3 + 3) / 4) * 4;
    int pixel_data_size = row_size * height;
    int file_size = 54 + pixel_data_size;

    // Fill in header values
    *(int*)(&bmp_header[2]) = file_size;
    *(int*)(&bmp_header[18]) = width;
    *(int*)(&bmp_header[22]) = height;

    FILE* f = fopen(filename.c_str(), "wb");
    if (!f) return false;

    // Write header
    fwrite(bmp_header, 1, 54, f);

    // Write pixel data (simple gradient pattern)
    unsigned char* row = new unsigned char[row_size];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = x * 3;
            row[offset + 0] = (unsigned char)((x * 255) / width);        // Blue
            row[offset + 1] = (unsigned char)((y * 255) / height);       // Green
            row[offset + 2] = (unsigned char)(((x + y) * 255) / (width + height)); // Red
        }
        // Pad to row_size
        for (int x = width * 3; x < row_size; x++) {
            row[x] = 0;
        }
        fwrite(row, 1, row_size, f);
    }

    delete[] row;
    fclose(f);
    return true;
}

// ============================================
// Dimension Calculation Tests
// ============================================

TEST(dimension_calculation_scale_percent) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;

    fastresize::internal::calculate_dimensions(100, 200, opts, opts.target_width, opts.target_height);

    ASSERT_EQ(opts.target_width, 50, "Width should be 50% of 100");
    ASSERT_EQ(opts.target_height, 100, "Height should be 50% of 200");
    return true;
}

TEST(dimension_calculation_fit_width_with_aspect) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = 800;
    opts.keep_aspect_ratio = true;

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(2000, 1500, opts, out_w, out_h);

    ASSERT_EQ(out_w, 800, "Width should be 800");
    // 800 / 2000 = 0.4, 1500 * 0.4 = 600
    ASSERT_EQ(out_h, 600, "Height should preserve 4:3 ratio");
    return true;
}

TEST(dimension_calculation_fit_width_without_aspect) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = 800;
    opts.keep_aspect_ratio = false;

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(2000, 1500, opts, out_w, out_h);

    ASSERT_EQ(out_w, 800, "Width should be 800");
    ASSERT_EQ(out_h, 1500, "Height should remain unchanged");
    return true;
}

TEST(dimension_calculation_fit_height_with_aspect) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
    opts.target_height = 600;
    opts.keep_aspect_ratio = true;

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(2000, 1500, opts, out_w, out_h);

    ASSERT_EQ(out_h, 600, "Height should be 600");
    // 600 / 1500 = 0.4, 2000 * 0.4 = 800
    ASSERT_EQ(out_w, 800, "Width should preserve 4:3 ratio");
    return true;
}

TEST(dimension_calculation_exact_size_no_aspect) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;
    opts.keep_aspect_ratio = false;

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(2000, 1500, opts, out_w, out_h);

    ASSERT_EQ(out_w, 800, "Width should be exactly 800");
    ASSERT_EQ(out_h, 600, "Height should be exactly 600");
    return true;
}

TEST(dimension_calculation_exact_size_with_aspect) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 800;  // Square target
    opts.keep_aspect_ratio = true;

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(2000, 1500, opts, out_w, out_h);

    // Should fit 2000x1500 (4:3) into 800x800, maintaining aspect
    // Ratio = min(800/2000, 800/1500) = min(0.4, 0.533) = 0.4
    // Result: 800x600
    ASSERT_EQ(out_w, 800, "Width should be 800");
    ASSERT_EQ(out_h, 600, "Height should be 600 to maintain 4:3 ratio");
    return true;
}

TEST(dimension_calculation_minimum_size) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.00001f;  // Extremely small

    int out_w = 0, out_h = 0;
    fastresize::internal::calculate_dimensions(100, 100, opts, out_w, out_h);

    // Should clamp to minimum 1x1
    ASSERT_TRUE(out_w >= 1, "Width should be at least 1");
    ASSERT_TRUE(out_h >= 1, "Height should be at least 1");
    return true;
}

// ============================================
// Resize Operation Tests
// ============================================

TEST(resize_scale_percent_50) {
    // Generate test image
    const char* input = "/tmp/test_input_800x600.bmp";
    const char* output = "/tmp/test_output_400x300.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 800, 600), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Resize should succeed");

    // Verify output dimensions
    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 400, "Output width should be 400");
    ASSERT_EQ(info.height, 300, "Output height should be 300");

    // Cleanup
    remove(input);
    remove(output);
    return true;
}

TEST(resize_fit_width) {
    const char* input = "/tmp/test_input_1000x800.bmp";
    const char* output = "/tmp/test_output_500x400.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 1000, 800), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = 500;
    opts.keep_aspect_ratio = true;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 500, "Output width should be 500");
    ASSERT_EQ(info.height, 400, "Output height should be 400 (5:4 ratio preserved)");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_fit_height) {
    const char* input = "/tmp/test_input_800x1000.bmp";
    const char* output = "/tmp/test_output_400x500.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 800, 1000), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
    opts.target_height = 500;
    opts.keep_aspect_ratio = true;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 400, "Output width should be 400 (4:5 ratio preserved)");
    ASSERT_EQ(info.height, 500, "Output height should be 500");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_exact_size) {
    const char* input = "/tmp/test_input_1920x1080.bmp";
    const char* output = "/tmp/test_output_640x480.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 1920, 1080), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 640;
    opts.target_height = 480;
    opts.keep_aspect_ratio = false;  // Force exact dimensions

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 640, "Output width should be exactly 640");
    ASSERT_EQ(info.height, 480, "Output height should be exactly 480");

    remove(input);
    remove(output);
    return true;
}

// ============================================
// Edge Case Tests
// ============================================

TEST(resize_very_small_1x1) {
    const char* input = "/tmp/test_input_1x1.bmp";
    const char* output = "/tmp/test_output_10x10.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 1, 1), "Failed to generate 1x1 image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 10;
    opts.target_height = 10;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Should handle 1x1 upscaling");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 10, "Output width should be 10");
    ASSERT_EQ(info.height, 10, "Output height should be 10");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_to_1x1) {
    const char* input = "/tmp/test_input_100x100.bmp";
    const char* output = "/tmp/test_output_1x1.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 100, 100), "Failed to generate 100x100 image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 1;
    opts.target_height = 1;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Should handle downscaling to 1x1");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 1, "Output width should be 1");
    ASSERT_EQ(info.height, 1, "Output height should be 1");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_large_image_2000x2000) {
    const char* input = "/tmp/test_input_2000x2000.bmp";
    const char* output = "/tmp/test_output_800x800.bmp";

    printf("  Generating 2000x2000 test image...\n");
    ASSERT_TRUE(generate_test_bmp(input, 2000, 2000), "Failed to generate large image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 800;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Should handle large image resize");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 800, "Output width should be 800");
    ASSERT_EQ(info.height, 800, "Output height should be 800");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_extreme_aspect_ratio_wide) {
    const char* input = "/tmp/test_input_1000x100.bmp";
    const char* output = "/tmp/test_output_500x50.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 1000, 100), "Failed to generate wide image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Should handle extreme wide aspect ratio");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 500, "Output width should be 500");
    ASSERT_EQ(info.height, 50, "Output height should be 50");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_extreme_aspect_ratio_tall) {
    const char* input = "/tmp/test_input_100x1000.bmp";
    const char* output = "/tmp/test_output_50x500.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 100, 1000), "Failed to generate tall image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Should handle extreme tall aspect ratio");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 50, "Output width should be 50");
    ASSERT_EQ(info.height, 500, "Output height should be 500");

    remove(input);
    remove(output);
    return true;
}

// ============================================
// Filter Comparison Tests
// ============================================

TEST(resize_filter_mitchell) {
    const char* input = "/tmp/test_input_filter_mitchell.bmp";
    const char* output = "/tmp/test_output_mitchell.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 400, 400), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.filter = fastresize::ResizeOptions::MITCHELL;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Mitchell filter resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 200, "Output should be 200x200");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_filter_catmull_rom) {
    const char* input = "/tmp/test_input_filter_catmull.bmp";
    const char* output = "/tmp/test_output_catmull.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 400, 400), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.filter = fastresize::ResizeOptions::CATMULL_ROM;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Catmull-Rom filter resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 200, "Output should be 200x200");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_filter_box) {
    const char* input = "/tmp/test_input_filter_box.bmp";
    const char* output = "/tmp/test_output_box.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 400, 400), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.filter = fastresize::ResizeOptions::BOX;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Box filter resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 200, "Output should be 200x200");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_filter_triangle) {
    const char* input = "/tmp/test_input_filter_triangle.bmp";
    const char* output = "/tmp/test_output_triangle.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 400, 400), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.filter = fastresize::ResizeOptions::TRIANGLE;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "Triangle filter resize should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 200, "Output should be 200x200");

    remove(input);
    remove(output);
    return true;
}

// ============================================
// Upscaling and Downscaling Tests
// ============================================

TEST(resize_upscale_2x) {
    const char* input = "/tmp/test_input_200x200.bmp";
    const char* output = "/tmp/test_output_400x400_upscale.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 200, 200), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 2.0f;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "2x upscale should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 400, "Output width should be 400");
    ASSERT_EQ(info.height, 400, "Output height should be 400");

    remove(input);
    remove(output);
    return true;
}

TEST(resize_downscale_4x) {
    const char* input = "/tmp/test_input_800x800.bmp";
    const char* output = "/tmp/test_output_200x200_downscale.bmp";

    ASSERT_TRUE(generate_test_bmp(input, 800, 800), "Failed to generate test image");

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.25f;

    bool success = fastresize::resize(input, output, opts);
    ASSERT_TRUE(success, "4x downscale should succeed");

    fastresize::ImageInfo info = fastresize::get_image_info(output);
    ASSERT_EQ(info.width, 200, "Output width should be 200");
    ASSERT_EQ(info.height, 200, "Output height should be 200");

    remove(input);
    remove(output);
    return true;
}

// ============================================
// Test Runner
// ============================================

struct TestCase {
    const char* name;
    bool (*func)();
};

int main() {
    TestCase tests[] = {
        // Dimension calculation tests
        {"dimension_calculation_scale_percent", test_dimension_calculation_scale_percent},
        {"dimension_calculation_fit_width_with_aspect", test_dimension_calculation_fit_width_with_aspect},
        {"dimension_calculation_fit_width_without_aspect", test_dimension_calculation_fit_width_without_aspect},
        {"dimension_calculation_fit_height_with_aspect", test_dimension_calculation_fit_height_with_aspect},
        {"dimension_calculation_exact_size_no_aspect", test_dimension_calculation_exact_size_no_aspect},
        {"dimension_calculation_exact_size_with_aspect", test_dimension_calculation_exact_size_with_aspect},
        {"dimension_calculation_minimum_size", test_dimension_calculation_minimum_size},

        // Resize operation tests
        {"resize_scale_percent_50", test_resize_scale_percent_50},
        {"resize_fit_width", test_resize_fit_width},
        {"resize_fit_height", test_resize_fit_height},
        {"resize_exact_size", test_resize_exact_size},

        // Edge case tests
        {"resize_very_small_1x1", test_resize_very_small_1x1},
        {"resize_to_1x1", test_resize_to_1x1},
        {"resize_large_image_2000x2000", test_resize_large_image_2000x2000},
        {"resize_extreme_aspect_ratio_wide", test_resize_extreme_aspect_ratio_wide},
        {"resize_extreme_aspect_ratio_tall", test_resize_extreme_aspect_ratio_tall},

        // Filter tests
        {"resize_filter_mitchell", test_resize_filter_mitchell},
        {"resize_filter_catmull_rom", test_resize_filter_catmull_rom},
        {"resize_filter_box", test_resize_filter_box},
        {"resize_filter_triangle", test_resize_filter_triangle},

        // Upscaling/downscaling tests
        {"resize_upscale_2x", test_resize_upscale_2x},
        {"resize_downscale_4x", test_resize_downscale_4x},
    };

    int num_tests = sizeof(tests) / sizeof(TestCase);
    int passed = 0;
    int failed = 0;

    printf("\n");
    printf("FastResize Phase 2 - Comprehensive Resize Tests\n");
    printf("================================================\n");
    printf("\n");

    for (int i = 0; i < num_tests; i++) {
        printf("Running test: %s...\n", tests[i].name);
        if (tests[i].func()) {
            printf("  PASSED\n");
            passed++;
        } else {
            failed++;
        }
        printf("\n");
    }

    printf("================================================\n");
    printf("Test Summary:\n");
    printf("  Tests run:    %d\n", num_tests);
    printf("  Tests passed: %d\n", passed);
    printf("  Tests failed: %d\n", failed);
    printf("================================================\n");

    return (failed == 0) ? 0 : 1;
}
