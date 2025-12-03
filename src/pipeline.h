#pragma once

#include "internal.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace fastresize {
namespace internal {

// ============================================
// Bounded Queue for Pipeline Stages
// ============================================

template<typename T>
class BoundedQueue {
public:
    BoundedQueue(size_t capacity)
        : capacity_(capacity)
        , done_(false)
    {}

    // Push item to queue (blocks if full)
    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until queue has space or is done
        cv_not_full_.wait(lock, [this]() {
            return queue_.size() < capacity_ || done_;
        });

        if (done_) return false;

        queue_.push(std::move(item));
        lock.unlock();
        cv_not_empty_.notify_one();
        return true;
    }

    // Pop item from queue (blocks if empty)
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until queue has items or is done
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

    // Signal that no more items will be added
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

// ============================================
// Pipeline Task Structures
// ============================================

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

// ============================================
// 3-Stage Pipeline Processor
// ============================================

class PipelineProcessor {
public:
    PipelineProcessor(
        size_t decode_threads = 4,
        size_t resize_threads = 8,
        size_t encode_threads = 4,
        size_t queue_capacity = 32
    );

    ~PipelineProcessor();

    // Process batch using 3-stage pipeline
    BatchResult process_batch(const std::vector<BatchItem>& items);

private:
    // Thread pools for each stage
    ThreadPool* decode_pool_;
    ThreadPool* resize_pool_;
    ThreadPool* encode_pool_;

    // Queues between stages
    BoundedQueue<DecodeResult> decode_queue_;
    BoundedQueue<ResizeResult> resize_queue_;

    // Result tracking
    std::atomic<int> success_count_;
    std::atomic<int> failed_count_;
    std::mutex errors_mutex_;
    std::vector<std::string> errors_;

    // Pipeline stages
    void decode_stage(const std::vector<BatchItem>& items);
    void resize_stage();
    void encode_stage();
};

} // namespace internal
} // namespace fastresize
