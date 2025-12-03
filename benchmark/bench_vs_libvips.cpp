#include <fastresize.h>
#include <vips/vips8>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

struct BenchResult {
    std::string library;
    double time_sec;
    int images_processed;
    int images_failed;
    double throughput;  // images per second
    double per_image_ms;
};

// ============================================
// FastResize Benchmark
// ============================================
BenchResult bench_fastresize(const std::vector<std::string>& input_files,
                              const std::string& output_dir,
                              int target_width) {
    BenchResult result;
    result.library = "FastResize";

    // Configure options
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = target_width;
    opts.keep_aspect_ratio = true;
    opts.quality = 85;

    fastresize::BatchOptions batch_opts;
    batch_opts.num_threads = 0;  // Auto-detect
    batch_opts.max_speed = false;

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Run batch resize
    fastresize::BatchResult batch_result = fastresize::batch_resize(
        input_files,
        output_dir,
        opts,
        batch_opts
    );

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    result.time_sec = elapsed.count();
    result.images_processed = batch_result.success;
    result.images_failed = batch_result.failed;
    result.throughput = result.images_processed / result.time_sec;
    result.per_image_ms = (result.time_sec / result.images_processed) * 1000.0;

    return result;
}

// ============================================
// libvips Benchmark (Sequential - for comparison)
// ============================================
BenchResult bench_libvips_sequential(const std::vector<std::string>& input_files,
                                     const std::string& output_dir,
                                     int target_width) {
    BenchResult result;
    result.library = "libvips (sequential)";
    result.images_processed = 0;
    result.images_failed = 0;

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Process each image with libvips
    for (const auto& input_path : input_files) {
        try {
            // Extract filename
            size_t last_slash = input_path.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos)
                ? input_path.substr(last_slash + 1)
                : input_path;
            std::string output_path = output_dir + "/" + filename;

            // Load image
            vips::VImage img = vips::VImage::new_from_file(input_path.c_str());

            // Calculate new height maintaining aspect ratio
            double scale = static_cast<double>(target_width) / img.width();

            // Resize image (libvips automatically uses threading internally)
            vips::VImage resized = img.resize(scale);

            // Save with JPEG quality
            resized.write_to_file(output_path.c_str(),
                vips::VImage::option()
                    ->set("Q", 85)  // JPEG quality
                    ->set("strip", true));

            result.images_processed++;
        } catch (vips::VError& e) {
            result.images_failed++;
            // Continue processing other images
        }
    }

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    result.time_sec = elapsed.count();
    result.throughput = result.images_processed / result.time_sec;
    result.per_image_ms = (result.time_sec / result.images_processed) * 1000.0;

    return result;
}

// ============================================
// libvips Benchmark (Parallel - with std::thread)
// ============================================
BenchResult bench_libvips_parallel(const std::vector<std::string>& input_files,
                                   const std::string& output_dir,
                                   int target_width,
                                   int num_threads) {
    BenchResult result;
    result.library = "libvips (parallel)";
    result.images_processed = 0;
    result.images_failed = 0;

    std::mutex result_mutex;

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Create worker threads
    std::vector<std::thread> threads;
    size_t images_per_thread = input_files.size() / num_threads;
    size_t remainder = input_files.size() % num_threads;

    for (int t = 0; t < num_threads; ++t) {
        size_t start_idx = t * images_per_thread;
        size_t end_idx = (t + 1) * images_per_thread;
        if (t == num_threads - 1) {
            end_idx += remainder;
        }

        threads.emplace_back([&, start_idx, end_idx]() {
            int local_success = 0;
            int local_failed = 0;

            for (size_t i = start_idx; i < end_idx; ++i) {
                try {
                    const std::string& input_path = input_files[i];

                    // Extract filename
                    size_t last_slash = input_path.find_last_of("/\\");
                    std::string filename = (last_slash != std::string::npos)
                        ? input_path.substr(last_slash + 1)
                        : input_path;
                    std::string output_path = output_dir + "/" + filename;

                    // Load image
                    vips::VImage img = vips::VImage::new_from_file(input_path.c_str());

                    // Calculate new height maintaining aspect ratio
                    double scale = static_cast<double>(target_width) / img.width();

                    // Resize image
                    vips::VImage resized = img.resize(scale);

                    // Save with JPEG quality
                    resized.write_to_file(output_path.c_str(),
                        vips::VImage::option()
                            ->set("Q", 85)
                            ->set("strip", true));

                    local_success++;
                } catch (vips::VError& e) {
                    local_failed++;
                }
            }

            // Update global counters
            std::lock_guard<std::mutex> lock(result_mutex);
            result.images_processed += local_success;
            result.images_failed += local_failed;
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    result.time_sec = elapsed.count();
    result.throughput = result.images_processed / result.time_sec;
    result.per_image_ms = (result.time_sec / result.images_processed) * 1000.0;

    return result;
}

// ============================================
// Main Benchmark
// ============================================
int main(int argc, char** argv) {
    // Initialize libvips
    if (VIPS_INIT(argv[0])) {
        vips_error_exit(nullptr);
    }

    std::string input_dir = "/Users/canh.th/Desktop/fastgems/fastresize/images/input";
    std::string output_base = "/Users/canh.th/Desktop/fastgems/fastresize/images/benchmark_vs_libvips";
    int target_width = 800;

    // Allow command line override
    if (argc > 1) input_dir = argv[1];
    if (argc > 2) target_width = std::atoi(argv[2]);

    std::cout << "============================================================\n";
    std::cout << "FastResize vs libvips - C++ Benchmark\n";
    std::cout << "============================================================\n\n";

    // Get input files using POSIX API
    std::vector<std::string> input_files;
    DIR* dir = opendir(input_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                std::string filename = entry->d_name;
                std::string ext;
                size_t dot_pos = filename.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    ext = filename.substr(dot_pos);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                }
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                    input_files.push_back(input_dir + "/" + filename);
                }
            }
        }
        closedir(dir);
    }

    std::sort(input_files.begin(), input_files.end());

    if (input_files.empty()) {
        std::cerr << "ERROR: No image files found in " << input_dir << "\n";
        return 1;
    }

    std::cout << "Input directory: " << input_dir << "\n";
    std::cout << "Total images:    " << input_files.size() << "\n";
    std::cout << "Target width:    " << target_width << " (auto height)\n";
    std::cout << "Output format:   JPEG (quality=85)\n";
    std::cout << "\n";

    // Get sample image info
    if (!input_files.empty()) {
        fastresize::ImageInfo info = fastresize::get_image_info(input_files[0]);
        size_t last_slash = input_files[0].find_last_of("/\\");
        std::string sample_name = (last_slash != std::string::npos)
            ? input_files[0].substr(last_slash + 1)
            : input_files[0];
        std::cout << "Sample image:    " << sample_name << "\n";
        std::cout << "Original size:   " << info.width << "x" << info.height
                  << " (" << info.channels << " channels)\n";
        std::cout << "\n";
    }

    std::vector<BenchResult> results;

    // ============================================
    // Benchmark 1: FastResize
    // ============================================
    std::cout << "============================================================\n";
    std::cout << "TEST 1: FastResize (C++ native, multi-threaded)\n";
    std::cout << "============================================================\n\n";

    std::string output_fr = output_base + "/fastresize";
    mkdir(output_base.c_str(), 0755);
    mkdir(output_fr.c_str(), 0755);

    std::cout << "Processing " << input_files.size() << " images...\n";
    BenchResult result_fr = bench_fastresize(input_files, output_fr, target_width);
    results.push_back(result_fr);

    std::cout << "✓ Complete!\n\n";
    std::cout << "Results:\n";
    std::cout << "  ⏱️  Time:       " << result_fr.time_sec << " seconds\n";
    std::cout << "  ✅ Success:     " << result_fr.images_processed << " images\n";
    std::cout << "  ❌ Failed:      " << result_fr.images_failed << " images\n";
    std::cout << "  🚀 Throughput:  " << result_fr.throughput << " img/s\n";
    std::cout << "  📊 Per image:   " << result_fr.per_image_ms << " ms\n";
    std::cout << "\n";

    // Wait a bit between benchmarks
    std::cout << "Waiting 2 seconds before next test...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ============================================
    // Benchmark 2: libvips (parallel with 8 threads)
    // ============================================
    std::cout << "============================================================\n";
    std::cout << "TEST 2: libvips (parallel, 8 threads)\n";
    std::cout << "============================================================\n\n";

    std::string output_vips_parallel = output_base + "/libvips_parallel";
    mkdir(output_vips_parallel.c_str(), 0755);

    std::cout << "Processing " << input_files.size() << " images with 8 threads...\n";
    BenchResult result_vips_parallel = bench_libvips_parallel(input_files, output_vips_parallel, target_width, 8);
    results.push_back(result_vips_parallel);

    std::cout << "✓ Complete!\n\n";
    std::cout << "Results:\n";
    std::cout << "  ⏱️  Time:       " << result_vips_parallel.time_sec << " seconds\n";
    std::cout << "  ✅ Success:     " << result_vips_parallel.images_processed << " images\n";
    std::cout << "  ❌ Failed:      " << result_vips_parallel.images_failed << " images\n";
    std::cout << "  🚀 Throughput:  " << result_vips_parallel.throughput << " img/s\n";
    std::cout << "  📊 Per image:   " << result_vips_parallel.per_image_ms << " ms\n";
    std::cout << "\n";

    // Wait
    std::cout << "Waiting 2 seconds before next test...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ============================================
    // Benchmark 3: libvips (sequential - for reference)
    // ============================================
    std::cout << "============================================================\n";
    std::cout << "TEST 3: libvips (sequential - reference)\n";
    std::cout << "============================================================\n\n";

    std::string output_vips_seq = output_base + "/libvips_sequential";
    mkdir(output_vips_seq.c_str(), 0755);

    std::cout << "Processing " << input_files.size() << " images sequentially...\n";
    BenchResult result_vips_seq = bench_libvips_sequential(input_files, output_vips_seq, target_width);
    results.push_back(result_vips_seq);

    std::cout << "✓ Complete!\n\n";
    std::cout << "Results:\n";
    std::cout << "  ⏱️  Time:       " << result_vips_seq.time_sec << " seconds\n";
    std::cout << "  ✅ Success:     " << result_vips_seq.images_processed << " images\n";
    std::cout << "  ❌ Failed:      " << result_vips_seq.images_failed << " images\n";
    std::cout << "  🚀 Throughput:  " << result_vips_seq.throughput << " img/s\n";
    std::cout << "  📊 Per image:   " << result_vips_seq.per_image_ms << " ms\n";
    std::cout << "\n";

    // ============================================
    // Comparison
    // ============================================
    std::cout << "============================================================\n";
    std::cout << "COMPARISON\n";
    std::cout << "============================================================\n\n";

    std::cout << "Library       Time(s)  Throughput   Per Image   Success\n";
    std::cout << "------------------------------------------------------------\n";

    for (const auto& r : results) {
        printf("%-12s  %6.2f   %8.1f/s   %7.2fms   %4d/%d\n",
               r.library.c_str(),
               r.time_sec,
               r.throughput,
               r.per_image_ms,
               r.images_processed,
               r.images_processed + r.images_failed);
    }

    std::cout << "\n";

    // Calculate winner
    auto fastest = std::min_element(results.begin(), results.end(),
        [](const BenchResult& a, const BenchResult& b) {
            return a.time_sec < b.time_sec;
        });

    std::cout << "🏆 Winner: " << fastest->library << "\n";
    std::cout << "   Time: " << fastest->time_sec << " seconds\n";
    std::cout << "   Speed: " << fastest->throughput << " img/s\n\n";

    // Calculate speedup/slowdown
    for (const auto& r : results) {
        if (r.library != fastest->library) {
            double speedup = r.time_sec / fastest->time_sec;
            double percent = (speedup - 1.0) * 100.0;
            if (speedup > 1.0) {
                printf("   %s is %.1f%% slower (%.2fx)\n",
                       r.library.c_str(), percent, speedup);
            } else {
                printf("   %s is %.1f%% faster (%.2fx)\n",
                       r.library.c_str(), -percent, 1.0/speedup);
            }
        }
    }

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Notes:\n";
    std::cout << "- FastResize:        Multi-threaded (auto-detect, 8 cores)\n";
    std::cout << "- libvips parallel:  8 std::threads, each processing images\n";
    std::cout << "- libvips also uses internal threading per operation\n";
    std::cout << "- Both using libjpeg-turbo for JPEG encoding\n";
    std::cout << "- Quality: 85 for both libraries\n";
    std::cout << "============================================================\n";

    vips_shutdown();
    return 0;
}
