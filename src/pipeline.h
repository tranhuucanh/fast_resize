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

#pragma once

#include "internal.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace fastresize {
namespace internal {

template<typename T>
class BoundedQueue {
public:
    BoundedQueue(size_t capacity)
        : capacity_(capacity)
        , done_(false)
    {}

    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_not_full_.wait(lock, [this]() {
            return queue_.size() < capacity_ || done_;
        });

        if (done_) return false;

        queue_.push(std::move(item));
        lock.unlock();
        cv_not_empty_.notify_one();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_not_empty_.wait(lock, [this]() {
            return !queue_.empty() || done_;
        });

        if (queue_.empty() && done_) return false;

        item = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        cv_not_full_.notify_one();
        return true;
    }

    void set_done() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_ = true;
        lock.unlock();
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
    bool done_;
};

struct DecodeTask {
    std::string input_path;
    std::string output_path;
    ResizeOptions options;
    int task_id;
};

struct DecodeResult {
    ImageData image;
    std::string output_path;
    ResizeOptions options;
    int task_id;
    bool success;
    std::string error_message;
};

struct ResizeResult {
    unsigned char* pixels;
    int width;
    int height;
    int channels;
    std::string output_path;
    ResizeOptions options;
    int task_id;
    bool success;
    std::string error_message;
};

class PipelineProcessor {
public:
    PipelineProcessor(
        size_t decode_threads = 4,
        size_t resize_threads = 8,
        size_t encode_threads = 4,
        size_t queue_capacity = 32
    );

    ~PipelineProcessor();
    BatchResult process_batch(const std::vector<BatchItem>& items);

private:
    ThreadPool* decode_pool_;
    ThreadPool* resize_pool_;
    ThreadPool* encode_pool_;

    std::vector<BufferPool*> encode_buffer_pools_;

    BoundedQueue<DecodeResult> decode_queue_;
    BoundedQueue<ResizeResult> resize_queue_;

    std::atomic<int> success_count_;
    std::atomic<int> failed_count_;
    std::mutex errors_mutex_;
    std::vector<std::string> errors_;

    void decode_stage(const std::vector<BatchItem>& items);
    void resize_stage();
    void encode_stage();
};

}
}
