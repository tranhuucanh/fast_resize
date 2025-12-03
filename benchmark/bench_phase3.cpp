/**
 * FastResize Phase 3 - Codec Performance Benchmark
 *
 * Compares performance of specialized codecs vs stb:
 * - JPEG: libjpeg-turbo vs stb_image
 * - PNG: libpng vs stb_image
 * - WEBP: libwebp (new capability)
 *
 * Benchmark categories:
 * 1. Decode performance (by format)
 * 2. Encode performance (by format)
 * 3. Full resize pipeline (decode + resize + encode)
 * 4. Quality vs performance tradeoff
 */

#include <fastresize.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

using namespace std::chrono;

// Benchmark utilities
namespace {

struct BenchResult {
    std::string name;
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double throughput_mpps;  // megapixels per second
    long file_size;
};

std::vector<BenchResult> results;

template<typename Func>
BenchResult benchmark(const std::string& name, Func func, int iterations, int width, int height) {
    std::vector<double> times;
    times.reserve(iterations);

    // Warm-up
    func();

    // Benchmark
    for (int i = 0; i < iterations; ++i) {
        auto start = high_resolution_clock::now();
        func();
        auto end = high_resolution_clock::now();

        double elapsed_ms = duration_cast<nanoseconds>(end - start).count() / 1e6;
        times.push_back(elapsed_ms);
    }

    // Calculate statistics
    double sum = 0.0;
    double min_time = times[0];
    double max_time = times[0];

    for (double t : times) {
        sum += t;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
    }

    double avg_time = sum / iterations;
    double megapixels = (width * height) / 1e6;
    double throughput = megapixels / (avg_time / 1000.0);  // MP/s

    BenchResult result;
    result.name = name;
    result.avg_time_ms = avg_time;
    result.min_time_ms = min_time;
    result.max_time_ms = max_time;
    result.throughput_mpps = throughput;
    result.file_size = 0;

    return result;
}

void print_result(const BenchResult& result) {
    std::cout << std::left << std::setw(40) << result.name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << result.avg_time_ms << " ms";

    if (result.throughput_mpps > 0) {
        std::cout << std::setw(12) << std::fixed << std::setprecision(2)
                  << result.throughput_mpps << " MP/s";
    }

    if (result.file_size > 0) {
        std::cout << std::setw(12) << (result.file_size / 1024) << " KB";
    }

    std::cout << "\n";
}

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

long get_file_size(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

} // anonymous namespace

// ============================================
// Benchmark 1: Decode Performance Comparison
// ============================================

void benchmark_decode_performance() {
    print_header("Decode Performance (JPEG, PNG, WEBP, BMP)");

    const int ITERATIONS = 50;

    std::cout << std::left << std::setw(40) << "Operation"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "Throughput"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    // First create test files of the same content in different formats
    {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 1000;
        opts.target_height = 1000;
        opts.quality = 85;

        fastresize::resize("examples/input.jpg", "bench_test.jpg", opts);
        fastresize::resize("examples/input.jpg", "bench_test.png", opts);
        fastresize::resize("examples/input.jpg", "bench_test.webp", opts);
        fastresize::resize("examples/input.jpg", "bench_test.bmp", opts);
    }

    // Benchmark JPEG decode
    auto jpeg_result = benchmark("JPEG Decode (1000x1000)", []() {
        fastresize::ImageInfo info = fastresize::get_image_info("bench_test.jpg");
    }, ITERATIONS, 1000, 1000);
    print_result(jpeg_result);

    // Benchmark PNG decode
    auto png_result = benchmark("PNG Decode (1000x1000)", []() {
        fastresize::ImageInfo info = fastresize::get_image_info("bench_test.png");
    }, ITERATIONS, 1000, 1000);
    print_result(png_result);

    // Benchmark WEBP decode
    auto webp_result = benchmark("WEBP Decode (1000x1000)", []() {
        fastresize::ImageInfo info = fastresize::get_image_info("bench_test.webp");
    }, ITERATIONS, 1000, 1000);
    print_result(webp_result);

    // Benchmark BMP decode
    auto bmp_result = benchmark("BMP Decode (1000x1000)", []() {
        fastresize::ImageInfo info = fastresize::get_image_info("bench_test.bmp");
    }, ITERATIONS, 1000, 1000);
    print_result(bmp_result);
}

// ============================================
// Benchmark 2: Encode Performance Comparison
// ============================================

void benchmark_encode_performance() {
    print_header("Encode Performance (JPEG, PNG, WEBP, BMP)");

    const int ITERATIONS = 30;

    std::cout << std::left << std::setw(40) << "Operation"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "Throughput"
              << std::setw(12) << "File Size"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    // Benchmark JPEG encode
    auto jpeg_result = benchmark("JPEG Encode (1000x1000, Q=85)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 1000;
        opts.target_height = 1000;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_encode.jpg", opts);
    }, ITERATIONS, 1000, 1000);
    jpeg_result.file_size = get_file_size("bench_encode.jpg");
    print_result(jpeg_result);

    // Benchmark PNG encode
    auto png_result = benchmark("PNG Encode (1000x1000)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 1000;
        opts.target_height = 1000;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_encode.png", opts);
    }, ITERATIONS, 1000, 1000);
    png_result.file_size = get_file_size("bench_encode.png");
    print_result(png_result);

    // Benchmark WEBP encode
    auto webp_result = benchmark("WEBP Encode (1000x1000, Q=85)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 1000;
        opts.target_height = 1000;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_encode.webp", opts);
    }, ITERATIONS, 1000, 1000);
    webp_result.file_size = get_file_size("bench_encode.webp");
    print_result(webp_result);

    // Benchmark BMP encode
    auto bmp_result = benchmark("BMP Encode (1000x1000)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 1000;
        opts.target_height = 1000;
        fastresize::resize("examples/input.jpg", "bench_encode.bmp", opts);
    }, ITERATIONS, 1000, 1000);
    bmp_result.file_size = get_file_size("bench_encode.bmp");
    print_result(bmp_result);
}

// ============================================
// Benchmark 3: Full Pipeline Performance
// ============================================

void benchmark_full_pipeline() {
    print_header("Full Pipeline: Decode -> Resize -> Encode");

    const int ITERATIONS = 20;

    std::cout << std::left << std::setw(40) << "Pipeline"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "Throughput"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    // JPG -> JPG
    auto jpg_jpg = benchmark("JPG->JPG (2000x2000 -> 800x600)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 800;
        opts.target_height = 600;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_pipeline_jj.jpg", opts);
    }, ITERATIONS, 2000, 2000);
    print_result(jpg_jpg);

    // JPG -> PNG
    auto jpg_png = benchmark("JPG->PNG (2000x2000 -> 800x600)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 800;
        opts.target_height = 600;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_pipeline_jp.png", opts);
    }, ITERATIONS, 2000, 2000);
    print_result(jpg_png);

    // JPG -> WEBP
    auto jpg_webp = benchmark("JPG->WEBP (2000x2000 -> 800x600)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 800;
        opts.target_height = 600;
        opts.quality = 85;
        fastresize::resize("examples/input.jpg", "bench_pipeline_jw.webp", opts);
    }, ITERATIONS, 2000, 2000);
    print_result(jpg_webp);

    // PNG -> JPG
    auto png_jpg = benchmark("PNG->JPG (1000x1000 -> 500x500)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 500;
        opts.target_height = 500;
        opts.quality = 85;
        fastresize::resize("bench_test.png", "bench_pipeline_pj.jpg", opts);
    }, ITERATIONS, 1000, 1000);
    print_result(png_jpg);

    // PNG -> WEBP
    auto png_webp = benchmark("PNG->WEBP (1000x1000 -> 500x500)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 500;
        opts.target_height = 500;
        opts.quality = 85;
        fastresize::resize("bench_test.png", "bench_pipeline_pw.webp", opts);
    }, ITERATIONS, 1000, 1000);
    print_result(png_webp);

    // WEBP -> JPG
    auto webp_jpg = benchmark("WEBP->JPG (1000x1000 -> 500x500)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 500;
        opts.target_height = 500;
        opts.quality = 85;
        fastresize::resize("bench_test.webp", "bench_pipeline_wj.jpg", opts);
    }, ITERATIONS, 1000, 1000);
    print_result(webp_jpg);

    // WEBP -> PNG
    auto webp_png = benchmark("WEBP->PNG (1000x1000 -> 500x500)", []() {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = 500;
        opts.target_height = 500;
        opts.quality = 85;
        fastresize::resize("bench_test.webp", "bench_pipeline_wp.png", opts);
    }, ITERATIONS, 1000, 1000);
    print_result(webp_png);
}

// ============================================
// Benchmark 4: Quality vs Performance
// ============================================

void benchmark_quality_vs_performance() {
    print_header("Quality vs Performance Tradeoff");

    const int ITERATIONS = 20;
    std::vector<int> qualities = {10, 50, 75, 90, 95};

    // JPEG quality test
    std::cout << "\nJPEG Quality (2000x2000 -> 1000x1000):\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << std::left << std::setw(15) << "Quality"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "File Size"
              << std::setw(15) << "Compression"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    for (int q : qualities) {
        std::string name = "Q=" + std::to_string(q);
        auto result = benchmark(name, [q]() {
            fastresize::ResizeOptions opts;
            opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
            opts.target_width = 1000;
            opts.target_height = 1000;
            opts.quality = q;
            fastresize::resize("examples/input.jpg", "bench_quality_j.jpg", opts);
        }, ITERATIONS, 2000, 2000);

        result.file_size = get_file_size("bench_quality_j.jpg");
        long uncompressed = 1000 * 1000 * 3;  // RGB
        double compression = (double)uncompressed / result.file_size;

        std::cout << std::left << std::setw(15) << name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << result.avg_time_ms << " ms"
                  << std::setw(12) << (result.file_size / 1024) << " KB"
                  << std::setw(15) << std::fixed << std::setprecision(1)
                  << compression << "x\n";
    }

    // WEBP quality test
    std::cout << "\nWEBP Quality (2000x2000 -> 1000x1000):\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << std::left << std::setw(15) << "Quality"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "File Size"
              << std::setw(15) << "Compression"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    for (int q : qualities) {
        std::string name = "Q=" + std::to_string(q);
        auto result = benchmark(name, [q]() {
            fastresize::ResizeOptions opts;
            opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
            opts.target_width = 1000;
            opts.target_height = 1000;
            opts.quality = q;
            fastresize::resize("examples/input.jpg", "bench_quality_w.webp", opts);
        }, ITERATIONS, 2000, 2000);

        result.file_size = get_file_size("bench_quality_w.webp");
        long uncompressed = 1000 * 1000 * 3;  // RGB
        double compression = (double)uncompressed / result.file_size;

        std::cout << std::left << std::setw(15) << name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << result.avg_time_ms << " ms"
                  << std::setw(12) << (result.file_size / 1024) << " KB"
                  << std::setw(15) << std::fixed << std::setprecision(1)
                  << compression << "x\n";
    }
}

// ============================================
// Benchmark 5: Format Comparison (Same Quality)
// ============================================

void benchmark_format_comparison() {
    print_header("Format Comparison at Quality=85");

    const int ITERATIONS = 20;

    std::cout << std::left << std::setw(15) << "Format"
              << std::right << std::setw(10) << "Avg Time"
              << std::setw(12) << "Throughput"
              << std::setw(12) << "File Size"
              << std::setw(15) << "Size Ratio"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    // All formats at same quality
    struct FormatBench {
        std::string name;
        std::string ext;
        BenchResult result;
    };

    std::vector<FormatBench> formats = {
        {"JPEG", "jpg", {}},
        {"PNG", "png", {}},
        {"WEBP", "webp", {}},
        {"BMP", "bmp", {}}
    };

    for (auto& fmt : formats) {
        std::string output = "bench_fmt." + fmt.ext;
        fmt.result = benchmark(fmt.name, [output]() {
            fastresize::ResizeOptions opts;
            opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
            opts.target_width = 1000;
            opts.target_height = 1000;
            opts.quality = 85;
            fastresize::resize("examples/input.jpg", output, opts);
        }, ITERATIONS, 2000, 2000);

        fmt.result.file_size = get_file_size(output);
    }

    // Find smallest for ratio comparison
    long smallest = formats[0].result.file_size;
    for (const auto& fmt : formats) {
        if (fmt.result.file_size < smallest) {
            smallest = fmt.result.file_size;
        }
    }

    // Print results
    for (const auto& fmt : formats) {
        double size_ratio = (double)fmt.result.file_size / smallest;

        std::cout << std::left << std::setw(15) << fmt.name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << fmt.result.avg_time_ms << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << fmt.result.throughput_mpps << " MP/s"
                  << std::setw(12) << (fmt.result.file_size / 1024) << " KB"
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << size_ratio << "x\n";
    }
}

// ============================================
// Main
// ============================================

int main() {
    std::cout << "\n";
    std::cout << "FastResize Phase 3 - Codec Performance Benchmark\n";
    std::cout << "==================================================\n";
    std::cout << "\n";
    std::cout << "Testing specialized codec performance:\n";
    std::cout << "  - libjpeg-turbo for JPEG\n";
    std::cout << "  - libpng for PNG\n";
    std::cout << "  - libwebp for WEBP\n";

    // Run benchmarks
    benchmark_decode_performance();
    benchmark_encode_performance();
    benchmark_full_pipeline();
    benchmark_quality_vs_performance();
    benchmark_format_comparison();

    std::cout << "\n";
    std::cout << "Benchmark complete!\n";
    std::cout << "==================================================\n";

    return 0;
}
