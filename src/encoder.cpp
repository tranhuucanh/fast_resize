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
#include <cstdio>
#include <csetjmp>

#include <jpeglib.h>
#include <png.h>
#include <webp/encode.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(__ARM_NEON) || defined(__aarch64__)
    #define USE_NEON 1
    #include <arm_neon.h>
#elif defined(__SSE2__) || defined(__x86_64__) || defined(_M_X64)
    #define USE_SSE2 1
    #include <emmintrin.h>
#endif

namespace fastresize {
namespace internal {

#ifdef USE_NEON
static void convert_rgba_to_rgb_neon(const unsigned char* __restrict src,
                                      unsigned char* __restrict dst,
                                      int pixel_count) {
    int i = 0;

    for (; i + 16 <= pixel_count; i += 16) {
        uint8x16x4_t rgba = vld4q_u8(src + i * 4);

        uint8x16x3_t rgb;
        rgb.val[0] = rgba.val[0];  // R
        rgb.val[1] = rgba.val[1];  // G
        rgb.val[2] = rgba.val[2];  // B

        vst3q_u8(dst + i * 3, rgb);
    }

    for (; i < pixel_count; i++) {
        dst[i * 3 + 0] = src[i * 4 + 0];
        dst[i * 3 + 1] = src[i * 4 + 1];
        dst[i * 3 + 2] = src[i * 4 + 2];
    }
}
#endif

#ifdef USE_SSE2
static void convert_rgba_to_rgb_sse2(const unsigned char* __restrict src,
                                      unsigned char* __restrict dst,
                                      int pixel_count) {
    int i = 0;

    for (; i + 4 <= pixel_count; i += 4) {
        __m128i rgba = _mm_loadu_si128((__m128i*)(src + i * 4));

        unsigned char temp[16];
        _mm_storeu_si128((__m128i*)temp, rgba);

        dst[i * 3 + 0] = temp[0];   dst[i * 3 + 1] = temp[1];   dst[i * 3 + 2] = temp[2];
        dst[i * 3 + 3] = temp[4];   dst[i * 3 + 4] = temp[5];   dst[i * 3 + 5] = temp[6];
        dst[i * 3 + 6] = temp[8];   dst[i * 3 + 7] = temp[9];   dst[i * 3 + 8] = temp[10];
        dst[i * 3 + 9] = temp[12];  dst[i * 3 + 10] = temp[13]; dst[i * 3 + 11] = temp[14];
    }

    for (; i < pixel_count; i++) {
        dst[i * 3 + 0] = src[i * 4 + 0];
        dst[i * 3 + 1] = src[i * 4 + 1];
        dst[i * 3 + 2] = src[i * 4 + 2];
    }
}
#endif

static void convert_rgba_to_rgb_scalar(const unsigned char* __restrict src,
                                        unsigned char* __restrict dst,
                                        int pixel_count) {
    for (int i = 0; i < pixel_count; i++) {
        dst[i * 3 + 0] = src[i * 4 + 0];
        dst[i * 3 + 1] = src[i * 4 + 1];
        dst[i * 3 + 2] = src[i * 4 + 2];
    }
}

static void convert_rgba_to_rgb(const unsigned char* src, unsigned char* dst, int pixel_count) {
#ifdef USE_NEON
    convert_rgba_to_rgb_neon(src, dst, pixel_count);
#elif defined(USE_SSE2)
    convert_rgba_to_rgb_sse2(src, dst, pixel_count);
#else
    convert_rgba_to_rgb_scalar(src, dst, pixel_count);
#endif
}

struct jpeg_error_mgr_ext {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void jpeg_encode_error_exit(j_common_ptr cinfo) {
    jpeg_error_mgr_ext* myerr = (jpeg_error_mgr_ext*)cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);
}

static const int JPEG_SCANLINE_BATCH = 8;

bool encode_jpeg(const std::string& path, const ImageData& data, int quality, BufferPool* buffer_pool = nullptr) {
    FILE* outfile = fopen(path.c_str(), "wb");
    if (!outfile) return false;

    unsigned char* rgb_buffer = nullptr;
    size_t rgb_buffer_capacity = 0;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr_ext jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_encode_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        if (rgb_buffer) {
            if (rgb_buffer_capacity > 0 && buffer_pool) {
                buffer_pool_release(buffer_pool, rgb_buffer, rgb_buffer_capacity);
            } else {
                delete[] rgb_buffer;
            }
        }
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = data.width;
    cinfo.image_height = data.height;

    const unsigned char* encode_pixels = data.pixels;
    int encode_components = data.channels;

    if (data.channels == 1) {
        cinfo.in_color_space = JCS_GRAYSCALE;
        cinfo.input_components = 1;
    } else if (data.channels == 3) {
        cinfo.in_color_space = JCS_RGB;
        cinfo.input_components = 3;
    } else if (data.channels == 4) {
        cinfo.in_color_space = JCS_RGB;
        cinfo.input_components = 3;
        encode_components = 3;

        size_t rgb_size = static_cast<size_t>(data.width) * data.height * 3;
        if (buffer_pool) {
            rgb_buffer = buffer_pool_acquire(buffer_pool, rgb_size);
            rgb_buffer_capacity = rgb_size;
        } else {
            rgb_buffer = new unsigned char[rgb_size];
            rgb_buffer_capacity = 0;
        }

        convert_rgba_to_rgb(data.pixels, rgb_buffer, data.width * data.height);
        encode_pixels = rgb_buffer;
    } else {
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        return false;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    cinfo.dct_method = JDCT_IFAST;

    jpeg_start_compress(&cinfo, TRUE);

    size_t row_stride = data.width * encode_components;
    JSAMPROW row_pointers[JPEG_SCANLINE_BATCH];

    while (cinfo.next_scanline < cinfo.image_height) {
        int remaining = cinfo.image_height - cinfo.next_scanline;
        int batch_size = (remaining < JPEG_SCANLINE_BATCH) ? remaining : JPEG_SCANLINE_BATCH;

        for (int i = 0; i < batch_size; i++) {
            row_pointers[i] = const_cast<JSAMPROW>(
                &encode_pixels[(cinfo.next_scanline + i) * row_stride]
            );
        }

        jpeg_write_scanlines(&cinfo, row_pointers, batch_size);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    if (rgb_buffer) {
        if (rgb_buffer_capacity > 0 && buffer_pool) {
            buffer_pool_release(buffer_pool, rgb_buffer, rgb_buffer_capacity);
        } else {
            delete[] rgb_buffer;
        }
    }

    return true;
}

bool encode_png(const std::string& path, const ImageData& data, int quality) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);

    int compression_level = 9 - ((quality - 1) * 9 / 99);
    if (compression_level < 0) compression_level = 0;
    if (compression_level > 9) compression_level = 9;
    png_set_compression_level(png, compression_level);

    int color_type;
    switch (data.channels) {
        case 1: color_type = PNG_COLOR_TYPE_GRAY; break;
        case 2: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case 3: color_type = PNG_COLOR_TYPE_RGB; break;
        case 4: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
        default:
            png_destroy_write_struct(&png, &info);
            fclose(fp);
            return false;
    }

    png_set_IHDR(png, info, data.width, data.height, 8,
                 color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    size_t row_bytes = data.width * data.channels;
    png_bytep* row_pointers = new png_bytep[data.height];
    for (int y = 0; y < data.height; y++) {
        row_pointers[y] = data.pixels + y * row_bytes;
    }

    png_write_image(png, row_pointers);
    png_write_end(png, nullptr);

    delete[] row_pointers;
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    return true;
}

bool encode_webp(const std::string& path, const ImageData& data, int quality) {
    uint8_t* output = nullptr;
    size_t output_size = 0;

    if (data.channels == 4) {
        output_size = WebPEncodeRGBA(data.pixels, data.width, data.height,
                                     data.width * 4, quality, &output);
    } else if (data.channels == 3) {
        output_size = WebPEncodeRGB(data.pixels, data.width, data.height,
                                    data.width * 3, quality, &output);
    } else {
        return false;
    }

    if (output_size == 0 || !output) {
        if (output) WebPFree(output);
        return false;
    }

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        WebPFree(output);
        return false;
    }

    size_t written = fwrite(output, 1, output_size, fp);
    fclose(fp);
    WebPFree(output);

    return written == output_size;
}

bool encode_image(const std::string& path, const ImageData& data, ImageFormat format, int quality, BufferPool* buffer_pool) {
    if (!data.pixels || data.width <= 0 || data.height <= 0) {
        return false;
    }

    switch (format) {
        case FORMAT_JPEG:
            return encode_jpeg(path, data, quality, buffer_pool);

        case FORMAT_PNG:
            return encode_png(path, data, quality);

        case FORMAT_WEBP:
            return encode_webp(path, data, quality);

        case FORMAT_BMP:
            {
                int result = stbi_write_bmp(path.c_str(), data.width, data.height, data.channels, data.pixels);
                if (!result) {
                    set_last_error(ENCODE_ERROR, "Failed to encode BMP image");
                    return false;
                }
                return true;
            }

        default:
            set_last_error(UNSUPPORTED_FORMAT, "Unsupported output format");
            return false;
    }
}

}
}
