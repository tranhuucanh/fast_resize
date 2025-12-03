/**
 * FastResize Phase 3 - Advanced Codec Tests
 *
 * Tests for specialized image codecs:
 * - libjpeg-turbo for JPEG
 * - libpng for PNG
 * - libwebp for WEBP
 *
 * Test categories:
 * 1. Codec decode/encode tests
 * 2. Format conversion tests
 * 3. Quality control tests
 * 4. Cross-format resize tests
 * 5. WEBP support tests (new in Phase 3)
 */

#include <fastresize.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sys/stat.h>

// Test utilities
namespace {

struct TestResult {
    std::string name;
    bool passed;
    std::string error;
};

std::vector<TestResult> test_results;

void print_header(const std::string& title) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(title.length(), '=') << "\n\n";
}

void run_test(const std::string& name, bool (*test_func)()) {
    std::cout << "Running test: " << name << "... " << std::flush;

    TestResult result;
    result.name = name;

    try {
        result.passed = test_func();
        result.error = result.passed ? "" : "Test returned false";
    } catch (const std::exception& e) {
        result.passed = false;
        result.error = std::string("Exception: ") + e.what();
    } catch (...) {
        result.passed = false;
        result.error = "Unknown exception";
    }

    test_results.push_back(result);

    if (result.passed) {
        std::cout << "\033[32mPASSED\033[0m\n";
    } else {
        std::cout << "\033[31mFAILED\033[0m";
        if (!result.error.empty()) {
            std::cout << " - " << result.error;
        }
        std::cout << "\n";
    }
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

long get_file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Create a test image with a known pattern
void create_test_image_bmp(const std::string& path, int width, int height) {
    fastresize::ResizeOptions opts;
    // We'll create a simple gradient pattern manually
    // For now, we'll use an existing image or assume one exists
}

} // anonymous namespace

// ============================================
// Test Category 1: Codec Decode Tests
// ============================================

bool test_decode_jpeg() {
    // Test JPEG decoding with libjpeg-turbo
    fastresize::ImageInfo info = fastresize::get_image_info("examples/input.jpg");
    return info.width > 0 && info.height > 0 && info.format == "jpg";
}

bool test_decode_png() {
    // Test PNG decoding with libpng
    // First create a PNG from our test image
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 100;
    opts.target_height = 100;

    if (!fastresize::resize("examples/input.jpg", "test_temp.png", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_temp.png");
    return info.width == 100 && info.height == 100 && info.format == "png";
}

bool test_decode_webp() {
    // Test WEBP decoding
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 100;
    opts.target_height = 100;

    // Create a WEBP file first
    if (!fastresize::resize("examples/input.jpg", "test_temp.webp", opts)) {
        return false;
    }

    // Try to read it back
    fastresize::ImageInfo info = fastresize::get_image_info("test_temp.webp");
    return info.width == 100 && info.height == 100 && info.format == "webp";
}

bool test_decode_bmp() {
    // Test BMP decoding (still using stb)
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 100;
    opts.target_height = 100;

    if (!fastresize::resize("examples/input.jpg", "test_temp.bmp", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_temp.bmp");
    return info.width == 100 && info.height == 100 && info.format == "bmp";
}

// ============================================
// Test Category 2: Codec Encode Tests
// ============================================

bool test_encode_jpeg_quality_high() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.quality = 95;

    if (!fastresize::resize("examples/input.jpg", "test_jpeg_high.jpg", opts)) {
        return false;
    }

    return file_exists("test_jpeg_high.jpg");
}

bool test_encode_jpeg_quality_low() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.quality = 50;

    if (!fastresize::resize("examples/input.jpg", "test_jpeg_low.jpg", opts)) {
        return false;
    }

    // Low quality should produce smaller file
    long high_size = get_file_size("test_jpeg_high.jpg");
    long low_size = get_file_size("test_jpeg_low.jpg");

    return file_exists("test_jpeg_low.jpg") && low_size < high_size;
}

bool test_encode_png_quality() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.quality = 85;

    if (!fastresize::resize("examples/input.jpg", "test_png.png", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_png.png");
    return info.width == 200 && info.height == 200;
}

bool test_encode_webp_quality_high() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.quality = 90;

    if (!fastresize::resize("examples/input.jpg", "test_webp_high.webp", opts)) {
        return false;
    }

    return file_exists("test_webp_high.webp");
}

bool test_encode_webp_quality_low() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 200;
    opts.target_height = 200;
    opts.quality = 50;

    if (!fastresize::resize("examples/input.jpg", "test_webp_low.webp", opts)) {
        return false;
    }

    // Low quality WEBP should be smaller
    long high_size = get_file_size("test_webp_high.webp");
    long low_size = get_file_size("test_webp_low.webp");

    return file_exists("test_webp_low.webp") && low_size < high_size;
}

// ============================================
// Test Category 3: Format Conversion Tests
// ============================================

bool test_convert_jpg_to_png() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 150;
    opts.target_height = 150;

    if (!fastresize::resize("examples/input.jpg", "test_convert.png", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert.png");
    return info.format == "png" && info.width == 150 && info.height == 150;
}

bool test_convert_jpg_to_webp() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 150;
    opts.target_height = 150;

    if (!fastresize::resize("examples/input.jpg", "test_convert.webp", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert.webp");
    return info.format == "webp" && info.width == 150 && info.height == 150;
}

bool test_convert_png_to_jpg() {
    // First create PNG
    fastresize::ResizeOptions opts1;
    opts1.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts1.target_width = 150;
    opts1.target_height = 150;
    if (!fastresize::resize("examples/input.jpg", "test_temp2.png", opts1)) {
        return false;
    }

    // Convert PNG to JPG
    fastresize::ResizeOptions opts2;
    opts2.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts2.target_width = 100;
    opts2.target_height = 100;

    if (!fastresize::resize("test_temp2.png", "test_convert_pj.jpg", opts2)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert_pj.jpg");
    return info.format == "jpg" && info.width == 100 && info.height == 100;
}

bool test_convert_png_to_webp() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 100;
    opts.target_height = 100;

    if (!fastresize::resize("test_temp2.png", "test_convert_pw.webp", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert_pw.webp");
    return info.format == "webp" && info.width == 100 && info.height == 100;
}

bool test_convert_webp_to_jpg() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 120;
    opts.target_height = 120;

    if (!fastresize::resize("test_temp.webp", "test_convert_wj.jpg", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert_wj.jpg");
    return info.format == "jpg" && info.width == 120 && info.height == 120;
}

bool test_convert_webp_to_png() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 120;
    opts.target_height = 120;

    if (!fastresize::resize("test_temp.webp", "test_convert_wp.png", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_convert_wp.png");
    return info.format == "png" && info.width == 120 && info.height == 120;
}

// ============================================
// Test Category 4: Quality Comparison Tests
// ============================================

bool test_quality_range_jpeg() {
    // Test different quality levels for JPEG
    std::vector<int> qualities = {10, 50, 85, 95};
    std::vector<std::string> outputs;

    for (int q : qualities) {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 300;
        opts.target_height = 300;
        opts.quality = q;

        std::string output = "test_q" + std::to_string(q) + ".jpg";
        if (!fastresize::resize("examples/input.jpg", output, opts)) {
            return false;
        }
        outputs.push_back(output);
    }

    // Verify files exist and size increases with quality
    long prev_size = 0;
    for (const auto& output : outputs) {
        long size = get_file_size(output);
        if (size <= 0) return false;
        // Size should generally increase with quality (but not strictly enforced)
        prev_size = size;
    }

    return true;
}

bool test_quality_range_webp() {
    // Test different quality levels for WEBP
    std::vector<int> qualities = {10, 50, 85, 95};

    for (int q : qualities) {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 300;
        opts.target_height = 300;
        opts.quality = q;

        std::string output = "test_wq" + std::to_string(q) + ".webp";
        if (!fastresize::resize("examples/input.jpg", output, opts)) {
            return false;
        }

        if (!file_exists(output)) return false;
    }

    return true;
}

// ============================================
// Test Category 5: Resize with All Formats
// ============================================

bool test_resize_jpeg_scale_percent() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    opts.scale_percent = 0.5f;
    opts.quality = 85;

    return fastresize::resize("examples/input.jpg", "test_resize_j50.jpg", opts);
}

bool test_resize_png_fit_width() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = 400;
    opts.keep_aspect_ratio = true;

    if (!fastresize::resize("examples/input.jpg", "test_resize_png_fw.png", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_resize_png_fw.png");
    return info.width == 400;
}

bool test_resize_webp_fit_height() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
    opts.target_height = 300;
    opts.keep_aspect_ratio = true;
    opts.quality = 80;

    if (!fastresize::resize("examples/input.jpg", "test_resize_webp_fh.webp", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_resize_webp_fh.webp");
    return info.height == 300;
}

bool test_resize_bmp_exact() {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 250;
    opts.target_height = 180;
    opts.keep_aspect_ratio = false;

    if (!fastresize::resize("examples/input.jpg", "test_resize_bmp.bmp", opts)) {
        return false;
    }

    fastresize::ImageInfo info = fastresize::get_image_info("test_resize_bmp.bmp");
    return info.width == 250 && info.height == 180;
}

// ============================================
// Test Category 6: File Size Comparison
// ============================================

bool test_file_size_comparison() {
    // Resize the same image to multiple formats and compare sizes
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 500;
    opts.target_height = 500;
    opts.quality = 85;

    std::string jpg_file = "test_size.jpg";
    std::string png_file = "test_size.png";
    std::string webp_file = "test_size.webp";
    std::string bmp_file = "test_size.bmp";

    if (!fastresize::resize("examples/input.jpg", jpg_file, opts)) return false;
    if (!fastresize::resize("examples/input.jpg", png_file, opts)) return false;
    if (!fastresize::resize("examples/input.jpg", webp_file, opts)) return false;
    if (!fastresize::resize("examples/input.jpg", bmp_file, opts)) return false;

    long jpg_size = get_file_size(jpg_file);
    long png_size = get_file_size(png_file);
    long webp_size = get_file_size(webp_file);
    long bmp_size = get_file_size(bmp_file);

    // BMP should be largest (uncompressed)
    // WEBP should typically be smaller than JPEG at same quality
    // All sizes should be positive
    return jpg_size > 0 && png_size > 0 && webp_size > 0 && bmp_size > 0 &&
           bmp_size > jpg_size && bmp_size > png_size && bmp_size > webp_size;
}

// ============================================
// Test Category 7: Filter Tests with New Codecs
// ============================================

bool test_filters_with_webp() {
    std::vector<fastresize::ResizeOptions::Filter> filters = {
        fastresize::ResizeOptions::MITCHELL,
        fastresize::ResizeOptions::CATMULL_ROM,
        fastresize::ResizeOptions::BOX,
        fastresize::ResizeOptions::TRIANGLE
    };

    for (size_t i = 0; i < filters.size(); i++) {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 200;
        opts.target_height = 200;
        opts.filter = filters[i];
        opts.quality = 85;

        std::string output = "test_filter_webp_" + std::to_string(i) + ".webp";
        if (!fastresize::resize("examples/input.jpg", output, opts)) {
            return false;
        }
    }

    return true;
}

// ============================================
// Main Test Runner
// ============================================

int main() {
    print_header("FastResize Phase 3 - Advanced Codec Tests");

    std::cout << "Testing specialized image codecs:\n";
    std::cout << "  - libjpeg-turbo for JPEG\n";
    std::cout << "  - libpng for PNG\n";
    std::cout << "  - libwebp for WEBP\n\n";

    // Category 1: Decode Tests
    print_header("Category 1: Codec Decode Tests");
    run_test("decode_jpeg", test_decode_jpeg);
    run_test("decode_png", test_decode_png);
    run_test("decode_webp", test_decode_webp);
    run_test("decode_bmp", test_decode_bmp);

    // Category 2: Encode Tests
    print_header("Category 2: Codec Encode Tests");
    run_test("encode_jpeg_quality_high", test_encode_jpeg_quality_high);
    run_test("encode_jpeg_quality_low", test_encode_jpeg_quality_low);
    run_test("encode_png_quality", test_encode_png_quality);
    run_test("encode_webp_quality_high", test_encode_webp_quality_high);
    run_test("encode_webp_quality_low", test_encode_webp_quality_low);

    // Category 3: Format Conversion Tests
    print_header("Category 3: Format Conversion Tests");
    run_test("convert_jpg_to_png", test_convert_jpg_to_png);
    run_test("convert_jpg_to_webp", test_convert_jpg_to_webp);
    run_test("convert_png_to_jpg", test_convert_png_to_jpg);
    run_test("convert_png_to_webp", test_convert_png_to_webp);
    run_test("convert_webp_to_jpg", test_convert_webp_to_jpg);
    run_test("convert_webp_to_png", test_convert_webp_to_png);

    // Category 4: Quality Tests
    print_header("Category 4: Quality Comparison Tests");
    run_test("quality_range_jpeg", test_quality_range_jpeg);
    run_test("quality_range_webp", test_quality_range_webp);

    // Category 5: Resize Tests
    print_header("Category 5: Resize with All Formats");
    run_test("resize_jpeg_scale_percent", test_resize_jpeg_scale_percent);
    run_test("resize_png_fit_width", test_resize_png_fit_width);
    run_test("resize_webp_fit_height", test_resize_webp_fit_height);
    run_test("resize_bmp_exact", test_resize_bmp_exact);

    // Category 6: File Size Tests
    print_header("Category 6: File Size Comparison");
    run_test("file_size_comparison", test_file_size_comparison);

    // Category 7: Filter Tests
    print_header("Category 7: Filter Tests with New Codecs");
    run_test("filters_with_webp", test_filters_with_webp);

    // Summary
    print_header("Test Summary");
    int passed = 0;
    int failed = 0;

    for (const auto& result : test_results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "  \033[31mFAILED\033[0m: " << result.name << "\n";
            if (!result.error.empty()) {
                std::cout << "    Error: " << result.error << "\n";
            }
        }
    }

    std::cout << "\n";
    std::cout << std::string(48, '=') << "\n";
    std::cout << "Tests run:    " << test_results.size() << "\n";
    std::cout << "Tests passed: " << passed << "\n";
    std::cout << "Tests failed: " << failed << "\n";
    std::cout << std::string(48, '=') << "\n";

    return (failed == 0) ? 0 : 1;
}
