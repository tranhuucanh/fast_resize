#include "fastresize.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <sys/resource.h>
#include <cstdlib>

using namespace std;
using namespace fastresize;

// Get peak memory usage in MB
double get_peak_memory_mb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0 / 1024.0;  // Convert to MB
}

void benchmark_format(const string& format, const string& test_dir, int num_images) {
    cout << "\n========================================\n";
    cout << "Format: " << format << " (" << num_images << " images)\n";
    cout << "========================================\n";

    // Prepare file paths
    vector<string> input_paths;
    for (int i = 1; i <= num_images; i++) {
        input_paths.push_back(test_dir + "/img_" + to_string(i) + "." + format);
    }

    string output_dir = "/tmp/fastresize_output_" + format;
    system(("mkdir -p " + output_dir).c_str());

    // Configure resize options
    ResizeOptions opts;
    opts.mode = ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;
    opts.quality = 85;

    // Benchmark with different thread counts
    vector<int> thread_counts = {1, 2, 4, 8};

    for (int threads : thread_counts) {
        BatchOptions batch_opts;
        batch_opts.num_threads = threads;

        // Reset peak memory
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

        auto start = chrono::high_resolution_clock::now();

        BatchResult result = batch_resize(input_paths, output_dir, opts, batch_opts);

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

        double peak_memory = get_peak_memory_mb();

        cout << threads << " thread" << (threads > 1 ? "s" : " ") << ": "
             << setw(6) << duration.count() << " ms"
             << " (" << setw(4) << fixed << setprecision(2)
             << (double)duration.count() / num_images << " ms/image)"
             << " | Success: " << result.success
             << "/" << result.total
             << " | RAM: " << setw(5) << fixed << setprecision(1) << peak_memory << " MB"
             << endl;
    }

    // Cleanup
    system(("rm -rf " + output_dir).c_str());
}

int main() {
    cout << "\n";
    cout << "FastResize - Format-Specific Benchmarks\n";
    cout << "=========================================\n";
    cout << "Testing: JPG, PNG, WEBP, BMP\n";
    cout << "Image size: 2000x2000 -> 800x600\n";
    cout << "\n";

    string base_dir = "/tmp/fastresize_format_test";

    // Test each format with 100 images
    benchmark_format("jpg", base_dir + "/jpg_test", 100);
    benchmark_format("png", base_dir + "/png_test", 100);
    benchmark_format("webp", base_dir + "/webp_test", 100);
    benchmark_format("bmp", base_dir + "/bmp_test", 100);

    cout << "\n";
    cout << "=========================================\n";
    cout << "All format benchmarks completed!\n";
    cout << "=========================================\n";

    return 0;
}
