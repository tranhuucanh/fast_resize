/**
 * FastResize - Phase 4 Benchmarks
 * Performance benchmarks for batch processing with threading
 */

#include <fastresize.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <unistd.h>

// Don't define IMPLEMENTATION - already in library
#include "../include/stb_image_write.h"

// Helper functions
static bool create_directory(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return true;
    }
    return mkdir(path, 0755) == 0;
}

static bool directory_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode));
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

static bool create_test_image(const char* path, int width, int height) {
    int channels = 3;
    unsigned char* pixels = new unsigned char[width * height * channels];

    // Create gradient pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * channels;
            pixels[idx + 0] = (x * 255) / width;
            pixels[idx + 1] = (y * 255) / height;
            pixels[idx + 2] = 128;
        }
    }

    int result = stbi_write_jpg(path, width, height, channels, pixels, 85);
    delete[] pixels;
    return result != 0;
}

// Benchmark helper
class Timer {
public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start_;
        return elapsed.count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

void print_header(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void print_result(const std::string& name, int num_images, double time_ms,
                  int input_mp, int output_mp) {
    double throughput_input = (num_images * input_mp * 1000.0) / time_ms;
    double time_per_image = time_ms / num_images;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << name << ":" << std::endl;
    std::cout << "  Images: " << num_images << std::endl;
    std::cout << "  Total time: " << time_ms << " ms" << std::endl;
    std::cout << "  Time per image: " << time_per_image << " ms" << std::endl;
    std::cout << "  Throughput: " << throughput_input << " MP/s (input basis)" << std::endl;
    std::cout << std::endl;
}

// ============================================
// Benchmark 1: Thread Scaling (1, 2, 4, 8 threads)
// ============================================

void bench_thread_scaling() {
    print_header("Thread Scaling Benchmark");

    const int num_images = 100;
    const char* input_dir = "bench_thread_input";
    const char* output_dir = "bench_thread_output";

    // Setup
    cleanup_directory(input_dir);
    create_directory(input_dir);

    std::cout << "Creating " << num_images << " test images (800x600)..." << std::endl;
    std::vector<std::string> input_paths;
    for (int i = 0; i < num_images; i++) {
        std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
        create_test_image(path.c_str(), 800, 600);
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 300;

    int thread_counts[] = {1, 2, 4, 8};

    std::cout << "\nResizing to 400x300:\n" << std::endl;

    for (int threads : thread_counts) {
        cleanup_directory(output_dir);
        create_directory(output_dir);

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = threads;

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();

        std::string name = std::to_string(threads) + " thread" + (threads > 1 ? "s" : "");
        int input_mp = 0;  // 800 * 600 / 1000000
        int output_mp = 0; // 400 * 300 / 1000000

        std::cout << std::fixed << std::setprecision(2);
        std::cout << name << ": " << time_ms << " ms";
        std::cout << " (" << (time_ms / num_images) << " ms/image)" << std::endl;
    }

    // Cleanup
    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 2: Batch Size Scaling
// ============================================

void bench_batch_size_scaling() {
    print_header("Batch Size Scaling Benchmark");

    const char* input_dir = "bench_size_input";
    const char* output_dir = "bench_size_output";

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 300;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 8;

    int batch_sizes[] = {10, 50, 100, 200};

    std::cout << "Testing with 8 threads, 800x600 -> 400x300:\n" << std::endl;

    for (int size : batch_sizes) {
        cleanup_directory(input_dir);
        cleanup_directory(output_dir);
        create_directory(input_dir);
        create_directory(output_dir);

        // Create test images
        std::vector<std::string> input_paths;
        for (int i = 0; i < size; i++) {
            std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
            create_test_image(path.c_str(), 800, 600);
            input_paths.push_back(path);
        }

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << size << " images: " << time_ms << " ms";
        std::cout << " (" << (time_ms / size) << " ms/image)" << std::endl;
    }

    // Cleanup
    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 3: 300 Images Target Performance
// ============================================

void bench_300_images_target() {
    print_header("300 Images Performance Test (Primary Target)");

    const int num_images = 300;
    const char* input_dir = "bench_300_input";
    const char* output_dir = "bench_300_output";

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
    create_directory(input_dir);
    create_directory(output_dir);

    std::cout << "Creating " << num_images << " test images (2000x2000)..." << std::endl;
    std::vector<std::string> input_paths;
    for (int i = 0; i < num_images; i++) {
        std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
        create_test_image(path.c_str(), 2000, 2000);
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;

    std::cout << "\nResizing 300 images (2000x2000 -> 800x600):\n" << std::endl;

    // Test with different thread counts
    int thread_counts[] = {4, 8};

    for (int threads : thread_counts) {
        cleanup_directory(output_dir);
        create_directory(output_dir);

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = threads;

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();
        double time_sec = time_ms / 1000.0;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << threads << " threads: " << time_ms << " ms (" << time_sec << " sec)" << std::endl;
        std::cout << "  Per image: " << (time_ms / num_images) << " ms" << std::endl;
        std::cout << "  Target: < 3000 ms - " << (time_ms < 3000 ? "PASS ✓" : "FAIL ✗") << std::endl;
        std::cout << std::endl;
    }

    // Cleanup
    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 4: Different Image Sizes
// ============================================

void bench_different_sizes() {
    print_header("Different Image Size Performance");

    const int num_images = 100;
    const char* input_dir = "bench_sizes_input";
    const char* output_dir = "bench_sizes_output";

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 8;

    struct TestCase {
        const char* name;
        int input_w, input_h;
        int output_w, output_h;
    };

    TestCase cases[] = {
        {"Small (400x300 -> 200x150)", 400, 300, 200, 150},
        {"Medium (800x600 -> 400x300)", 800, 600, 400, 300},
        {"Large (1920x1080 -> 960x540)", 1920, 1080, 960, 540},
        {"Very Large (2000x2000 -> 800x600)", 2000, 2000, 800, 600},
    };

    for (const auto& test : cases) {
        cleanup_directory(input_dir);
        cleanup_directory(output_dir);
        create_directory(input_dir);
        create_directory(output_dir);

        // Create test images
        std::vector<std::string> input_paths;
        for (int i = 0; i < num_images; i++) {
            std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
            create_test_image(path.c_str(), test.input_w, test.input_h);
            input_paths.push_back(path);
        }

        fastresize::ResizeOptions opts;
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = test.output_w;
        opts.target_height = test.output_h;

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << test.name << ":" << std::endl;
        std::cout << "  Total: " << time_ms << " ms" << std::endl;
        std::cout << "  Per image: " << (time_ms / num_images) << " ms" << std::endl;
        std::cout << std::endl;
    }

    // Cleanup
    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 5: Sequential vs Parallel
// ============================================

void bench_sequential_vs_parallel() {
    print_header("Sequential vs Parallel Comparison");

    const int num_images = 100;
    const char* input_dir = "bench_compare_input";
    const char* output_dir = "bench_compare_output";

    cleanup_directory(input_dir);
    create_directory(input_dir);

    std::cout << "Creating " << num_images << " test images (1000x1000)..." << std::endl;
    std::vector<std::string> input_paths;
    for (int i = 0; i < num_images; i++) {
        std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
        create_test_image(path.c_str(), 1000, 1000);
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 400;

    std::cout << "\nResizing to 400x400:\n" << std::endl;

    // Sequential (1 thread)
    {
        cleanup_directory(output_dir);
        create_directory(output_dir);

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = 1;

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Sequential (1 thread): " << time_ms << " ms" << std::endl;
    }

    // Parallel (8 threads)
    {
        cleanup_directory(output_dir);
        create_directory(output_dir);

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = 8;

        Timer timer;
        timer.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        double time_ms = timer.elapsed_ms();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Parallel (8 threads): " << time_ms << " ms" << std::endl;
    }

    // Cleanup
    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Main
// ============================================

int main() {
    std::cout << "FastResize Phase 4 - Batch Processing Benchmarks" << std::endl;
    std::cout << "=================================================" << std::endl;

    bench_thread_scaling();
    bench_batch_size_scaling();
    bench_300_images_target();
    bench_different_sizes();
    bench_sequential_vs_parallel();

    std::cout << "\n=================================================" << std::endl;
    std::cout << "All benchmarks completed!" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
