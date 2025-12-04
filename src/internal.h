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

#ifndef FASTRESIZE_INTERNAL_H
#define FASTRESIZE_INTERNAL_H

#include <fastresize.h>
#include <string>
#include <functional>

namespace fastresize {
namespace internal {

class ThreadPool;
class BufferPool;

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

struct ImageData {
    unsigned char* pixels;
    int width;
    int height;
    int channels;
};

ImageData decode_image(const std::string& path, ImageFormat format);
void free_image_data(ImageData& data);
bool get_image_dimensions(const std::string& path, int& width, int& height, int& channels);

bool encode_image(const std::string& path, const ImageData& data, ImageFormat format, int quality, BufferPool* buffer_pool = nullptr);

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

void set_last_error(ErrorCode code, const std::string& message);

ThreadPool* create_thread_pool(size_t num_threads);
void destroy_thread_pool(ThreadPool* pool);
void thread_pool_enqueue(ThreadPool* pool, std::function<void()> task);
void thread_pool_wait(ThreadPool* pool);

BufferPool* create_buffer_pool();
void destroy_buffer_pool(BufferPool* pool);
unsigned char* buffer_pool_acquire(BufferPool* pool, size_t size);
void buffer_pool_release(BufferPool* pool, unsigned char* buffer, size_t capacity);

}
}

#endif
