#pragma once

// Phase B Optimization #4: SIMD-Optimized Memory Operations
// Fast aligned memory copy using SIMD (AVX2 for x86_64, NEON for ARM)

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace fastresize {
namespace internal {

// ============================================
// SIMD-Optimized Memory Copy
// ============================================

inline void fast_copy_aligned(void* dst, const void* src, size_t size) {
#ifdef __AVX2__
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    size_t i = 0;

    // Process 32 bytes at a time with AVX2
    for (; i + 32 <= size; i += 32) {
        __m256i data = _mm256_loadu_si256((const __m256i*)(s + i));
        _mm256_storeu_si256((__m256i*)(d + i), data);
    }

    // Handle remaining bytes
    for (; i < size; i++) {
        d[i] = s[i];
    }
#elif defined(__ARM_NEON)
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    size_t i = 0;

    // Process 16 bytes at a time with NEON
    for (; i + 16 <= size; i += 16) {
        uint8x16_t data = vld1q_u8(s + i);
        vst1q_u8(d + i, data);
    }

    // Handle remaining bytes
    for (; i < size; i++) {
        d[i] = s[i];
    }
#else
    // Fallback to standard memcpy
    memcpy(dst, src, size);
#endif
}

// Fast pixel buffer copy (handles RGB/RGBA formats)
inline void fast_copy_pixels(unsigned char* dst, const unsigned char* src,
                             int width, int height, int channels) {
    size_t total_bytes = static_cast<size_t>(width) * height * channels;
    fast_copy_aligned(dst, src, total_bytes);
}

// ============================================
// SIMD-Optimized Memory Zero
// ============================================

inline void fast_zero(void* dst, size_t size) {
#ifdef __AVX2__
    uint8_t* d = (uint8_t*)dst;
    size_t i = 0;
    __m256i zero = _mm256_setzero_si256();

    for (; i + 32 <= size; i += 32) {
        _mm256_storeu_si256((__m256i*)(d + i), zero);
    }

    for (; i < size; i++) {
        d[i] = 0;
    }
#elif defined(__ARM_NEON)
    uint8_t* d = (uint8_t*)dst;
    size_t i = 0;
    uint8x16_t zero = vdupq_n_u8(0);

    for (; i + 16 <= size; i += 16) {
        vst1q_u8(d + i, zero);
    }

    for (; i < size; i++) {
        d[i] = 0;
    }
#else
    memset(dst, 0, size);
#endif
}

} // namespace internal
} // namespace fastresize
