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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <vector>

namespace fastresize {
namespace internal {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    void enqueue(std::function<void()> task);

    void wait();

    size_t get_thread_count() const { return threads_.size(); }

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable wait_condition_;
    std::atomic<bool> stop_;
    std::atomic<int> active_tasks_;
    std::atomic<int> queued_tasks_;
};

ThreadPool::ThreadPool(size_t num_threads)
    : stop_(false)
    , active_tasks_(0)
    , queued_tasks_(0)
{
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if (stop_ && tasks_.empty())
                        return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                    --queued_tasks_;
                }

                ++active_tasks_;
                task();
                --active_tasks_;
                wait_condition_.notify_all();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::move(task));
        ++queued_tasks_;
    }
    condition_.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    wait_condition_.wait(lock, [this] {
        return queued_tasks_ == 0 && active_tasks_ == 0;
    });
}

class BufferPool {
public:
    BufferPool() = default;
    ~BufferPool();

    unsigned char* acquire(size_t size);
    void release(unsigned char* buffer, size_t capacity);

private:
    struct Buffer {
        unsigned char* data;
        size_t capacity;
    };

    std::vector<Buffer> pool_;
    std::mutex mutex_;
};

BufferPool::~BufferPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Buffer& buf : pool_) {
        delete[] buf.data;
    }
    pool_.clear();
}

unsigned char* BufferPool::acquire(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
        if (it->capacity >= size) {
            unsigned char* buffer = it->data;
            pool_.erase(it);
            return buffer;
        }
    }

    return new unsigned char[size];
}

void BufferPool::release(unsigned char* buffer, size_t capacity) {
    if (!buffer) return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_.size() < 32) {
        Buffer buf;
        buf.data = buffer;
        buf.capacity = capacity;
        pool_.push_back(buf);
    } else {
        delete[] buffer;
    }
}

ThreadPool* create_thread_pool(size_t num_threads) {
    return new ThreadPool(num_threads);
}

void destroy_thread_pool(ThreadPool* pool) {
    delete pool;
}

void thread_pool_enqueue(ThreadPool* pool, std::function<void()> task) {
    if (pool) {
        pool->enqueue(std::move(task));
    }
}

void thread_pool_wait(ThreadPool* pool) {
    if (pool) {
        pool->wait();
    }
}

BufferPool* create_buffer_pool() {
    return new BufferPool();
}

void destroy_buffer_pool(BufferPool* pool) {
    delete pool;
}

unsigned char* buffer_pool_acquire(BufferPool* pool, size_t size) {
    return pool ? pool->acquire(size) : new unsigned char[size];
}

void buffer_pool_release(BufferPool* pool, unsigned char* buffer, size_t capacity) {
    if (pool) {
        pool->release(buffer, capacity);
    } else {
        delete[] buffer;
    }
}

}
}
