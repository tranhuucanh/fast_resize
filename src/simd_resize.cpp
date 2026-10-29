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

#include "simd_resize.h"
#include <cstring>
#include <cmath>
#include <algorithm>

#if defined(__ARM_NEON) || defined(__aarch64__)
    #define USE_NEON 1
    #include <arm_neon.h>
#elif defined(__AVX2__)
    #define USE_AVX2 1
    #include <immintrin.h>
#elif defined(__SSE2__) || defined(__x86_64__) || defined(_M_X64)
    #define USE_SSE2 1
    #include <emmintrin.h>
#endif

namespace fastresize {
namespace internal {

#ifdef USE_NEON

// Load one RGBA pixel. A 4-byte copy avoids vld1_u8's 8-byte read, which
// walks 4 bytes past the last pixel of a row.
static inline int16x4_t load_rgba_s16(const uint8_t* p) {
    uint32_t px;
    std::memcpy(&px, p, sizeof(px));
    uint8x8_t bytes = vreinterpret_u8_u32(vdup_n_u32(px));
    return vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(bytes)));
}

// Bilinear blend in int32. int16 multiply overflows: a weight of 256 and a
// channel of 255 produce 65280, past the signed 16-bit max of 32767.
// The wrapped negative value is then saturated to 0, which wipes opaque alpha.
static inline void store_bilinear_rgba(
    const uint8_t* row1, const uint8_t* row2,
    int x1, int x2,
    int wx1, int wx2,
    int wy1, int wy2,
    uint8_t* out
) {
    int16x4_t tl = load_rgba_s16(row1 + x1 * 4);
    int16x4_t tr = load_rgba_s16(row1 + x2 * 4);
    int16x4_t bl = load_rgba_s16(row2 + x1 * 4);
    int16x4_t br = load_rgba_s16(row2 + x2 * 4);

    int16x4_t wx1v = vdup_n_s16(static_cast<int16_t>(wx1));
    int16x4_t wx2v = vdup_n_s16(static_cast<int16_t>(wx2));
    int16x4_t wy1v = vdup_n_s16(static_cast<int16_t>(wy1));
    int16x4_t wy2v = vdup_n_s16(static_cast<int16_t>(wy2));

    int32x4_t top = vmlal_s16(vmull_s16(tl, wx1v), tr, wx2v);
    int32x4_t bottom = vmlal_s16(vmull_s16(bl, wx1v), br, wx2v);
    int16x4_t top16 = vmovn_s32(vshrq_n_s32(top, 8));
    int16x4_t bottom16 = vmovn_s32(vshrq_n_s32(bottom, 8));

    int32x4_t mixed = vmlal_s16(vmull_s16(top16, wy1v), bottom16, wy2v);
    uint16x4_t result16 = vqmovun_s32(vshrq_n_s32(mixed, 8));
    uint8x8_t result8 = vqmovn_u16(vcombine_u16(result16, result16));
    vst1_lane_u32(reinterpret_cast<uint32_t*>(out), vreinterpret_u32_u8(result8), 0);
}

static void resize_bilinear_neon_rgba(
    const uint8_t* __restrict src, int src_w, int src_h,
    uint8_t* __restrict dst, int dst_w, int dst_h,
    int channels
) {
    const int FRAC_BITS = 8;
    const int FRAC_ONE = 1 << FRAC_BITS;

    // int32 overflows once src_w > 32768: (src_w - 1) << 16 exceeds 2^31 - 1.
    int64_t x_ratio_fp = ((int64_t)(src_w - 1) << 16) / dst_w;
    int64_t y_ratio_fp = ((int64_t)(src_h - 1) << 16) / dst_h;

    int src_stride = src_w * channels;
    int dst_stride = dst_w * channels;

    for (int y = 0; y < dst_h; y++) {
        int64_t src_y_fp = ((int64_t)y * y_ratio_fp) >> 8;
        int y1 = (int)(src_y_fp >> FRAC_BITS);
        int y2 = std::min(y1 + 1, src_h - 1);
        int y_frac = (int)(src_y_fp & (FRAC_ONE - 1));
        int wy1 = FRAC_ONE - y_frac;
        int wy2 = y_frac;

        const uint8_t* row1 = src + y1 * src_stride;
        const uint8_t* row2 = src + y2 * src_stride;
        uint8_t* out_row = dst + y * dst_stride;

        if (y + 1 < dst_h) {
            int64_t next_y_fp = ((int64_t)(y + 1) * y_ratio_fp) >> 8;
            int next_y1 = (int)(next_y_fp >> FRAC_BITS);
            int next_y2 = std::min(next_y1 + 1, src_h - 1);
            __builtin_prefetch(src + next_y1 * src_stride, 0, 3);
            __builtin_prefetch(src + next_y2 * src_stride, 0, 3);
        }

        int x = 0;

        for (; x + 16 <= dst_w; x += 16) {
            int64_t src_x_fp[16];
            int x1_arr[16], x2_arr[16], x_frac_arr[16];

            if (x + 32 < dst_w) {
                int64_t prefetch_x_fp = ((int64_t)(x + 32) * x_ratio_fp) >> 8;
                int prefetch_x = (int)(prefetch_x_fp >> FRAC_BITS);
                __builtin_prefetch(row1 + prefetch_x * channels, 0, 2);
                __builtin_prefetch(row2 + prefetch_x * channels, 0, 2);
            }

            for (int i = 0; i < 16; i++) {
                src_x_fp[i] = ((int64_t)(x + i) * x_ratio_fp) >> 8;
                x1_arr[i] = (int)(src_x_fp[i] >> FRAC_BITS);
                x2_arr[i] = std::min(x1_arr[i] + 1, src_w - 1);
                x_frac_arr[i] = (int)(src_x_fp[i] & (FRAC_ONE - 1));
            }

            if (channels == 4) {
                for (int i = 0; i < 16; i++) {
                    int wx2 = x_frac_arr[i];
                    int wx1 = FRAC_ONE - wx2;
                    store_bilinear_rgba(
                        row1, row2, x1_arr[i], x2_arr[i],
                        wx1, wx2, wy1, wy2,
                        out_row + (x + i) * 4
                    );
                }
            } else if (channels == 3) {
                for (int i = 0; i < 16; i++) {
                    const uint8_t* p_tl = row1 + x1_arr[i] * 3;
                    const uint8_t* p_tr = row1 + x2_arr[i] * 3;
                    const uint8_t* p_bl = row2 + x1_arr[i] * 3;
                    const uint8_t* p_br = row2 + x2_arr[i] * 3;

                    int16_t wx2 = x_frac_arr[i];
                    int16_t wx1 = FRAC_ONE - wx2;
                    int16_t wy1 = FRAC_ONE - y_frac;
                    int16_t wy2 = y_frac;

                    uint8_t* out = out_row + (x + i) * 3;

                    for (int c = 0; c < 3; c++) {
                        int top = (p_tl[c] * wx1 + p_tr[c] * wx2) >> FRAC_BITS;
                        int bottom = (p_bl[c] * wx1 + p_br[c] * wx2) >> FRAC_BITS;
                        int val = (top * wy1 + bottom * wy2) >> FRAC_BITS;
                        out[c] = (uint8_t)std::min(std::max(val, 0), 255);
                    }
                }
            }
        }

        for (; x + 8 <= dst_w; x += 8) {
            int64_t src_x_fp[8];
            int x1_arr[8], x2_arr[8], x_frac_arr[8];

            for (int i = 0; i < 8; i++) {
                src_x_fp[i] = ((int64_t)(x + i) * x_ratio_fp) >> 8;
                x1_arr[i] = (int)(src_x_fp[i] >> FRAC_BITS);
                x2_arr[i] = std::min(x1_arr[i] + 1, src_w - 1);
                x_frac_arr[i] = (int)(src_x_fp[i] & (FRAC_ONE - 1));
            }

            if (channels == 4) {
                for (int i = 0; i < 8; i++) {
                    int wx2 = x_frac_arr[i];
                    int wx1 = FRAC_ONE - wx2;
                    store_bilinear_rgba(
                        row1, row2, x1_arr[i], x2_arr[i],
                        wx1, wx2, wy1, wy2,
                        out_row + (x + i) * 4
                    );
                }
            } else if (channels == 3) {
                for (int i = 0; i < 8; i++) {
                    const uint8_t* p_tl = row1 + x1_arr[i] * 3;
                    const uint8_t* p_tr = row1 + x2_arr[i] * 3;
                    const uint8_t* p_bl = row2 + x1_arr[i] * 3;
                    const uint8_t* p_br = row2 + x2_arr[i] * 3;

                    int16_t wx2 = x_frac_arr[i];
                    int16_t wx1 = FRAC_ONE - wx2;
                    int16_t wy1 = FRAC_ONE - y_frac;
                    int16_t wy2 = y_frac;

                    uint8_t* out = out_row + (x + i) * 3;

                    for (int c = 0; c < 3; c++) {
                        int top = (p_tl[c] * wx1 + p_tr[c] * wx2) >> FRAC_BITS;
                        int bottom = (p_bl[c] * wx1 + p_br[c] * wx2) >> FRAC_BITS;
                        int val = (top * wy1 + bottom * wy2) >> FRAC_BITS;
                        out[c] = (uint8_t)std::min(std::max(val, 0), 255);
                    }
                }
            }
        }

        for (; x + 4 <= dst_w; x += 4) {
            int64_t src_x_fp[4];
            int x1_arr[4], x2_arr[4], x_frac_arr[4];

            for (int i = 0; i < 4; i++) {
                src_x_fp[i] = ((int64_t)(x + i) * x_ratio_fp) >> 8;
                x1_arr[i] = (int)(src_x_fp[i] >> FRAC_BITS);
                x2_arr[i] = std::min(x1_arr[i] + 1, src_w - 1);
                x_frac_arr[i] = (int)(src_x_fp[i] & (FRAC_ONE - 1));
            }

            if (channels == 4) {
                for (int i = 0; i < 4; i++) {
                    int wx2 = x_frac_arr[i];
                    int wx1 = FRAC_ONE - wx2;
                    store_bilinear_rgba(
                        row1, row2, x1_arr[i], x2_arr[i],
                        wx1, wx2, wy1, wy2,
                        out_row + (x + i) * 4
                    );
                }
            } else if (channels == 3) {
                for (int i = 0; i < 4; i++) {
                    const uint8_t* p_tl = row1 + x1_arr[i] * 3;
                    const uint8_t* p_tr = row1 + x2_arr[i] * 3;
                    const uint8_t* p_bl = row2 + x1_arr[i] * 3;
                    const uint8_t* p_br = row2 + x2_arr[i] * 3;

                    int16_t wx2 = x_frac_arr[i];
                    int16_t wx1 = FRAC_ONE - wx2;
                    int16_t wy1 = FRAC_ONE - y_frac;
                    int16_t wy2 = y_frac;

                    uint8_t* out = out_row + (x + i) * 3;

                    for (int c = 0; c < 3; c++) {
                        int top = (p_tl[c] * wx1 + p_tr[c] * wx2) >> FRAC_BITS;
                        int bottom = (p_bl[c] * wx1 + p_br[c] * wx2) >> FRAC_BITS;
                        int val = (top * wy1 + bottom * wy2) >> FRAC_BITS;
                        out[c] = (uint8_t)std::min(std::max(val, 0), 255);
                    }
                }
            }
        }

        for (; x < dst_w; x++) {
            int64_t src_x_fp = ((int64_t)x * x_ratio_fp) >> 8;
            int x1 = (int)(src_x_fp >> FRAC_BITS);
            int x2 = std::min(x1 + 1, src_w - 1);
            int x_frac = (int)(src_x_fp & (FRAC_ONE - 1));
            int x_frac_inv = FRAC_ONE - x_frac;
            int y_frac_inv = FRAC_ONE - y_frac;

            const uint8_t* p1 = row1 + x1 * channels;
            const uint8_t* p2 = row1 + x2 * channels;
            const uint8_t* p3 = row2 + x1 * channels;
            const uint8_t* p4 = row2 + x2 * channels;

            int w1 = (x_frac_inv * y_frac_inv) >> FRAC_BITS;
            int w2 = (x_frac * y_frac_inv) >> FRAC_BITS;
            int w3 = (x_frac_inv * y_frac) >> FRAC_BITS;
            int w4 = (x_frac * y_frac) >> FRAC_BITS;

            uint8_t* out = out_row + x * channels;

            for (int c = 0; c < channels; c++) {
                int val = (p1[c] * w1 + p2[c] * w2 + p3[c] * w3 + p4[c] * w4) >> FRAC_BITS;
                out[c] = (uint8_t)std::min(val, 255);
            }
        }
    }
}

static void resize_area_neon(
    const uint8_t* __restrict src, int src_w, int src_h,
    uint8_t* __restrict dst, int dst_w, int dst_h,
    int channels
) {
    // Keep float so images that already resize correctly keep the same source bins.
    float x_scale = (float)src_w / dst_w;
    float y_scale = (float)src_h / dst_h;

    int src_stride = src_w * channels;
    int dst_stride = dst_w * channels;

    for (int dy = 0; dy < dst_h; dy++) {
        int sy_start = (int)(dy * y_scale);
        int sy_end = std::min((int)((dy + 1) * y_scale), src_h);
        int y_count = sy_end - sy_start;
        if (y_count < 1) y_count = 1;

        if (dy + 1 < dst_h) {
            int next_sy_start = (int)((dy + 1) * y_scale);
            __builtin_prefetch(src + next_sy_start * src_stride, 0, 3);
        }

        uint8_t* out_row = dst + dy * dst_stride;

        for (int dx = 0; dx < dst_w; dx++) {
            int sx_start = (int)(dx * x_scale);
            int sx_end = std::min((int)((dx + 1) * x_scale), src_w);
            int x_count = sx_end - sx_start;
            if (x_count < 1) x_count = 1;

            // x_count * y_count overflows int32 on a huge window, and a uint32
            // channel sum overflows once that window holds more than 255 * n == 2^32.
            uint64_t pixel_count = (uint64_t)x_count * (uint64_t)y_count;
            if (pixel_count == 0) pixel_count = 1;
            bool sum_fits_u32 = pixel_count <= 4294967295ull / 255ull;

            if (channels == 4 && sum_fits_u32) {
                uint32x4_t sum = vdupq_n_u32(0);

                for (int sy = sy_start; sy < sy_end; sy++) {
                    const uint8_t* src_row = src + sy * src_stride + sx_start * 4;

                    int sx = 0;
                    for (; sx + 4 <= x_count; sx += 4) {
                        uint8x16_t pixels = vld1q_u8(src_row + sx * 4);

                        uint16x8_t lo = vmovl_u8(vget_low_u8(pixels));
                        uint16x8_t hi = vmovl_u8(vget_high_u8(pixels));

                        sum = vaddq_u32(sum, vmovl_u16(vget_low_u16(lo)));
                        sum = vaddq_u32(sum, vmovl_u16(vget_high_u16(lo)));
                        sum = vaddq_u32(sum, vmovl_u16(vget_low_u16(hi)));
                        sum = vaddq_u32(sum, vmovl_u16(vget_high_u16(hi)));
                    }

                    for (; sx < x_count; sx++) {
                        const uint8_t* p = src_row + sx * 4;
                        uint32_t vals[4] = {p[0], p[1], p[2], p[3]};
                        sum = vaddq_u32(sum, vld1q_u32(vals));
                    }
                }

                uint32_t sums[4];
                vst1q_u32(sums, sum);

                uint8_t* out = out_row + dx * 4;
                out[0] = (uint8_t)(sums[0] / pixel_count);
                out[1] = (uint8_t)(sums[1] / pixel_count);
                out[2] = (uint8_t)(sums[2] / pixel_count);
                out[3] = (uint8_t)(sums[3] / pixel_count);

            } else if (channels == 4) {
                uint64_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;

                for (int sy = sy_start; sy < sy_end; sy++) {
                    const uint8_t* src_row = src + sy * src_stride + sx_start * 4;
                    for (int sx = 0; sx < x_count; sx++) {
                        const uint8_t* p = src_row + sx * 4;
                        sum_r += p[0];
                        sum_g += p[1];
                        sum_b += p[2];
                        sum_a += p[3];
                    }
                }

                uint8_t* out = out_row + dx * 4;
                out[0] = (uint8_t)(sum_r / pixel_count);
                out[1] = (uint8_t)(sum_g / pixel_count);
                out[2] = (uint8_t)(sum_b / pixel_count);
                out[3] = (uint8_t)(sum_a / pixel_count);

            } else if (channels == 3 && sum_fits_u32) {
                uint32_t sum_r = 0, sum_g = 0, sum_b = 0;

                for (int sy = sy_start; sy < sy_end; sy++) {
                    const uint8_t* src_row = src + sy * src_stride + sx_start * 3;
                    for (int sx = 0; sx < x_count; sx++) {
                        const uint8_t* p = src_row + sx * 3;
                        sum_r += p[0];
                        sum_g += p[1];
                        sum_b += p[2];
                    }
                }

                uint8_t* out = out_row + dx * 3;
                out[0] = (uint8_t)(sum_r / pixel_count);
                out[1] = (uint8_t)(sum_g / pixel_count);
                out[2] = (uint8_t)(sum_b / pixel_count);

            } else if (channels == 3) {
                uint64_t sum_r = 0, sum_g = 0, sum_b = 0;

                for (int sy = sy_start; sy < sy_end; sy++) {
                    const uint8_t* src_row = src + sy * src_stride + sx_start * 3;
                    for (int sx = 0; sx < x_count; sx++) {
                        const uint8_t* p = src_row + sx * 3;
                        sum_r += p[0];
                        sum_g += p[1];
                        sum_b += p[2];
                    }
                }

                uint8_t* out = out_row + dx * 3;
                out[0] = (uint8_t)(sum_r / pixel_count);
                out[1] = (uint8_t)(sum_g / pixel_count);
                out[2] = (uint8_t)(sum_b / pixel_count);

            } else {
                uint64_t sums[4] = {0, 0, 0, 0};

                for (int sy = sy_start; sy < sy_end; sy++) {
                    const uint8_t* src_row = src + sy * src_stride + sx_start * channels;
                    for (int sx = 0; sx < x_count; sx++) {
                        const uint8_t* p = src_row + sx * channels;
                        for (int c = 0; c < channels; c++) {
                            sums[c] += p[c];
                        }
                    }
                }

                uint8_t* out = out_row + dx * channels;
                for (int c = 0; c < channels; c++) {
                    out[c] = (uint8_t)(sums[c] / pixel_count);
                }
            }
        }
    }
}

#endif

bool simd_resize(
    const uint8_t* src, int src_w, int src_h, int channels,
    uint8_t* dst, int dst_w, int dst_h,
    ResizeQuality quality
) {
    if (!src || !dst || src_w <= 0 || src_h <= 0 ||
        dst_w <= 0 || dst_h <= 0 || channels < 1 || channels > 4) {
        return false;
    }

    if (quality == ResizeQuality::BEST) {
        return false;
    }

    float x_scale = (float)src_w / dst_w;
    float y_scale = (float)src_h / dst_h;
    float max_scale = std::max(x_scale, y_scale);

#ifdef USE_NEON
    if (max_scale > 3.0f && (channels == 3 || channels == 4)) {
        resize_area_neon(src, src_w, src_h, dst, dst_w, dst_h, channels);
        return true;
    }

    if (channels == 3 || channels == 4) {
        resize_bilinear_neon_rgba(src, src_w, src_h, dst, dst_w, dst_h, channels);
        return true;
    }
#endif

    return false;
}

}
}
