#ifndef FASTRESIZE_INTERNAL_H
#define FASTRESIZE_INTERNAL_H

#include <fastresize.h>
#include <string>
#include <functional>

namespace fastresize {
namespace internal {

// ============================================
// Image Format Detection
// ============================================

enum ImageFormat {
    FORMAT_UNKNOWN,
    FORMAT_JPEG,
    FORMAT_PNG,
    FORMAT_WEBP,
    FORMAT_BMP
};

ImageFormat detect_format(const std::string& path);
std::string format_to_string(ImageFormat format);
ImageFormat string_to_format(const std::string& str);

// ============================================
// Image Data Structure
// ============================================

struct ImageData {
    unsigned char* pixels;
    int width;
    int height;
    int channels;
};

// ============================================
// Decoder Functions
// ============================================

ImageData decode_image(const std::string& path, ImageFormat format);
void free_image_data(ImageData& data);
bool get_image_dimensions(const std::string& path, int& width, int& height, int& channels);

// ============================================
// Encoder Functions
// ============================================

bool encode_image(const std::string& path, const ImageData& data, ImageFormat format, int quality);

// ============================================
// Resizer Functions
// ============================================

void calculate_dimensions(
    int in_w, int in_h,
    const ResizeOptions& opts,
    int& out_w, int& out_h
);

bool resize_image(
    const unsigned char* input_pixels,
    int input_w, int input_h, int channels,
    unsigned char** output_pixels,
    int output_w, int output_h,
    const ResizeOptions& opts
);

// ============================================
// Error Handling
// ============================================

void set_last_error(ErrorCode code, const std::string& message);

// ============================================
// Thread Pool
// ============================================

class ThreadPool;
ThreadPool* create_thread_pool(size_t num_threads);
void destroy_thread_pool(ThreadPool* pool);
void thread_pool_enqueue(ThreadPool* pool, std::function<void()> task);
void thread_pool_wait(ThreadPool* pool);

// ============================================
// Buffer Pool
// ============================================

class BufferPool;
BufferPool* create_buffer_pool();
void destroy_buffer_pool(BufferPool* pool);
unsigned char* buffer_pool_acquire(BufferPool* pool, size_t size);
void buffer_pool_release(BufferPool* pool, unsigned char* buffer, size_t capacity);

} // namespace internal
} // namespace fastresize

#endif // FASTRESIZE_INTERNAL_H
