/*
 * FastResize - The Fastest Image Resizing Library On The Planet
 * Copyright (C) 2025 Tran Huu Canh (0xTh3OKrypt) and FastResize Contributors
 *
 * Resize 1,000 images in 2 seconds. Up to 2.9x faster than libvips,
 * 3.1x faster than imageflow. Uses 3-4x less RAM than alternatives.
 *
 * Author: Tran Huu Canh (0xTh3OKrypt)
 * Email: tranhuucanh39@gmail.com
 * Homepage: https://github.com/tranhuucanh/fast_resize
 *
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "internal.h"
#include "pipeline.h"
#include <cstring>
#include <mutex>

namespace fastresize {

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

// Validate resize options
static bool validate_options(const ResizeOptions& opts) {
    switch (opts.mode) {
        case ResizeOptions::SCALE_PERCENT:
            if (opts.scale_percent <= 0) {
                internal::set_last_error(RESIZE_ERROR, "Scale must be positive");
                return false;
            }
            break;
        case ResizeOptions::FIT_WIDTH:
            if (opts.target_width <= 0) {
                internal::set_last_error(RESIZE_ERROR, "Width must be positive");
                return false;
            }
            break;
        case ResizeOptions::FIT_HEIGHT:
            if (opts.target_height <= 0) {
                internal::set_last_error(RESIZE_ERROR, "Height must be positive");
                return false;
            }
            break;
        case ResizeOptions::EXACT_SIZE:
            if (opts.target_width <= 0 || opts.target_height <= 0) {
                internal::set_last_error(RESIZE_ERROR, "Width and height must be positive");
                return false;
            }
            break;
    }

    if (opts.quality < 1 || opts.quality > 100) {
        internal::set_last_error(RESIZE_ERROR, "Quality must be between 1 and 100");
        return false;
    }

    return true;
}

ImageInfo get_image_info(const std::string& path) {
    ImageInfo info;
    info.width = 0;
    info.height = 0;
    info.channels = 0;

    internal::ImageFormat format = internal::detect_format(path);
    if (format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown image format");
        return info;
    }

    info.format = internal::format_to_string(format);

    if (!internal::get_image_dimensions(path, info.width, info.height, info.channels)) {
        internal::set_last_error(DECODE_ERROR, "Failed to read image dimensions");
        info.width = 0;
        info.height = 0;
        info.channels = 0;
    }

    return info;
}

bool resize(
    const std::string& input_path,
    const std::string& output_path,
    const ResizeOptions& options
) {
    if (!validate_options(options)) {
        return false;
    }

    internal::ImageFormat input_format = internal::detect_format(input_path);
    if (input_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown input image format");
        return false;
    }

    internal::ImageFormat output_format = internal::detect_format(output_path);
    if (output_format == internal::FORMAT_UNKNOWN) {
        size_t dot_pos = output_path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            std::string ext = output_path.substr(dot_pos + 1);
            for (char& c : ext) {
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            }
            output_format = internal::string_to_format(ext);
        }

        if (output_format == internal::FORMAT_UNKNOWN) {
            output_format = input_format;
        }
    }

    int input_w, input_h, input_channels;
    if (!internal::get_image_dimensions(input_path, input_w, input_h, input_channels)) {
        internal::set_last_error(DECODE_ERROR, "Failed to read image dimensions");
        return false;
    }

    int output_w, output_h;
    internal::calculate_dimensions(
        input_w, input_h,
        options,
        output_w, output_h
    );

    internal::ImageData input_data = internal::decode_image(input_path, input_format, output_w, output_h);
    if (!input_data.pixels) {
        internal::set_last_error(DECODE_ERROR, "Failed to decode input image");
        return false;
    }

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

    internal::ImageData output_data;
    output_data.pixels = output_pixels;
    output_data.width = output_w;
    output_data.height = output_h;
    output_data.channels = input_data.channels;

    bool encode_ok = internal::encode_image(output_path, output_data, output_format, options.quality);

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
    if (!validate_options(options)) {
        return false;
    }

    internal::ImageFormat input_format = internal::detect_format(input_path);
    if (input_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown input image format");
        return false;
    }

    internal::ImageFormat output_format = internal::string_to_format(output_format_str);
    if (output_format == internal::FORMAT_UNKNOWN) {
        internal::set_last_error(UNSUPPORTED_FORMAT, "Unknown output format: " + output_format_str);
        return false;
    }

    int input_w, input_h, input_channels;
    if (!internal::get_image_dimensions(input_path, input_w, input_h, input_channels)) {
        internal::set_last_error(DECODE_ERROR, "Failed to read image dimensions");
        return false;
    }

    int output_w, output_h;
    internal::calculate_dimensions(
        input_w, input_h,
        options,
        output_w, output_h
    );

    internal::ImageData input_data = internal::decode_image(input_path, input_format, output_w, output_h);
    if (!input_data.pixels) {
        internal::set_last_error(DECODE_ERROR, "Failed to decode input image");
        return false;
    }

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

    internal::ImageData output_data;
    output_data.pixels = output_pixels;
    output_data.width = output_w;
    output_data.height = output_h;
    output_data.channels = input_data.channels;

    bool encode_ok = internal::encode_image(output_path, output_data, output_format, options.quality);

    internal::free_image_data(input_data);
    delete[] output_pixels;

    if (!encode_ok) {
        return false;
    }

    internal::set_last_error(OK, "");
    return true;
}

namespace {
    size_t calculate_optimal_threads(size_t batch_size, int requested_threads) {
        if (requested_threads > 0) {
            return static_cast<size_t>(requested_threads);
        }

        if (batch_size < 5) {
            return 1;
        } else if (batch_size < 20) {
            return 2;
        } else if (batch_size < 50) {
            return 4;
        } else {
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

    if (batch_opts.max_speed && input_paths.size() >= 20) {
        std::vector<BatchItem> items;
        items.reserve(input_paths.size());

        for (const std::string& input_path : input_paths) {
            BatchItem item;
            item.input_path = input_path;

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

    size_t num_threads = calculate_optimal_threads(input_paths.size(), batch_opts.num_threads);

    internal::ThreadPool* pool = internal::create_thread_pool(num_threads);
    internal::BufferPool* buffer_pool = internal::create_buffer_pool();

    std::mutex result_mutex;
    std::atomic<bool> should_stop(false);

    for (const std::string& input_path : input_paths) {
        if (should_stop.load()) {
            break;
        }

        internal::thread_pool_enqueue(pool, [&, input_path]() {
            if (should_stop.load()) {
                return;
            }

            size_t last_slash = input_path.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos)
                ? input_path.substr(last_slash + 1)
                : input_path;

            std::string output_path = output_dir + "/" + filename;

            bool success = resize(input_path, output_path, options);

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

    internal::thread_pool_wait(pool);

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

    if (batch_opts.max_speed && items.size() >= 20) {
        int total_width = 0;
        int total_height = 0;
        int valid_count = 0;

        for (const auto& item : items) {
            if (item.options.target_width > 0 && item.options.target_height > 0) {
                total_width += item.options.target_width;
                total_height += item.options.target_height;
                valid_count++;
            }
        }

        int avg_width = valid_count > 0 ? total_width / valid_count : 2000;
        int avg_height = valid_count > 0 ? total_height / valid_count : 2000;

        size_t queue_capacity = internal::calculate_queue_capacity(avg_width, avg_height);

        internal::PipelineProcessor pipeline(4, 8, 4, queue_capacity);
        return pipeline.process_batch(items);
    }

    size_t num_threads = calculate_optimal_threads(items.size(), batch_opts.num_threads);

    internal::ThreadPool* pool = internal::create_thread_pool(num_threads);
    internal::BufferPool* buffer_pool = internal::create_buffer_pool();

    std::mutex result_mutex;
    std::atomic<bool> should_stop(false);

    for (const BatchItem& item : items) {
        if (should_stop.load()) {
            break;
        }

        internal::thread_pool_enqueue(pool, [&, item]() {
            if (should_stop.load()) {
                return;
            }

            bool success = resize(item.input_path, item.output_path, item.options);

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

    internal::thread_pool_wait(pool);

    internal::destroy_buffer_pool(buffer_pool);
    internal::destroy_thread_pool(pool);

    return result;
}

}
