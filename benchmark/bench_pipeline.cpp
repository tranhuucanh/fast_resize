// Phase C Benchmark: 3-Stage Pipeline vs Thread Pool
#include <fastresize.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <cstdlib>

// Don't define IMPLEMENTATION - already in library
#include <stb_image_write.h>

using namespace std;
using namespace std::chrono;
using namespace fastresize;

void create_test_images(int count, int width, int height) {
    cout << "Creating " << count << " test images (" << width << "x" << height << ")..." << endl;

    vector<unsigned char> pixels(width * height * 3);

    // Create gradient pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 3;
            pixels[idx + 0] = (x * 255) / width;   // R
            pixels[idx + 1] = (y * 255) / height;  // G
            pixels[idx + 2] = 128;                 // B
        }
    }

    // Write test images
    for (int i = 0; i < count; ++i) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/pipeline_test_%03d.jpg", i);
        stbi_write_jpg(filename, width, height, 3, pixels.data(), 85);
    }

    cout << "✓ Created " << count << " test images" << endl;
}

void cleanup_output_files(int count) {
    for (int i = 0; i < count; ++i) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/pipeline_out_%03d.jpg", i);
        remove(filename);
    }
}

void cleanup_test_images(int count) {
    for (int i = 0; i < count; ++i) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/pipeline_test_%03d.jpg", i);
        remove(filename);
    }
    cleanup_output_files(count);
}

void benchmark_mode(const char* mode_name, int count, bool max_speed) {
    cout << "\n=== Testing: " << mode_name << " ===" << endl;

    // Prepare batch items
    vector<string> input_paths;
    for (int i = 0; i < count; ++i) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/pipeline_test_%03d.jpg", i);
        input_paths.push_back(filename);
    }

    // Resize options
    ResizeOptions opts;
    opts.mode = ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;
    opts.quality = 85;

    // Batch options
    BatchOptions batch_opts;
    batch_opts.max_speed = max_speed;
    batch_opts.num_threads = 0;  // Auto-detect

    // Benchmark
    auto start = high_resolution_clock::now();

    BatchResult result = batch_resize(
        input_paths,
        "/tmp",
        opts,
        batch_opts
    );

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    // Results
    cout << "Total: " << result.total << endl;
    cout << "Success: " << result.success << endl;
    cout << "Failed: " << result.failed << endl;
    cout << "Time: " << duration << "ms" << endl;
    cout << "Speed: " << (count * 1000.0 / duration) << " images/sec" << endl;
    cout << "Per image: " << (duration / (double)count) << "ms" << endl;

    if (result.failed > 0) {
        cout << "Errors:" << endl;
        for (const string& err : result.errors) {
            cout << "  - " << err << endl;
        }
    }
}

int main(int argc, char** argv) {
    int count = 100;
    int width = 2000;
    int height = 2000;

    if (argc > 1) count = atoi(argv[1]);
    if (argc > 2) width = atoi(argv[2]);
    if (argc > 3) height = atoi(argv[3]);

    cout << "=== Phase C Pipeline Benchmark ===" << endl;
    cout << "Images: " << count << endl;
    cout << "Size: " << width << "x" << height << " → 800x600" << endl;
    cout << endl;

    // Create test images
    create_test_images(count, width, height);

    // Benchmark 1: Thread Pool (max_speed=false)
    benchmark_mode("Thread Pool (Normal Mode)", count, false);

    // Cleanup output files only (keep input files for next test)
    cleanup_output_files(count);

    // Benchmark 2: Pipeline (max_speed=true)
    if (count >= 50) {
        benchmark_mode("Pipeline (max_speed=true)", count, true);
    } else {
        cout << "\n⚠ Skipping pipeline test (batch size < 50)" << endl;
    }

    // Cleanup all files
    cleanup_test_images(count);

    cout << "\n✓ Benchmark complete!" << endl;

    return 0;
}
