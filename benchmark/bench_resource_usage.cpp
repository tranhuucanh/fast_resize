/**
 * FastResize - Resource Usage Benchmark
 * Measures CPU and RAM usage for single and batch resize operations
 */

#include <fastresize.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <unistd.h>

// macOS specific includes for resource monitoring
#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

// Don't define IMPLEMENTATION - already in library
#include "../include/stb_image_write.h"

// Resource usage tracker
struct ResourceUsage {
    double cpu_percent;
    size_t ram_used_mb;
    size_t ram_peak_mb;
    double wall_time_ms;
    double cpu_time_ms;
};

// Helper to get current process memory usage (macOS)
size_t get_memory_usage_mb() {
    struct mach_task_basic_info info;
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;

    kern_return_t kerr = task_info(mach_task_self(),
                                    MACH_TASK_BASIC_INFO,
                                    (task_info_t)&info,
                                    &size);

    if (kerr == KERN_SUCCESS) {
        return info.resident_size / (1024 * 1024); // Convert to MB
    }
    return 0;
}

// Helper to get CPU time (user + system)
double get_cpu_time_ms() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    double user_ms = usage.ru_utime.tv_sec * 1000.0 + usage.ru_utime.tv_usec / 1000.0;
    double sys_ms = usage.ru_stime.tv_sec * 1000.0 + usage.ru_stime.tv_usec / 1000.0;

    return user_ms + sys_ms;
}

// Monitor resources during operation
class ResourceMonitor {
public:
    ResourceMonitor() : monitoring_(false), peak_memory_(0) {}

    void start() {
        monitoring_ = true;
        peak_memory_ = 0;
        start_memory_ = get_memory_usage_mb();
        start_cpu_time_ = get_cpu_time_ms();
        start_wall_time_ = std::chrono::high_resolution_clock::now();

        // Start monitoring thread
        monitor_thread_ = std::thread([this]() {
            while (monitoring_) {
                size_t current_memory = get_memory_usage_mb();
                if (current_memory > peak_memory_) {
                    peak_memory_ = current_memory;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    ResourceUsage stop() {
        monitoring_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }

        auto end_wall_time = std::chrono::high_resolution_clock::now();
        double end_cpu_time = get_cpu_time_ms();
        size_t end_memory = get_memory_usage_mb();

        ResourceUsage usage;

        // Wall time
        std::chrono::duration<double, std::milli> wall_elapsed = end_wall_time - start_wall_time_;
        usage.wall_time_ms = wall_elapsed.count();

        // CPU time
        usage.cpu_time_ms = end_cpu_time - start_cpu_time_;

        // CPU percentage (CPU time / wall time)
        usage.cpu_percent = (usage.cpu_time_ms / usage.wall_time_ms) * 100.0;

        // Memory
        usage.ram_used_mb = end_memory - start_memory_;
        usage.ram_peak_mb = peak_memory_ - start_memory_;

        return usage;
    }

private:
    std::atomic<bool> monitoring_;
    std::thread monitor_thread_;
    size_t start_memory_;
    size_t peak_memory_;
    double start_cpu_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_wall_time_;
};

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

void print_header(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void print_resource_usage(const std::string& name, const ResourceUsage& usage) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << name << ":" << std::endl;
    std::cout << "  Wall time:    " << usage.wall_time_ms << " ms" << std::endl;
    std::cout << "  CPU time:     " << usage.cpu_time_ms << " ms" << std::endl;
    std::cout << "  CPU usage:    " << usage.cpu_percent << "%" << std::endl;
    std::cout << "  RAM used:     " << usage.ram_used_mb << " MB" << std::endl;
    std::cout << "  RAM peak:     " << usage.ram_peak_mb << " MB" << std::endl;
    std::cout << std::endl;
}

// ============================================
// Benchmark 1: Single Image Resize
// ============================================

void bench_single_image_resources() {
    print_header("Single Image Resize - Resource Usage");

    const char* input_dir = "bench_single_input";
    const char* output_dir = "bench_single_output";

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
    create_directory(input_dir);
    create_directory(output_dir);

    struct TestCase {
        const char* name;
        int width, height;
    };

    TestCase cases[] = {
        {"Small (400x300)", 400, 300},
        {"Medium (800x600)", 800, 600},
        {"Large (1920x1080)", 1920, 1080},
        {"Very Large (2000x2000)", 2000, 2000},
        {"Huge (4000x3000)", 4000, 3000},
    };

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;

    for (const auto& test : cases) {
        std::string input_path = std::string(input_dir) + "/test.jpg";
        std::string output_path = std::string(output_dir) + "/test.jpg";

        // Create test image
        create_test_image(input_path.c_str(), test.width, test.height);

        // Measure resources
        ResourceMonitor monitor;
        monitor.start();

        bool success = fastresize::resize(input_path, output_path, opts);

        ResourceUsage usage = monitor.stop();

        if (success) {
            print_resource_usage(test.name, usage);
        } else {
            std::cout << test.name << ": FAILED" << std::endl;
        }

        // Cleanup for next test
        remove(input_path.c_str());
        remove(output_path.c_str());
    }

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 2: Batch Resize (Sequential)
// ============================================

void bench_batch_sequential_resources() {
    print_header("Batch Resize (Sequential - 1 Thread) - Resource Usage");

    const char* input_dir = "bench_batch_seq_input";
    const char* output_dir = "bench_batch_seq_output";

    struct TestCase {
        const char* name;
        int num_images;
        int width, height;
    };

    TestCase cases[] = {
        {"10 images (800x600)", 10, 800, 600},
        {"50 images (800x600)", 50, 800, 600},
        {"100 images (800x600)", 100, 800, 600},
        {"100 images (2000x2000)", 100, 2000, 2000},
    };

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 300;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 1; // Sequential

    for (const auto& test : cases) {
        cleanup_directory(input_dir);
        cleanup_directory(output_dir);
        create_directory(input_dir);
        create_directory(output_dir);

        // Create test images
        std::vector<std::string> input_paths;
        for (int i = 0; i < test.num_images; i++) {
            std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
            create_test_image(path.c_str(), test.width, test.height);
            input_paths.push_back(path);
        }

        // Measure resources
        ResourceMonitor monitor;
        monitor.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        ResourceUsage usage = monitor.stop();

        if (result.success == test.num_images) {
            print_resource_usage(test.name, usage);
        } else {
            std::cout << test.name << ": FAILED" << std::endl;
        }
    }

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 3: Batch Resize (Parallel)
// ============================================

void bench_batch_parallel_resources() {
    print_header("Batch Resize (Parallel - 8 Threads) - Resource Usage");

    const char* input_dir = "bench_batch_par_input";
    const char* output_dir = "bench_batch_par_output";

    struct TestCase {
        const char* name;
        int num_images;
        int width, height;
        int threads;
    };

    TestCase cases[] = {
        {"10 images, 4 threads (800x600)", 10, 800, 600, 4},
        {"10 images, 8 threads (800x600)", 10, 800, 600, 8},
        {"50 images, 4 threads (800x600)", 50, 800, 600, 4},
        {"50 images, 8 threads (800x600)", 50, 800, 600, 8},
        {"100 images, 4 threads (800x600)", 100, 800, 600, 4},
        {"100 images, 8 threads (800x600)", 100, 800, 600, 8},
        {"100 images, 4 threads (2000x2000)", 100, 2000, 2000, 4},
        {"100 images, 8 threads (2000x2000)", 100, 2000, 2000, 8},
        {"300 images, 8 threads (2000x2000)", 300, 2000, 2000, 8},
    };

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 400;
    opts.target_height = 300;

    for (const auto& test : cases) {
        cleanup_directory(input_dir);
        cleanup_directory(output_dir);
        create_directory(input_dir);
        create_directory(output_dir);

        // Create test images
        std::vector<std::string> input_paths;
        for (int i = 0; i < test.num_images; i++) {
            std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
            create_test_image(path.c_str(), test.width, test.height);
            input_paths.push_back(path);
        }

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = test.threads;

        // Measure resources
        ResourceMonitor monitor;
        monitor.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        ResourceUsage usage = monitor.stop();

        if (result.success == test.num_images) {
            print_resource_usage(test.name, usage);
        } else {
            std::cout << test.name << ": FAILED" << std::endl;
        }
    }

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Benchmark 4: Thread Count Comparison
// ============================================

void bench_thread_scaling_resources() {
    print_header("Thread Scaling Impact on Resources (100 images, 2000x2000)");

    const char* input_dir = "bench_threads_input";
    const char* output_dir = "bench_threads_output";

    cleanup_directory(input_dir);
    create_directory(input_dir);

    const int num_images = 100;
    const int width = 2000;
    const int height = 2000;

    // Create test images once
    std::vector<std::string> input_paths;
    for (int i = 0; i < num_images; i++) {
        std::string path = std::string(input_dir) + "/img" + std::to_string(i) + ".jpg";
        create_test_image(path.c_str(), width, height);
        input_paths.push_back(path);
    }

    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;

    int thread_counts[] = {1, 2, 4, 8};

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Threads | Wall Time | CPU Time | CPU%   | RAM Used | RAM Peak" << std::endl;
    std::cout << "--------|-----------|----------|--------|----------|----------" << std::endl;

    for (int threads : thread_counts) {
        cleanup_directory(output_dir);
        create_directory(output_dir);

        fastresize::BatchOptions batch_opts;
        batch_opts.num_threads = threads;

        // Measure resources
        ResourceMonitor monitor;
        monitor.start();

        fastresize::BatchResult result = fastresize::batch_resize(
            input_paths, output_dir, opts, batch_opts);

        ResourceUsage usage = monitor.stop();

        std::cout << std::setw(7) << threads << " | ";
        std::cout << std::setw(7) << usage.wall_time_ms << "ms | ";
        std::cout << std::setw(6) << usage.cpu_time_ms << "ms | ";
        std::cout << std::setw(5) << usage.cpu_percent << "% | ";
        std::cout << std::setw(6) << usage.ram_used_mb << "MB | ";
        std::cout << std::setw(6) << usage.ram_peak_mb << "MB" << std::endl;
    }

    std::cout << std::endl;

    cleanup_directory(input_dir);
    cleanup_directory(output_dir);
}

// ============================================
// Main
// ============================================

int main() {
    std::cout << "FastResize - Resource Usage Benchmarks (CPU & RAM)" << std::endl;
    std::cout << "===================================================" << std::endl;
    std::cout << "\nSystem Information:" << std::endl;
    std::cout << "  Platform: macOS" << std::endl;
    std::cout << "  Monitoring: CPU time, Wall time, RAM (resident)" << std::endl;
    std::cout << "  Sample rate: 10ms for peak RAM detection" << std::endl;

    bench_single_image_resources();
    bench_batch_sequential_resources();
    bench_batch_parallel_resources();
    bench_thread_scaling_resources();

    std::cout << "\n===================================================" << std::endl;
    std::cout << "All resource usage benchmarks completed!" << std::endl;
    std::cout << "===================================================" << std::endl;

    return 0;
}
