#include "internal.h"
#include <cmath>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace fastresize {
namespace internal {

// ============================================
// Dimension Calculation
// ============================================

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
                // Calculate scaling to fit within target dimensions
                float ratio_w = static_cast<float>(out_w) / in_w;
                float ratio_h = static_cast<float>(out_h) / in_h;
                float ratio = std::min(ratio_w, ratio_h);
                out_w = static_cast<int>(std::round(in_w * ratio));
                out_h = static_cast<int>(std::round(in_h * ratio));
            }
            break;
    }

    // Ensure dimensions are at least 1x1
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;
}

// ============================================
// Image Resizing
// ============================================

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

    // Allocate output buffer
    size_t output_size = static_cast<size_t>(output_w) * output_h * channels;
    *output_pixels = new unsigned char[output_size];

    // Phase D Optimization: Auto-select TRIANGLE filter for large downscales
    // TRIANGLE is 10% faster than MITCHELL on Apple Silicon with good quality
    float downscale_ratio_w = static_cast<float>(input_w) / output_w;
    float downscale_ratio_h = static_cast<float>(input_h) / output_h;
    float max_downscale = (downscale_ratio_w > downscale_ratio_h) ? downscale_ratio_w : downscale_ratio_h;

    // Map our filter enum to stb filter
    stbir_filter stb_filter;

    // Auto-optimize: Use TRIANGLE for large downscales (>=3x) when MITCHELL is requested
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

    // Determine pixel layout
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

    // Perform resize
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

} // namespace internal
} // namespace fastresize
