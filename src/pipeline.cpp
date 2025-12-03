#include "pipeline.h"
#include <thread>

namespace fastresize {
namespace internal {

// ============================================
// PipelineProcessor Implementation
// ============================================

PipelineProcessor::PipelineProcessor(
    size_t decode_threads,
    size_t resize_threads,
    size_t encode_threads,
    size_t queue_capacity
)
    : decode_pool_(nullptr)
    , resize_pool_(nullptr)
    , encode_pool_(nullptr)
    , decode_queue_(queue_capacity)
    , resize_queue_(queue_capacity)
    , success_count_(0)
    , failed_count_(0)
{
    decode_pool_ = create_thread_pool(decode_threads);
    resize_pool_ = create_thread_pool(resize_threads);
    encode_pool_ = create_thread_pool(encode_threads);
}

PipelineProcessor::~PipelineProcessor() {
    if (decode_pool_) destroy_thread_pool(decode_pool_);
    if (resize_pool_) destroy_thread_pool(resize_pool_);
    if (encode_pool_) destroy_thread_pool(encode_pool_);
}

// ============================================
// Stage 1: Decode (I/O bound - 4 threads)
// ============================================

void PipelineProcessor::decode_stage(const std::vector<BatchItem>& items) {
    // Enqueue decode tasks
    for (size_t i = 0; i < items.size(); ++i) {
        thread_pool_enqueue(decode_pool_, [this, &items, i]() {
            const auto& item = items[i];

            DecodeResult result;
            result.task_id = i;
            result.output_path = item.output_path;
            result.options = item.options;
            result.success = false;

            // Detect input format
            ImageFormat fmt = detect_format(item.input_path);
            if (fmt == FORMAT_UNKNOWN) {
                result.error_message = "Unknown format: " + item.input_path;
                decode_queue_.push(std::move(result));
                return;
            }

            // Decode image
            result.image = decode_image(item.input_path, fmt);
            if (result.image.pixels == nullptr) {
                result.error_message = "Decode failed: " + item.input_path;
                decode_queue_.push(std::move(result));
                return;
            }

            result.success = true;
            decode_queue_.push(std::move(result));
        });
    }

    // Wait for all decode tasks to complete
    thread_pool_wait(decode_pool_);

    // Signal decode queue is done
    decode_queue_.set_done();
}

// ============================================
// Stage 2: Resize (CPU bound - 8 threads)
// ============================================

void PipelineProcessor::resize_stage() {
    // Create resize worker threads
    std::vector<std::thread> workers;
    for (size_t i = 0; i < 8; ++i) {  // 8 resize workers
        workers.emplace_back([this]() {
            DecodeResult decode_result;

            while (decode_queue_.pop(decode_result)) {
                ResizeResult resize_result;
                resize_result.task_id = decode_result.task_id;
                resize_result.output_path = decode_result.output_path;
                resize_result.options = decode_result.options;
                resize_result.success = false;

                // If decode failed, pass through
                if (!decode_result.success) {
                    resize_result.error_message = decode_result.error_message;
                    resize_result.pixels = nullptr;
                    resize_result.width = 0;
                    resize_result.height = 0;
                    resize_result.channels = 0;
                    resize_queue_.push(std::move(resize_result));
                    continue;
                }

                // Calculate output dimensions
                int out_w, out_h;
                calculate_dimensions(
                    decode_result.image.width,
                    decode_result.image.height,
                    decode_result.options,
                    out_w, out_h
                );

                // Resize image
                unsigned char* resized_pixels = nullptr;
                bool resize_ok = resize_image(
                    decode_result.image.pixels,
                    decode_result.image.width,
                    decode_result.image.height,
                    decode_result.image.channels,
                    &resized_pixels,
                    out_w, out_h,
                    decode_result.options
                );

                // Free decode buffer
                free_image_data(decode_result.image);

                if (!resize_ok || resized_pixels == nullptr) {
                    resize_result.error_message = "Resize failed";
                    resize_result.pixels = nullptr;
                    resize_result.width = 0;
                    resize_result.height = 0;
                    resize_result.channels = 0;
                    resize_queue_.push(std::move(resize_result));
                    continue;
                }

                resize_result.pixels = resized_pixels;
                resize_result.width = out_w;
                resize_result.height = out_h;
                resize_result.channels = decode_result.image.channels;
                resize_result.success = true;
                resize_queue_.push(std::move(resize_result));
            }
        });
    }

    // Wait for all resize workers
    for (auto& worker : workers) {
        worker.join();
    }

    // Signal resize queue is done
    resize_queue_.set_done();
}

// ============================================
// Stage 3: Encode (I/O bound - 4 threads)
// ============================================

void PipelineProcessor::encode_stage() {
    // Create encode worker threads
    std::vector<std::thread> workers;
    for (size_t i = 0; i < 4; ++i) {  // 4 encode workers
        workers.emplace_back([this]() {
            ResizeResult resize_result;

            while (resize_queue_.pop(resize_result)) {
                // If resize failed, record error
                if (!resize_result.success) {
                    failed_count_.fetch_add(1);
                    if (!resize_result.error_message.empty()) {
                        std::lock_guard<std::mutex> lock(errors_mutex_);
                        errors_.push_back(resize_result.error_message);
                    }
                    continue;
                }

                // Detect output format from file extension (not content, since file doesn't exist yet)
                ImageFormat out_fmt = FORMAT_UNKNOWN;
                size_t dot_pos = resize_result.output_path.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    std::string ext = resize_result.output_path.substr(dot_pos + 1);
                    // Convert to lowercase
                    for (char& c : ext) {
                        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                    }
                    out_fmt = string_to_format(ext);
                }

                // Default to JPEG if unknown
                if (out_fmt == FORMAT_UNKNOWN) {
                    out_fmt = FORMAT_JPEG;
                }

                // Validate data before encoding
                if (resize_result.pixels == nullptr || resize_result.width <= 0 ||
                    resize_result.height <= 0 || resize_result.channels <= 0) {
                    failed_count_.fetch_add(1);
                    std::lock_guard<std::mutex> lock(errors_mutex_);
                    errors_.push_back("Invalid resize data for: " + resize_result.output_path);
                    if (resize_result.pixels) delete[] resize_result.pixels;
                    continue;
                }

                // Encode image
                ImageData img_data;
                img_data.pixels = resize_result.pixels;
                img_data.width = resize_result.width;
                img_data.height = resize_result.height;
                img_data.channels = resize_result.channels;

                bool encode_ok = encode_image(
                    resize_result.output_path,
                    img_data,
                    out_fmt,
                    resize_result.options.quality
                );

                // Free resize buffer
                delete[] resize_result.pixels;

                if (encode_ok) {
                    success_count_.fetch_add(1);
                } else {
                    failed_count_.fetch_add(1);
                    std::lock_guard<std::mutex> lock(errors_mutex_);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Encode failed: %s (fmt=%d, %dx%d, %dch)",
                             resize_result.output_path.c_str(), (int)out_fmt,
                             resize_result.width, resize_result.height, resize_result.channels);
                    errors_.push_back(buf);
                }
            }
        });
    }

    // Wait for all encode workers
    for (auto& worker : workers) {
        worker.join();
    }
}

// ============================================
// Main Pipeline Execution
// ============================================

BatchResult PipelineProcessor::process_batch(const std::vector<BatchItem>& items) {
    // Reset counters
    success_count_ = 0;
    failed_count_ = 0;
    errors_.clear();

    // Start all 3 stages concurrently
    std::thread decode_thread([this, &items]() { decode_stage(items); });
    std::thread resize_thread([this]() { resize_stage(); });
    std::thread encode_thread([this]() { encode_stage(); });

    // Wait for all stages to complete
    decode_thread.join();
    resize_thread.join();
    encode_thread.join();

    // Build result
    BatchResult result;
    result.total = items.size();
    result.success = success_count_.load();
    result.failed = failed_count_.load();
    result.errors = errors_;

    return result;
}

} // namespace internal
} // namespace fastresize
