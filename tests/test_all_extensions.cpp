/**
 * Comprehensive Extension Test
 *
 * Tests all supported file extensions:
 * - .jpg
 * - .jpeg
 * - .png
 * - .webp
 * - .bmp
 */

#include <fastresize.h>
#include <iostream>
#include <string>
#include <vector>

struct TestResult {
    std::string extension;
    bool encode_passed;
    bool decode_passed;
    bool round_trip_passed;
    std::string error;
};

bool test_extension(const std::string& ext, TestResult& result) {
    result.extension = ext;
    result.encode_passed = false;
    result.decode_passed = false;
    result.round_trip_passed = false;

    std::string input = "../examples/input.jpg";
    std::string output1 = "test_ext_output." + ext;
    std::string output2 = "test_ext_roundtrip." + ext;

    // Step 1: Encode (JPG -> ext)
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 400;
    opts.quality = 85;

    if (!fastresize::resize(input, output1, opts)) {
        result.error = "Failed to encode to ." + ext;
        return false;
    }
    result.encode_passed = true;

    // Step 2: Decode (ext)
    fastresize::ImageInfo info = fastresize::get_image_info(output1);
    if (info.width != 400 || info.height != 400) {
        result.error = "Decode failed or wrong dimensions: " +
                       std::to_string(info.width) + "x" + std::to_string(info.height);
        return false;
    }
    result.decode_passed = true;

    // Step 3: Round trip (ext -> ext)
    opts.target_width = 200;
    opts.target_height = 200;

    if (!fastresize::resize(output1, output2, opts)) {
        result.error = "Failed round-trip resize";
        return false;
    }

    info = fastresize::get_image_info(output2);
    if (info.width != 200 || info.height != 200) {
        result.error = "Round-trip decode failed or wrong dimensions";
        return false;
    }

    result.round_trip_passed = true;
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "FastResize - Comprehensive Extension Test\n";
    std::cout << "==========================================\n\n";

    std::vector<std::string> extensions = {"jpg", "jpeg", "png", "webp", "bmp"};
    std::vector<TestResult> results;

    for (const auto& ext : extensions) {
        std::cout << "Testing ." << ext << " extension... " << std::flush;

        TestResult result;
        bool success = test_extension(ext, result);
        results.push_back(result);

        if (success) {
            std::cout << "\033[32mPASSED\033[0m\n";
        } else {
            std::cout << "\033[31mFAILED\033[0m - " << result.error << "\n";
        }
    }

    // Summary
    std::cout << "\n";
    std::cout << "==========================================\n";
    std::cout << "Summary:\n";
    std::cout << "==========================================\n\n";

    std::cout << "Extension  Encode  Decode  Round-trip\n";
    std::cout << "----------------------------------------\n";

    int total_passed = 0;
    for (const auto& r : results) {
        std::cout << "." << r.extension;
        std::cout << std::string(10 - r.extension.length(), ' ');

        std::cout << (r.encode_passed ? "   ✓" : "   ✗");
        std::cout << "       ";
        std::cout << (r.decode_passed ? "✓" : "✗");
        std::cout << "          ";
        std::cout << (r.round_trip_passed ? "✓" : "✗");
        std::cout << "\n";

        if (r.encode_passed && r.decode_passed && r.round_trip_passed) {
            total_passed++;
        }
    }

    std::cout << "\n";
    std::cout << "==========================================\n";
    std::cout << "Total: " << total_passed << "/" << extensions.size() << " extensions passed\n";
    std::cout << "==========================================\n";

    return (total_passed == (int)extensions.size()) ? 0 : 1;
}
