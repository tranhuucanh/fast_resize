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

#include "pipeline.h"
#include <thread>

namespace fastresize {
namespace internal {

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

    for (size_t i = 0; i < encode_threads; ++i) {
        encode_buffer_pools_.push_back(create_buffer_pool());
    }
}

PipelineProcessor::~PipelineProcessor() {
    if (decode_pool_) destroy_thread_pool(decode_pool_);
    if (resize_pool_) destroy_thread_pool(resize_pool_);
    if (encode_pool_) destroy_thread_pool(encode_pool_);

    for (BufferPool* pool : encode_buffer_pools_) {
        destroy_buffer_pool(pool);
    }
}

void PipelineProcessor::decode_stage(const std::vector<BatchItem>& items) {
    for (size_t i = 0; i < items.size(); ++i) {
        thread_pool_enqueue(decode_pool_, [this, &items, i]() {
            const auto& item = items[i];

            DecodeResult result;
            result.task_id = i;
            result.output_path = item.output_path;
            result.options = item.options;
            result.success = false;

            ImageFormat fmt = detect_format(item.input_path);
            if (fmt == FORMAT_UNKNOWN) {
                result.error_message = "Unknown format: " + item.input_path;
                decode_queue_.push(std::move(result));
                return;
            }

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

    thread_pool_wait(decode_pool_);
    decode_queue_.set_done();
}

void PipelineProcessor::resize_stage() {
    std::vector<std::thread> workers;
    for (size_t i = 0; i < 8; ++i) {
        workers.emplace_back([this]() {
            DecodeResult decode_result;

            while (decode_queue_.pop(decode_result)) {
                ResizeResult resize_result;
                resize_result.task_id = decode_result.task_id;
                resize_result.output_path = decode_result.output_path;
                resize_result.options = decode_result.options;
                resize_result.success = false;

                if (!decode_result.success) {
                    resize_result.error_message = decode_result.error_message;
                    resize_result.pixels = nullptr;
                    resize_result.width = 0;
                    resize_result.height = 0;
                    resize_result.channels = 0;
                    resize_queue_.push(std::move(resize_result));
                    continue;
                }

                int out_w, out_h;
                calculate_dimensions(
                    decode_result.image.width,
                    decode_result.image.height,
                    decode_result.options,
                    out_w, out_h
                );

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

    for (auto& worker : workers) {
        worker.join();
    }

    resize_queue_.set_done();
}

void PipelineProcessor::encode_stage() {
    std::vector<std::thread> workers;
    for (size_t i = 0; i < encode_buffer_pools_.size(); ++i) {
        workers.emplace_back([this, i]() {
            BufferPool* buffer_pool = encode_buffer_pools_[i];
            ResizeResult resize_result;

            while (resize_queue_.pop(resize_result)) {
                if (!resize_result.success) {
                    failed_count_.fetch_add(1);
                    if (!resize_result.error_message.empty()) {
                        std::lock_guard<std::mutex> lock(errors_mutex_);
                        errors_.push_back(resize_result.error_message);
                    }
                    continue;
                }

                ImageFormat out_fmt = FORMAT_UNKNOWN;
                size_t dot_pos = resize_result.output_path.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    std::string ext = resize_result.output_path.substr(dot_pos + 1);
                    for (char& c : ext) {
                        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                    }
                    out_fmt = string_to_format(ext);
                }

                if (out_fmt == FORMAT_UNKNOWN) {
                    out_fmt = FORMAT_JPEG;
                }

                if (resize_result.pixels == nullptr || resize_result.width <= 0 ||
                    resize_result.height <= 0 || resize_result.channels <= 0) {
                    failed_count_.fetch_add(1);
                    std::lock_guard<std::mutex> lock(errors_mutex_);
                    errors_.push_back("Invalid resize data for: " + resize_result.output_path);
                    if (resize_result.pixels) delete[] resize_result.pixels;
                    continue;
                }

                ImageData img_data;
                img_data.pixels = resize_result.pixels;
                img_data.width = resize_result.width;
                img_data.height = resize_result.height;
                img_data.channels = resize_result.channels;

                bool encode_ok = encode_image(
                    resize_result.output_path,
                    img_data,
                    out_fmt,
                    resize_result.options.quality,
                    buffer_pool
                );

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

    for (auto& worker : workers) {
        worker.join();
    }
}

BatchResult PipelineProcessor::process_batch(const std::vector<BatchItem>& items) {
    success_count_ = 0;
    failed_count_ = 0;
    errors_.clear();

    std::thread decode_thread([this, &items]() { decode_stage(items); });
    std::thread resize_thread([this]() { resize_stage(); });
    std::thread encode_thread([this]() { encode_stage(); });

    decode_thread.join();
    resize_thread.join();
    encode_thread.join();

    BatchResult result;
    result.total = items.size();
    result.success = success_count_.load();
    result.failed = failed_count_.load();
    result.errors = errors_;

    return result;
}

}
}
