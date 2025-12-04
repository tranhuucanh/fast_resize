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
#include "simd_resize.h"
#include <cmath>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace fastresize {
namespace internal {

void calculate_dimensions(
    int in_w, int in_h,
    const ResizeOptions& opts,
    int& out_w, int& out_h
) {
    switch (opts.mode) {
        case ResizeOptions::SCALE_PERCENT:
            out_w = static_cast<int>(std::round(in_w * opts.scale_percent));
            out_h = static_cast<int>(std::round(in_h * opts.scale_percent));
            break;

        case ResizeOptions::FIT_WIDTH:
            out_w = opts.target_width;
            if (opts.keep_aspect_ratio) {
                float ratio = static_cast<float>(out_w) / in_w;
                out_h = static_cast<int>(std::round(in_h * ratio));
            } else {
                out_h = in_h;
            }
            break;

        case ResizeOptions::FIT_HEIGHT:
            out_h = opts.target_height;
            if (opts.keep_aspect_ratio) {
                float ratio = static_cast<float>(out_h) / in_h;
                out_w = static_cast<int>(std::round(in_w * ratio));
            } else {
                out_w = in_w;
            }
            break;

        case ResizeOptions::EXACT_SIZE:
            out_w = opts.target_width;
            out_h = opts.target_height;
            if (opts.keep_aspect_ratio) {
                float ratio_w = static_cast<float>(out_w) / in_w;
                float ratio_h = static_cast<float>(out_h) / in_h;
                float ratio = std::min(ratio_w, ratio_h);
                out_w = static_cast<int>(std::round(in_w * ratio));
                out_h = static_cast<int>(std::round(in_h * ratio));
            }
            break;
    }

    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;
}

bool resize_image(
    const unsigned char* input_pixels,
    int input_w, int input_h, int channels,
    unsigned char** output_pixels,
    int output_w, int output_h,
    const ResizeOptions& opts
) {
    if (!input_pixels || input_w <= 0 || input_h <= 0 ||
        output_w <= 0 || output_h <= 0 || channels <= 0) {
        set_last_error(RESIZE_ERROR, "Invalid input parameters for resize");
        return false;
    }

    size_t output_size = static_cast<size_t>(output_w) * output_h * channels;
    *output_pixels = new unsigned char[output_size];

    float downscale_ratio_w = static_cast<float>(input_w) / output_w;
    float downscale_ratio_h = static_cast<float>(input_h) / output_h;
    float max_downscale = (downscale_ratio_w > downscale_ratio_h) ? downscale_ratio_w : downscale_ratio_h;

    stbir_filter stb_filter;

    if (max_downscale >= 3.0f && opts.filter == ResizeOptions::MITCHELL) {
        stb_filter = STBIR_FILTER_TRIANGLE;
    } else {
        switch (opts.filter) {
            case ResizeOptions::MITCHELL:
                stb_filter = STBIR_FILTER_MITCHELL;
                break;
            case ResizeOptions::CATMULL_ROM:
                stb_filter = STBIR_FILTER_CATMULLROM;
                break;
            case ResizeOptions::BOX:
                stb_filter = STBIR_FILTER_BOX;
                break;
            case ResizeOptions::TRIANGLE:
                stb_filter = STBIR_FILTER_TRIANGLE;
                break;
            default:
                stb_filter = STBIR_FILTER_MITCHELL;
        }
    }

    bool use_simd = (output_w < input_w && output_h < input_h) &&
                    (stb_filter == STBIR_FILTER_TRIANGLE ||
                     (stb_filter == STBIR_FILTER_MITCHELL && max_downscale < 3.0f)) &&
                    (channels == 3 || channels == 4 || channels == 1);

    if (use_simd) {
        bool simd_ok = simd_resize(
            input_pixels, input_w, input_h, channels,
            *output_pixels, output_w, output_h,
            ResizeQuality::FAST
        );

        if (simd_ok) {
            return true;
        }
    }

    stbir_pixel_layout pixel_layout;
    switch (channels) {
        case 1:
            pixel_layout = STBIR_1CHANNEL;
            break;
        case 2:
            pixel_layout = STBIR_2CHANNEL;
            break;
        case 3:
            pixel_layout = STBIR_RGB;
            break;
        case 4:
            pixel_layout = STBIR_RGBA;
            break;
        default:
            delete[] *output_pixels;
            *output_pixels = nullptr;
            set_last_error(RESIZE_ERROR, "Unsupported number of channels");
            return false;
    }

    void* result = stbir_resize(
        input_pixels, input_w, input_h, 0,
        *output_pixels, output_w, output_h, 0,
        pixel_layout, STBIR_TYPE_UINT8,
        STBIR_EDGE_CLAMP,
        stb_filter
    );

    if (!result) {
        delete[] *output_pixels;
        *output_pixels = nullptr;
        set_last_error(RESIZE_ERROR, "stb_image_resize2 failed");
        return false;
    }

    return true;
}

}
}
