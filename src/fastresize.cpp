#include "internal.h"
#include "pipeline.h"
#include <cstring>
#include <mutex>

namespace fastresize {

// ============================================
// Error Handling (Thread-safe)
// ============================================

namespace {
    std::mutex error_mutex;
    ErrorCode last_error_code = OK;
    std::string last_error_message;
}

namespace internal {
    void set_last_error(ErrorCode code, const std::string& message) {
        std::lock_guard<std::mutex> lock(error_mutex);
        last_error_code = code;
        last_error_message = message;
    }
}

std::string get_last_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error_message;
}

ErrorCode get_last_error_code() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error_code;
}

// ============================================
// Image Info
// ============================================

ImageInfo get_image_info(const std::string& path) {
    ImageInfo info;
    info.width = 0;
    info.height = 0;
    info.channels = 0;

    // Detect format
    internal::ImageFormat format = internal::detect_format(path);
    if (format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown image format");
        return info;
    }

    info.format = internal::format_to_string(format);

    // Get dimensions
    if (!internal::get_image_dimensions(path, info.width, info.height, info.channels)) {
        internal::set_last_error(DECODE_ERROR, "Failed to read image dimensions");
        info.width = 0;
        info.height = 0;
        info.channels = 0;
    }

    return info;
}

// ============================================
// Single Image Resize
// ============================================

bool resize(
    const std::string& input_path,
    const std::string& output_path,
    const ResizeOptions& options
) {
    // Detect input format
    internal::ImageFormat input_format = internal::detect_format(input_path);
    if (input_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown input image format");
        return false;
    }

    // Detect output format from extension or use input format
    internal::ImageFormat output_format = internal::detect_format(output_path);
    if (output_format == internal::FORMAT_UNKNOWN) {
        // Try to determine from extension
        size_t dot_pos = output_path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            std::string ext = output_path.substr(dot_pos + 1);
            // Convert to lowercase
            for (char& c : ext) {
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            }
            output_format = internal::string_to_format(ext);
        }

        // If still unknown, use input format
        if (output_format == internal::FORMAT_UNKNOWN) {
            output_format = input_format;
        }
    }

    // Load image
    internal::ImageData input_data = internal::decode_image(input_path, input_format);
    if (!input_data.pixels) {
        internal::set_last_error(DECODE_ERROR, "Failed to decode input image");
        return false;
    }

    // Calculate output dimensions
    int output_w, output_h;
    internal::calculate_dimensions(
        input_data.width, input_data.height,
        options,
        output_w, output_h
    );

    // Resize image
    unsigned char* output_pixels = nullptr;
    bool resize_ok = internal::resize_image(
        input_data.pixels,
        input_data.width, input_data.height, input_data.channels,
        &output_pixels,
        output_w, output_h,
        options
    );

    if (!resize_ok || !output_pixels) {
        internal::free_image_data(input_data);
        if (output_pixels) delete[] output_pixels;
        return false;
    }

    // Create output image data
    internal::ImageData output_data;
    output_data.pixels = output_pixels;
    output_data.width = output_w;
    output_data.height = output_h;
    output_data.channels = input_data.channels;

    // Encode and save
    bool encode_ok = internal::encode_image(output_path, output_data, output_format, options.quality);

    // Cleanup
    internal::free_image_data(input_data);
    delete[] output_pixels;

    if (!encode_ok) {
        return false;
    }

    internal::set_last_error(OK, "");
    return true;
}

bool resize_with_format(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& output_format_str,
    const ResizeOptions& options
) {
    // Detect input format
    internal::ImageFormat input_format = internal::detect_format(input_path);
    if (input_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown input image format");
        return false;
    }

    // Parse output format
    internal::ImageFormat output_format = internal::string_to_format(output_format_str);
    if (output_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown output format: " + output_format_str);
        return false;
    }

    // Load image
    internal::ImageData input_data = internal::decode_image(input_path, input_format);
    if (!input_data.pixels) {
        internal::set_last_error(DECODE_ERROR, "Failed to decode input image");
        return false;
    }

    // Calculate output dimensions
    int output_w, output_h;
    internal::calculate_dimensions(
        input_data.width, input_data.height,
        options,
        output_w, output_h
    );

    // Resize image
    unsigned char* output_pixels = nullptr;
    bool resize_ok = internal::resize_image(
        input_data.pixels,
        input_data.width, input_data.height, input_data.channels,
        &output_pixels,
        output_w, output_h,
        options
    );

    if (!resize_ok || !output_pixels) {
        internal::free_image_data(input_data);
        if (output_pixels) delete[] output_pixels;
        return false;
    }

    // Create output image data
    internal::ImageData output_data;
    output_data.pixels = output_pixels;
    output_data.width = output_w;
    output_data.height = output_h;
    output_data.channels = input_data.channels;

    // Encode and save
    bool encode_ok = internal::encode_image(output_path, output_data, output_format, options.quality);

    // Cleanup
    internal::free_image_data(input_data);
    delete[] output_pixels;

    if (!encode_ok) {
        return false;
    }

    internal::set_last_error(OK, "");
    return true;
}

// ============================================
// Batch Processing (Phase 4 - Parallel)
// ============================================

namespace {
    // Phase A Optimization #7: Calculate optimal thread count based on batch size
    size_t calculate_optimal_threads(size_t batch_size, int requested_threads) {
        // If user specified thread count, use it
        if (requested_threads > 0) {
            return static_cast<size_t>(requested_threads);
        }

        // Auto-detect based on batch size
        if (batch_size < 5) {
            return 1;  // Sequential for very small batches
        } else if (batch_size < 20) {
            return 2;  // 2 threads for small batches
        } else if (batch_size < 50) {
            return 4;  // 4 threads for medium batches
        } else {
            // For large batches, use 8 threads (optimal for most systems)
            return 8;
        }
    }
}

BatchResult batch_resize(
    const std::vector<std::string>& input_paths,
    const std::string& output_dir,
    const ResizeOptions& options,
    const BatchOptions& batch_opts
) {
    BatchResult result;
    result.total = static_cast<int>(input_paths.size());
    result.success = 0;
    result.failed = 0;

    if (input_paths.empty()) {
        return result;
    }

    // Phase C: Use 3-stage pipeline if max_speed is enabled and batch is large enough
    if (batch_opts.max_speed && input_paths.size() >= 50) {
        // Convert to BatchItems and use pipeline
        std::vector<BatchItem> items;
        items.reserve(input_paths.size());

        for (const std::string& input_path : input_paths) {
            BatchItem item;
            item.input_path = input_path;

            // Extract filename from input path
            size_t last_slash = input_path.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos)
                ? input_path.substr(last_slash + 1)
                : input_path;

            item.output_path = output_dir + "/" + filename;
            item.options = options;
            items.push_back(item);
        }

        return batch_resize_custom(items, batch_opts);
    }

    // Calculate optimal thread count (Phase A Optimization #7)
    size_t num_threads = calculate_optimal_threads(input_paths.size(), batch_opts.num_threads);

    // Create thread pool and buffer pool
    internal::ThreadPool* pool = internal::create_thread_pool(num_threads);
    internal::BufferPool* buffer_pool = internal::create_buffer_pool();

    // Thread-safe result tracking
    std::mutex result_mutex;
    std::atomic<bool> should_stop(false);

    // Process each image
    for (const std::string& input_path : input_paths) {
        // Check if we should stop
        if (should_stop.load()) {
            break;
        }

        // Enqueue task
        internal::thread_pool_enqueue(pool, [&, input_path]() {
            // Check stop flag
            if (should_stop.load()) {
                return;
            }

            // Extract filename from input path
            size_t last_slash = input_path.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos)
                ? input_path.substr(last_slash + 1)
                : input_path;

            std::string output_path = output_dir + "/" + filename;

            // Perform resize
            bool success = resize(input_path, output_path, options);

            // Update results (thread-safe)
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                if (success) {
                    result.success++;
                } else {
                    result.failed++;
                    result.errors.push_back(input_path + ": " + get_last_error());
                    if (batch_opts.stop_on_error) {
                        should_stop.store(true);
                    }
                }
            }
        });
    }

    // Wait for all tasks to complete
    internal::thread_pool_wait(pool);

    // Cleanup
    internal::destroy_buffer_pool(buffer_pool);
    internal::destroy_thread_pool(pool);

    return result;
}

BatchResult batch_resize_custom(
    const std::vector<BatchItem>& items,
    const BatchOptions& batch_opts
) {
    BatchResult result;
    result.total = static_cast<int>(items.size());
    result.success = 0;
    result.failed = 0;

    if (items.empty()) {
        return result;
    }

    // Phase C: Use 3-stage pipeline if max_speed is enabled and batch is large enough
    if (batch_opts.max_speed && items.size() >= 50) {
        internal::PipelineProcessor pipeline(4, 8, 4, 32);
        return pipeline.process_batch(items);
    }

    // Calculate optimal thread count (Phase A Optimization #7)
    size_t num_threads = calculate_optimal_threads(items.size(), batch_opts.num_threads);

    // Create thread pool and buffer pool
    internal::ThreadPool* pool = internal::create_thread_pool(num_threads);
    internal::BufferPool* buffer_pool = internal::create_buffer_pool();

    // Thread-safe result tracking
    std::mutex result_mutex;
    std::atomic<bool> should_stop(false);

    // Process each item
    for (const BatchItem& item : items) {
        // Check if we should stop
        if (should_stop.load()) {
            break;
        }

        // Enqueue task
        internal::thread_pool_enqueue(pool, [&, item]() {
            // Check stop flag
            if (should_stop.load()) {
                return;
            }

            // Perform resize with custom options
            bool success = resize(item.input_path, item.output_path, item.options);

            // Update results (thread-safe)
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                if (success) {
                    result.success++;
                } else {
                    result.failed++;
                    result.errors.push_back(item.input_path + ": " + get_last_error());
                    if (batch_opts.stop_on_error) {
                        should_stop.store(true);
                    }
                }
            }
        });
    }

    // Wait for all tasks to complete
    internal::thread_pool_wait(pool);

    // Cleanup
    internal::destroy_buffer_pool(buffer_pool);
    internal::destroy_thread_pool(pool);

    return result;
}

} // namespace fastresize
