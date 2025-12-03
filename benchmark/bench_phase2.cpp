// bench_phase2.cpp - Phase 2 Performance Benchmarks
// Measures resize performance for different scenarios

#include <fastresize.h>
#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include <vector>
#include <string>

// ============================================
// Timing Utilities
// ============================================

double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// ============================================
// Test Image Generator
// ============================================

bool generate_test_bmp(const std::string& filename, int width, int height) {
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
        0, 0, 0, 0,         // Image size
        0, 0, 0, 0,         // X pixels per meter
        0, 0, 0, 0,         // Y pixels per meter
        0, 0, 0, 0,         // Colors in palette
        0, 0, 0, 0          // Important colors
    };

    int row_size = ((width * 3 + 3) / 4) * 4;
    int pixel_data_size = row_size * height;
    int file_size = 54 + pixel_data_size;

    *(int*)(&bmp_header[2]) = file_size;
    *(int*)(&bmp_header[18]) = width;
    *(int*)(&bmp_header[22]) = height;

    FILE* f = fopen(filename.c_str(), "wb");
    if (!f) return false;

    fwrite(bmp_header, 1, 54, f);

    unsigned char* row = new unsigned char[row_size];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = x * 3;
            row[offset + 0] = (unsigned char)((x * 255) / width);
            row[offset + 1] = (unsigned char)((y * 255) / height);
            row[offset + 2] = (unsigned char)(((x + y) * 255) / (width + height));
        }
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
// Benchmark Functions
// ============================================

void benchmark_single_resize(const char* name, int in_w, int in_h, int out_w, int out_h) {
    char input_file[256];
    char output_file[256];
    sprintf(input_file, "/tmp/bench_input_%dx%d.bmp", in_w, in_h);
    sprintf(output_file, "/tmp/bench_output_%dx%d.bmp", out_w, out_h);

    printf("Benchmark: %s\n", name);
    printf("  Input:  %dx%d\n", in_w, in_h);
    printf("  Output: %dx%d\n", out_w, out_h);

    // Generate test image
    if (!generate_test_bmp(input_file, in_w, in_h)) {
        printf("  ERROR: Failed to generate test image\n\n");
        return;
    }

    // Warm-up run
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = out_w;
    opts.target_height = out_h;
    fastresize::resize(input_file, output_file, opts);

    // Benchmark run (10 iterations)
    const int iterations = 10;
    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        double start = get_time_ms();
        bool success = fastresize::resize(input_file, output_file, opts);
        double end = get_time_ms();

        if (!success) {
            printf("  ERROR: Resize failed on iteration %d\n", i);
            break;
        }

        total_time += (end - start);
    }

    double avg_time = total_time / iterations;
    printf("  Average time: %.2f ms (over %d iterations)\n", avg_time, iterations);

    // Calculate throughput
    double input_mpixels = (in_w * in_h) / 1000000.0;
    double throughput = input_mpixels / (avg_time / 1000.0);
    printf("  Throughput: %.2f megapixels/sec\n", throughput);

    // Cleanup
    remove(input_file);
    remove(output_file);
    printf("\n");
}

void benchmark_filter_comparison(int in_w, int in_h, int out_w, int out_h) {
    char input_file[256];
    char output_file[256];
    sprintf(input_file, "/tmp/bench_filter_input_%dx%d.bmp", in_w, in_h);
    sprintf(output_file, "/tmp/bench_filter_output_%dx%d.bmp", out_w, out_h);

    printf("Filter Comparison Benchmark\n");
    printf("  Input:  %dx%d\n", in_w, in_h);
    printf("  Output: %dx%d\n\n", out_w, out_h);

    if (!generate_test_bmp(input_file, in_w, in_h)) {
        printf("  ERROR: Failed to generate test image\n\n");
        return;
    }

    struct FilterTest {
        const char* name;
        fastresize::ResizeOptions::Filter filter;
    };

    FilterTest filters[] = {
        {"Mitchell", fastresize::ResizeOptions::MITCHELL},
        {"Catmull-Rom", fastresize::ResizeOptions::CATMULL_ROM},
        {"Box", fastresize::ResizeOptions::BOX},
        {"Triangle", fastresize::ResizeOptions::TRIANGLE}
    };

    const int iterations = 10;

    for (int f = 0; f < 4; f++) {
        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = out_w;
        opts.target_height = out_h;
        opts.filter = filters[f].filter;

        // Warm-up
        fastresize::resize(input_file, output_file, opts);

        double total_time = 0.0;
        for (int i = 0; i < iterations; i++) {
            double start = get_time_ms();
            fastresize::resize(input_file, output_file, opts);
            double end = get_time_ms();
            total_time += (end - start);
        }

        double avg_time = total_time / iterations;
        printf("  %12s: %.2f ms\n", filters[f].name, avg_time);
    }

    remove(input_file);
    remove(output_file);
    printf("\n");
}

void benchmark_upscale_vs_downscale() {
    printf("Upscale vs Downscale Benchmark\n\n");

    // Downscale: 2000x2000 -> 800x600
    benchmark_single_resize("Downscale 2.5x", 2000, 2000, 800, 800);

    // Upscale: 400x400 -> 2000x2000
    benchmark_single_resize("Upscale 5x", 400, 400, 2000, 2000);
}

void benchmark_aspect_ratios() {
    printf("Various Aspect Ratios Benchmark\n\n");

    benchmark_single_resize("Square (1:1)", 1000, 1000, 500, 500);
    benchmark_single_resize("Wide (16:9)", 1920, 1080, 960, 540);
    benchmark_single_resize("Tall (9:16)", 1080, 1920, 540, 960);
    benchmark_single_resize("Ultra-wide (21:9)", 2560, 1080, 1280, 540);
}

// ============================================
// Main Benchmark Runner
// ============================================

int main() {
    printf("\n");
    printf("========================================\n");
    printf("FastResize Phase 2 - Performance Benchmarks\n");
    printf("========================================\n");
    printf("\n");

    // Standard resize benchmarks
    printf("----------------------------------------\n");
    printf("Standard Resize Operations\n");
    printf("----------------------------------------\n");
    printf("\n");

    benchmark_single_resize("Small (100x100 -> 50x50)", 100, 100, 50, 50);
    benchmark_single_resize("Medium (800x600 -> 400x300)", 800, 600, 400, 300);
    benchmark_single_resize("Large (2000x2000 -> 800x600)", 2000, 2000, 800, 600);
    benchmark_single_resize("Very Large (3000x2000 -> 1200x800)", 3000, 2000, 1200, 800);

    // Filter comparison
    printf("----------------------------------------\n");
    printf("Filter Performance\n");
    printf("----------------------------------------\n");
    printf("\n");

    benchmark_filter_comparison(2000, 2000, 800, 800);

    // Upscale vs downscale
    printf("----------------------------------------\n");
    printf("Scaling Direction\n");
    printf("----------------------------------------\n");
    printf("\n");

    benchmark_upscale_vs_downscale();

    // Aspect ratios
    printf("----------------------------------------\n");
    printf("Different Aspect Ratios\n");
    printf("----------------------------------------\n");
    printf("\n");

    benchmark_aspect_ratios();

    printf("========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}
