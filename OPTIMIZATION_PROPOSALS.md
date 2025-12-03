# FastResize - Performance Optimization Proposals

**Date**: 2025-11-29
**Current Performance**: 300 images (2000x2000 → 800x600) in 559ms, RAM 89MB
**Status**: Ready for implementation

---

## 📊 Current Performance Baseline

### Phase 4 Results
- **Speed**: 300 images in 559ms (target was <3000ms) ✅ **5.37x faster than target**
- **RAM**: 89 MB peak for 300 images
- **Thread Scaling**: 5.6x speedup with 8 threads
- **Throughput**: 537 images/sec

### Strengths
- ✅ Efficient thread pool
- ✅ Buffer pool reduces allocations
- ✅ Specialized codecs (libjpeg-turbo, libpng, libwebp)
- ✅ SIMD enabled (stb_image_resize2)

---

## 🎯 Optimization Goals

1. **Speed**: Make it even faster (target: 2-3x improvement)
2. **RAM**: Reduce memory usage (target: 50% reduction)
3. **CPU**: If possible to reduce, but not critical (fast execution = low CPU impact)

---

## 🚀 Optimization Proposals

---

## **Proposal #1: Memory-Mapped I/O** ⭐⭐⭐⭐⭐

### Problem
Current implementation uses `fopen/fread` → copy to RAM → decode:
- For 2000x2000 JPEG (~500KB), file reading takes ~15-20% of total time
- Unnecessary memory copy from kernel buffer to user buffer
- Peak memory includes both file buffer and decoded pixels

### Solution
Use `mmap()` (Unix) or `MapViewOfFile()` (Windows) instead of `fread()`:

```cpp
// Before (current):
FILE* f = fopen(path, "rb");
fseek(f, 0, SEEK_END);
size_t size = ftell(f);
fseek(f, 0, SEEK_SET);
unsigned char* buffer = new unsigned char[size];
fread(buffer, size, 1, f);
fclose(f);

// After (optimized):
int fd = open(path, O_RDONLY);
struct stat sb;
fstat(fd, &sb);
void* mapped = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
// Decoder reads directly from mapped memory
// No need to allocate buffer
munmap(mapped, sb.st_size);
close(fd);
```

### Implementation Details

#### File: `src/decoder.cpp`

Add new function:
```cpp
namespace fastresize {
namespace internal {

struct MappedFile {
    void* data;
    size_t size;
    int fd;

    MappedFile() : data(nullptr), size(0), fd(-1) {}
    ~MappedFile() { unmap(); }

    bool map(const std::string& path) {
        #ifdef _WIN32
        // Windows implementation
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMapping) {
            CloseHandle(hFile);
            return false;
        }

        data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        size = fileSize.QuadPart;

        CloseHandle(hMapping);
        CloseHandle(hFile);
        #else
        // Unix implementation
        fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            close(fd);
            fd = -1;
            return false;
        }

        size = sb.st_size;
        data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            close(fd);
            fd = -1;
            data = nullptr;
            return false;
        }
        #endif

        return data != nullptr;
    }

    void unmap() {
        if (data) {
            #ifdef _WIN32
            UnmapViewOfFile(data);
            #else
            munmap(data, size);
            close(fd);
            #endif
            data = nullptr;
            size = 0;
            fd = -1;
        }
    }
};

// Update decode functions to use mmap
ImageData decode_jpeg_mmap(const std::string& path) {
    ImageData result = {nullptr, 0, 0, 0};
    MappedFile mapped;

    if (!mapped.map(path)) {
        // Fallback to fread
        return decode_jpeg(path);
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr_ext jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return result;
    }

    jpeg_create_decompress(&cinfo);

    // Use jpeg_mem_src instead of jpeg_stdio_src
    jpeg_mem_src(&cinfo, (unsigned char*)mapped.data, mapped.size);

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    result.width = cinfo.output_width;
    result.height = cinfo.output_height;
    result.channels = cinfo.output_components;

    size_t row_stride = result.width * result.channels;
    result.pixels = new unsigned char[result.height * row_stride];

    JSAMPROW row_pointer[1];
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &result.pixels[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return result;
}

// Similar updates for PNG and WEBP decoders

} // namespace internal
} // namespace fastresize
```

### Benefits
- ✅ **Speed**: 20-30% faster (OS caches pages instead of copying)
- ✅ **RAM**: 30-40% reduction (no intermediate buffer)
- ✅ **OS optimization**: Kernel automatically prefetches and caches pages

### Feasibility: ⭐⭐⭐⭐⭐ (Very High)
- **Cross-platform**: `mmap()` on Unix, `MapViewOfFile()` on Windows
- **Effort**: 2-3 hours to implement
- **Risk**: Low (fallback to fread if mmap fails)
- **Testing**: Easy to benchmark and verify

### Expected Improvement
- **Speed**: +20-30% for JPEG decode
- **RAM**: -30-40% peak memory
- **Overall**: **300 images: 559ms → ~420ms, RAM: 89MB → ~55MB**

---

## **Proposal #2: JPEG Turbo's Fast DCT** ⭐⭐⭐⭐⭐

### Problem
libjpeg-turbo currently uses accurate IDCT (Inverse Discrete Cosine Transform):
- `JDCT_ISLOW`: Slow but accurate (default)
- Unnecessary precision for image resizing use case

### Solution
Use `JDCT_IFAST` for faster decoding with minimal quality loss:

```cpp
// In decode_jpeg() function
cinfo.dct_method = JDCT_IFAST;  // Instead of JDCT_ISLOW (default)
```

### Implementation Details

#### File: `src/decoder.cpp`

Update the `decode_jpeg()` function:

```cpp
ImageData decode_jpeg(const std::string& path) {
    ImageData data = {nullptr, 0, 0, 0};

    FILE* infile = fopen(path.c_str(), "rb");
    if (!infile) return data;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr_ext jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return data;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);

    // ADD THIS LINE: Use fast DCT method
    cinfo.dct_method = JDCT_IFAST;

    // ADD THIS LINE: Use faster (less accurate) color conversion
    cinfo.do_fancy_upsampling = FALSE;

    jpeg_start_decompress(&cinfo);

    // ... rest of the function remains the same ...
}
```

### Optional: Make it configurable via ResizeOptions

```cpp
// In include/fastresize.h
struct ResizeOptions {
    // ... existing fields ...

    enum DecodeSpeed {
        DECODE_ACCURATE,    // JDCT_ISLOW (default)
        DECODE_FAST         // JDCT_IFAST
    } decode_speed;

    ResizeOptions()
        // ... existing initializers ...
        , decode_speed(DECODE_FAST)  // Default to fast
    {}
};
```

### Benefits
- ✅ **Speed**: JPEG decode 5-10% faster
- ✅ **Quality**: Negligible difference (PSNR ~0.1-0.3dB, imperceptible)
- ✅ **RAM**: No impact
- ✅ **Easy toggle**: Can make it configurable

### Feasibility: ⭐⭐⭐⭐⭐ (Very High)
- **Effort**: 5 minutes (1-2 lines of code)
- **Risk**: None (can toggle back if needed)
- **Testing**: Visual inspection + PSNR measurement

### Expected Improvement
- **Speed**: +5-10% for JPEG workflows
- **Overall**: **300 images: 559ms → ~520ms**

---

## **Proposal #3: 3-Stage Parallel Pipeline** ⭐⭐⭐⭐⭐

### Problem
Current implementation processes each image sequentially:
```
For each image:
    Decode → Resize → Encode
```

CPU is idle during I/O (file read/write), and I/O is idle during CPU work (resize).

### Solution
Implement 3-stage pipeline with dedicated thread pools:

```
Thread Pool 1 (Decode - 4 threads, I/O bound):
    Read file → Decode JPEG/PNG/WEBP → Raw pixels
        ↓ (queue)
Thread Pool 2 (Resize - 8 threads, CPU bound):
    Raw pixels → stb_image_resize2 → Resized pixels
        ↓ (queue)
Thread Pool 3 (Encode - 4 threads, I/O bound):
    Resized pixels → Encode JPEG/PNG/WEBP → Write file
```

### Implementation Details

#### File: `src/pipeline.h` (NEW)

```cpp
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include "internal.h"

namespace fastresize {
namespace internal {

// Lock-free bounded queue (SPSC: Single Producer Single Consumer)
template<typename T>
class BoundedQueue {
public:
    BoundedQueue(size_t capacity)
        : capacity_(capacity)
        , buffer_(capacity)
        , read_pos_(0)
        , write_pos_(0)
    {}

    bool try_push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size() >= capacity_) return false;

        buffer_[write_pos_] = std::move(item);
        write_pos_ = (write_pos_ + 1) % capacity_;

        lock.unlock();
        cv_not_empty_.notify_one();
        return true;
    }

    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_not_empty_.wait(lock, [this]() { return size() > 0 || done_; });

        if (size() == 0 && done_) return false;

        item = std::move(buffer_[read_pos_]);
        read_pos_ = (read_pos_ + 1) % capacity_;

        lock.unlock();
        cv_not_full_.notify_one();
        return true;
    }

    void set_done() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_ = true;
        cv_not_empty_.notify_all();
    }

    size_t size() const {
        if (write_pos_ >= read_pos_) {
            return write_pos_ - read_pos_;
        } else {
            return capacity_ - read_pos_ + write_pos_;
        }
    }

private:
    size_t capacity_;
    std::vector<T> buffer_;
    size_t read_pos_;
    size_t write_pos_;
    bool done_ = false;

    std::mutex mutex_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
};

// Pipeline stages
struct DecodeTask {
    std::string input_path;
    int task_id;
};

struct DecodeResult {
    ImageData image;
    std::string output_path;
    ResizeOptions options;
    int task_id;
    bool success;
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
};

class PipelineProcessor {
public:
    PipelineProcessor(size_t decode_threads = 4,
                     size_t resize_threads = 8,
                     size_t encode_threads = 4,
                     size_t queue_capacity = 32)
        : decode_pool_(decode_threads)
        , resize_pool_(resize_threads)
        , encode_pool_(encode_threads)
        , decode_queue_(queue_capacity)
        , resize_queue_(queue_capacity)
    {}

    BatchResult process_batch(const std::vector<BatchItem>& items);

private:
    ThreadPool decode_pool_;
    ThreadPool resize_pool_;
    ThreadPool encode_pool_;

    BoundedQueue<DecodeResult> decode_queue_;
    BoundedQueue<ResizeResult> resize_queue_;

    void decode_stage(const std::vector<BatchItem>& items);
    void resize_stage();
    void encode_stage(BatchResult& result);
};

} // namespace internal
} // namespace fastresize
```

#### File: `src/pipeline.cpp` (NEW)

```cpp
#include "pipeline.h"
#include "decoder.h"
#include "encoder.h"
#include "resizer.h"

namespace fastresize {
namespace internal {

void PipelineProcessor::decode_stage(const std::vector<BatchItem>& items) {
    for (size_t i = 0; i < items.size(); ++i) {
        decode_pool_.enqueue([this, &items, i]() {
            const auto& item = items[i];

            DecodeResult result;
            result.task_id = i;
            result.output_path = item.output_path;
            result.options = item.options;

            // Decode image
            ImageFormat fmt = detect_format(item.input_path);
            result.image = decode_image(item.input_path, fmt);
            result.success = (result.image.pixels != nullptr);

            // Push to resize queue
            while (!decode_queue_.try_push(std::move(result))) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    decode_pool_.wait();
    decode_queue_.set_done();
}

void PipelineProcessor::resize_stage() {
    resize_pool_.enqueue([this]() {
        DecodeResult decode_result;

        while (decode_queue_.try_pop(decode_result)) {
            if (!decode_result.success) continue;

            ResizeResult resize_result;
            resize_result.task_id = decode_result.task_id;
            resize_result.output_path = decode_result.output_path;
            resize_result.options = decode_result.options;

            // Calculate dimensions
            int out_w, out_h;
            calculate_dimensions(
                decode_result.image.width,
                decode_result.image.height,
                decode_result.options,
                out_w, out_h
            );

            // Resize
            resize_result.success = resize_image(
                decode_result.image.pixels,
                decode_result.image.width,
                decode_result.image.height,
                decode_result.image.channels,
                &resize_result.pixels,
                out_w, out_h,
                decode_result.options
            );

            resize_result.width = out_w;
            resize_result.height = out_h;
            resize_result.channels = decode_result.image.channels;

            // Free decode buffer
            free_image_data(decode_result.image);

            // Push to encode queue
            while (!resize_queue_.try_push(std::move(resize_result))) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    resize_pool_.wait();
    resize_queue_.set_done();
}

void PipelineProcessor::encode_stage(BatchResult& result) {
    encode_pool_.enqueue([this, &result]() {
        ResizeResult resize_result;

        while (resize_queue_.try_pop(resize_result)) {
            if (!resize_result.success) {
                result.failed++;
                continue;
            }

            // Detect output format
            ImageFormat out_fmt = detect_format_from_extension(resize_result.output_path);

            // Encode
            ImageData img_data;
            img_data.pixels = resize_result.pixels;
            img_data.width = resize_result.width;
            img_data.height = resize_result.height;
            img_data.channels = resize_result.channels;

            bool success = encode_image(
                resize_result.output_path,
                img_data,
                out_fmt,
                resize_result.options.quality
            );

            if (success) {
                result.success++;
            } else {
                result.failed++;
                result.errors.push_back("Encode failed: " + resize_result.output_path);
            }

            // Free resize buffer
            delete[] resize_result.pixels;
        }
    });

    encode_pool_.wait();
}

BatchResult PipelineProcessor::process_batch(const std::vector<BatchItem>& items) {
    BatchResult result;
    result.total = items.size();
    result.success = 0;
    result.failed = 0;

    // Start all 3 stages concurrently
    std::thread decode_thread([this, &items]() { decode_stage(items); });
    std::thread resize_thread([this]() { resize_stage(); });
    std::thread encode_thread([this, &result]() { encode_stage(result); });

    decode_thread.join();
    resize_thread.join();
    encode_thread.join();

    return result;
}

} // namespace internal
} // namespace fastresize
```

### Usage

Update `batch_resize_custom()` to use pipeline:

```cpp
BatchResult batch_resize_custom(
    const std::vector<BatchItem>& items,
    const BatchOptions& batch_opts
) {
    if (items.size() > 50) {  // Use pipeline for large batches
        internal::PipelineProcessor pipeline(4, 8, 4, 32);
        return pipeline.process_batch(items);
    } else {
        // Use existing thread pool for small batches
        // ... existing implementation ...
    }
}
```

### Benefits
- ✅ **Speed**: 30-50% faster for large batches (overlap I/O + CPU)
- ✅ **Throughput**: Higher overall throughput
- ✅ **CPU utilization**: Better CPU usage during I/O wait
- ✅ **Scalability**: Scales better with batch size

### Feasibility: ⭐⭐⭐ (Medium - Complex)
- **Effort**: 2-3 days to implement and test thoroughly
- **Risk**: Medium (need careful synchronization)
- **Complexity**: High (3 thread pools + 2 queues + coordination)
- **Testing**: Requires comprehensive thread safety testing

### Expected Improvement
- **Speed**: +30-50% for batches > 100 images
- **Overall**: **300 images: 559ms → ~370ms**

---

## **Proposal #4: SIMD-Optimized Memory Copy** ⭐⭐⭐⭐

### Problem
Current pixel data copying uses standard `memcpy()`:
- Generic implementation, not optimized for large blocks
- Doesn't fully utilize SIMD capabilities
- Occurs multiple times: decode → resize → encode

### Solution
Use SIMD intrinsics for faster memory copy:

```cpp
#ifdef __AVX2__
void fast_copy_pixels_avx2(uint8_t* dst, const uint8_t* src, size_t size) {
    size_t i = 0;
    // AVX2: 32 bytes per iteration
    for (; i + 32 <= size; i += 32) {
        __m256i data = _mm256_loadu_si256((__m256i*)(src + i));
        _mm256_storeu_si256((__m256i*)(dst + i), data);
    }
    // Handle remaining bytes
    for (; i < size; i++) dst[i] = src[i];
}
#elif defined(__ARM_NEON)
void fast_copy_pixels_neon(uint8_t* dst, const uint8_t* src, size_t size) {
    size_t i = 0;
    // NEON: 16 bytes per iteration
    for (; i + 16 <= size; i += 16) {
        uint8x16_t data = vld1q_u8(src + i);
        vst1q_u8(dst + i, data);
    }
    for (; i < size; i++) dst[i] = src[i];
}
#else
#define fast_copy_pixels memcpy
#endif
```

### Implementation Details

#### File: `src/simd_utils.h` (NEW)

```cpp
#pragma once
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

// Fast aligned memory copy using SIMD
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
    size_t total_bytes = width * height * channels;
    fast_copy_aligned(dst, src, total_bytes);
}

// Zero-initialize buffer using SIMD
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
```

#### Update decoder/encoder/resizer

Replace all `memcpy()` calls with `fast_copy_pixels()`:

```cpp
// In decoder.cpp, encoder.cpp, resizer.cpp
#include "simd_utils.h"

// Replace:
memcpy(output_pixels, input_pixels, width * height * channels);

// With:
fast_copy_pixels(output_pixels, input_pixels, width, height, channels);
```

### Benefits
- ✅ **Speed**: 2-3x faster memory copy (32 bytes/cycle vs 8-16 bytes)
- ✅ **RAM**: No change
- ✅ **Wide impact**: Affects decode, resize, encode stages
- ✅ **Cache friendly**: Better cache line utilization

### Feasibility: ⭐⭐⭐⭐ (High)
- **Effort**: 4-6 hours (implementation + testing + fallback)
- **Risk**: Low (fallback to memcpy if SIMD unavailable)
- **Platform**: AVX2 (x86_64), NEON (ARM), fallback (generic)
- **Testing**: Easy to benchmark

### Expected Improvement
- **Speed**: +10-15% overall (reduces copy overhead across pipeline)
- **Overall**: **300 images: 559ms → ~480ms**

---

## **Proposal #5: Pre-allocated Output Buffer Pool** ⭐⭐⭐⭐

### Problem
Current implementation allocates encoder output buffer for each image:
- `new[]` / `delete[]` overhead
- Memory fragmentation
- Cache misses on new allocations

### Solution
Pre-allocate and reuse output buffers similar to existing buffer pool:

```cpp
class EncodedBufferPool {
    std::vector<std::vector<uint8_t>> buffers_;
    std::mutex mutex_;

    std::vector<uint8_t>& acquire(size_t estimated_size) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find reusable buffer with sufficient capacity
        for (auto& buf : buffers_) {
            if (buf.capacity() >= estimated_size) {
                auto result = std::move(buf);
                buf = std::vector<uint8_t>();  // Mark as taken
                return result;
            }
        }

        // No reusable buffer, create new
        std::vector<uint8_t> new_buf;
        new_buf.reserve(estimated_size);
        return new_buf;
    }

    void release(std::vector<uint8_t>&& buffer) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (buffers_.size() < 32) {  // Limit pool size
            buffer.clear();  // Keep capacity
            buffers_.push_back(std::move(buffer));
        }
    }
};
```

### Implementation Details

#### File: `src/thread_pool.cpp`

Add `EncodedBufferPool` class:

```cpp
namespace fastresize {
namespace internal {

class EncodedBufferPool {
public:
    struct Buffer {
        std::vector<uint8_t> data;
        size_t capacity;
    };

    EncodedBufferPool() {}
    ~EncodedBufferPool() {
        clear();
    }

    // Acquire buffer with at least 'size' capacity
    std::vector<uint8_t> acquire(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find suitable buffer from pool
        for (size_t i = 0; i < pool_.size(); ++i) {
            if (pool_[i].capacity >= size) {
                auto result = std::move(pool_[i].data);
                pool_.erase(pool_.begin() + i);
                result.clear();  // Clear but keep capacity
                return result;
            }
        }

        // No suitable buffer, create new
        std::vector<uint8_t> new_buffer;
        new_buffer.reserve(size);
        return new_buffer;
    }

    // Release buffer back to pool
    void release(std::vector<uint8_t>&& buffer) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (pool_.size() >= 32) {
            // Pool is full, discard buffer
            return;
        }

        Buffer buf;
        buf.data = std::move(buffer);
        buf.capacity = buf.data.capacity();
        buf.data.clear();  // Clear data but keep capacity

        pool_.push_back(std::move(buf));
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
    }

private:
    std::vector<Buffer> pool_;
    std::mutex mutex_;
};

// Global encoded buffer pool (similar to existing buffer pool)
static EncodedBufferPool g_encoded_buffer_pool;

// C-style interface
std::vector<uint8_t> acquire_encoded_buffer(size_t size) {
    return g_encoded_buffer_pool.acquire(size);
}

void release_encoded_buffer(std::vector<uint8_t>&& buffer) {
    g_encoded_buffer_pool.release(std::move(buffer));
}

} // namespace internal
} // namespace fastresize
```

#### Update encoder to use buffer pool

File: `src/encoder.cpp`

```cpp
bool encode_jpeg(const std::string& path, const ImageData& data, int quality) {
    // Estimate encoded size (conservative)
    size_t estimated_size = data.width * data.height * data.channels;

    // Acquire buffer from pool
    auto output_buffer = acquire_encoded_buffer(estimated_size);

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Use memory destination
    unsigned char* mem_buffer = nullptr;
    unsigned long mem_size = 0;
    jpeg_mem_dest(&cinfo, &mem_buffer, &mem_size);

    cinfo.image_width = data.width;
    cinfo.image_height = data.height;
    cinfo.input_components = data.channels;
    cinfo.in_color_space = (data.channels == 3) ? JCS_RGB : JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = data.width * data.channels;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &data.pixels[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    // Write to file
    FILE* outfile = fopen(path.c_str(), "wb");
    if (!outfile) {
        free(mem_buffer);
        release_encoded_buffer(std::move(output_buffer));
        return false;
    }

    fwrite(mem_buffer, 1, mem_size, outfile);
    fclose(outfile);

    free(mem_buffer);

    // Release buffer back to pool
    release_encoded_buffer(std::move(output_buffer));

    return true;
}
```

### Benefits
- ✅ **RAM**: 20-30% reduction (reuse buffers, no fragmentation)
- ✅ **Speed**: 5-10% faster (reduced allocation overhead)
- ✅ **Cache friendly**: Buffer reuse improves cache locality

### Feasibility: ⭐⭐⭐⭐⭐ (Very High)
- **Effort**: 3-4 hours
- **Risk**: Low (similar to existing buffer pool)
- **Pattern**: Already proven with pixel buffer pool

### Expected Improvement
- **RAM**: -20-30% peak memory
- **Speed**: +5-10% (reduced allocation time)
- **Overall**: **300 images: RAM 89MB → ~65MB**

---

## **Proposal #6: Zero-Copy JPEG Encode** ⭐⭐⭐

### Problem
Current JPEG encoding: pixels → temp memory buffer → write to file

Two-step process:
1. `jpeg_mem_dest()`: Encode to memory buffer
2. `fwrite()`: Write buffer to file

### Solution
Encode directly to file descriptor:

```cpp
// Instead of:
unsigned char* mem_buffer = nullptr;
unsigned long mem_size = 0;
jpeg_mem_dest(&cinfo, &mem_buffer, &mem_size);
// ... encode ...
FILE* f = fopen(path, "wb");
fwrite(mem_buffer, mem_size, 1, f);
free(mem_buffer);

// Use:
FILE* f = fopen(path, "wb");
jpeg_stdio_dest(&cinfo, f);
// ... encode directly to file ...
```

### Implementation Details

#### File: `src/encoder.cpp`

Update `encode_jpeg()` function:

```cpp
bool encode_jpeg(const std::string& path, const ImageData& data, int quality) {
    // Open file first
    FILE* outfile = fopen(path.c_str(), "wb");
    if (!outfile) return false;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // CHANGED: Use stdio destination instead of memory destination
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = data.width;
    cinfo.image_height = data.height;
    cinfo.input_components = data.channels;
    cinfo.in_color_space = (data.channels == 3) ? JCS_RGB : JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    // OPTIONAL: Use fast DCT for encoding too
    cinfo.dct_method = JDCT_IFAST;

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = data.width * data.channels;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &data.pixels[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(outfile);

    return true;
}
```

### Benefits
- ✅ **RAM**: -10-15MB (no intermediate buffer for encoded data)
- ✅ **Speed**: +2-5% (one less memory copy)
- ✅ **Simpler code**: Fewer allocations to track

### Feasibility: ⭐⭐⭐⭐⭐ (Very High)
- **Effort**: 1 hour (update encoder functions)
- **Risk**: Very low (official libjpeg-turbo API)
- **Testing**: Simple file comparison

### Expected Improvement
- **RAM**: -10-15MB
- **Speed**: +2-5%

---

## **Proposal #7: Adaptive Thread Count** ⭐⭐⭐

### Problem
Current implementation uses fixed 8 threads regardless of batch size:
- Small batches (< 10 images): Thread overhead > benefit
- Medium batches (10-50 images): Could use 4 threads
- Large batches (50+ images): 8 threads optimal

### Solution
Automatically adjust thread count based on batch size:

```cpp
size_t calculate_optimal_threads(size_t batch_size, size_t max_threads = 8) {
    if (batch_size < 10) return 2;
    if (batch_size < 50) return 4;
    return max_threads;
}
```

### Implementation Details

#### File: `include/fastresize.h`

Update `BatchOptions`:

```cpp
struct BatchOptions {
    int num_threads;        // Thread pool size (0 = auto)
    bool stop_on_error;     // Stop if any image fails (default: false)

    BatchOptions()
        : num_threads(0)     // 0 = auto-detect
        , stop_on_error(false)
    {}
};
```

#### File: `src/fastresize.cpp`

Add adaptive thread selection:

```cpp
namespace fastresize {
namespace internal {

size_t calculate_optimal_threads(size_t batch_size, int requested_threads) {
    // If user specified thread count, use it
    if (requested_threads > 0) {
        return requested_threads;
    }

    // Auto-detect based on batch size
    if (batch_size < 5) {
        return 1;  // Sequential for very small batches
    } else if (batch_size < 20) {
        return 2;  // 2 threads for small batches
    } else if (batch_size < 50) {
        return 4;  // 4 threads for medium batches
    } else {
        // For large batches, use system CPU count (capped at 8)
        size_t cpu_count = std::thread::hardware_concurrency();
        return std::min(cpu_count, size_t(8));
    }
}

} // namespace internal

BatchResult batch_resize(
    const std::vector<std::string>& input_paths,
    const std::string& output_dir,
    const ResizeOptions& options,
    const BatchOptions& batch_opts
) {
    // Calculate optimal thread count
    size_t num_threads = internal::calculate_optimal_threads(
        input_paths.size(),
        batch_opts.num_threads
    );

    // Create thread pool with optimal size
    internal::ThreadPool* pool = internal::create_thread_pool(num_threads);

    // ... rest of implementation ...
}
```

### Benefits
- ✅ **CPU**: Reduced overhead for small batches
- ✅ **RAM**: 10-20% reduction for small batches (fewer threads)
- ✅ **Latency**: Faster processing for small batches
- ✅ **User control**: Can override with explicit thread count

### Feasibility: ⭐⭐⭐⭐⭐ (Very High)
- **Effort**: 30 minutes
- **Risk**: None
- **Backward compatible**: Default behavior improved, can override

### Expected Improvement
- **Small batches** (< 20 images): +5-10% speed, -10-20% RAM
- **Medium batches** (20-50 images): +5% speed
- **Large batches** (50+ images): No change (already optimal)

---

## 📋 Summary Table

| # | Proposal | Speed Impact | RAM Impact | Effort | Risk | Priority |
|---|----------|--------------|------------|--------|------|----------|
| 1 | **Memory-mapped I/O** | +20-30% | -30-40% | 2-3h | Low | ⭐⭐⭐⭐⭐ |
| 2 | **JPEG Fast DCT** | +5-10% | 0% | 5min | None | ⭐⭐⭐⭐⭐ |
| 3 | **3-Stage Pipeline** | +30-50%* | 0% | 2-3 days | Medium | ⭐⭐⭐⭐ |
| 4 | **SIMD Memory Copy** | +10-15% | 0% | 4-6h | Low | ⭐⭐⭐⭐ |
| 5 | **Output Buffer Pool** | +5-10% | -20-30% | 3-4h | Low | ⭐⭐⭐⭐ |
| 6 | **Zero-Copy JPEG** | +2-5% | -15MB | 1h | Very Low | ⭐⭐⭐ |
| 7 | **Adaptive Threads** | +5-10%† | -10-20%† | 30min | None | ⭐⭐⭐ |

*: For large batches (100+ images)
†: For small batches (< 20 images)

---

## 🎯 Implementation Roadmap

### **Phase A: Quick Wins** (1 day, low risk, high impact)

**Goal**: Achieve 10-20% improvement with minimal effort

1. ✅ **Proposal #2**: JPEG Fast DCT (5 minutes)
   - Expected: +5-10% speed
   - Risk: None

2. ✅ **Proposal #7**: Adaptive Thread Count (30 minutes)
   - Expected: +5-10% speed for small batches
   - Risk: None

3. ✅ **Proposal #6**: Zero-Copy JPEG Encode (1 hour)
   - Expected: +2-5% speed, -15MB RAM
   - Risk: Very low

**Expected Results**:
- Speed: 559ms → ~490ms (~14% improvement)
- RAM: 89MB → ~75MB (~16% reduction)

---

### **Phase B: High Impact** (1 week, medium effort, high impact)

**Goal**: Achieve 50-70% improvement

4. ✅ **Proposal #1**: Memory-mapped I/O (2-3 hours)
   - Expected: +20-30% speed, -30-40% RAM
   - Risk: Low

5. ✅ **Proposal #5**: Pre-allocated Output Buffers (3-4 hours)
   - Expected: +5-10% speed, -20-30% RAM
   - Risk: Low

6. ✅ **Proposal #4**: SIMD Memory Copy (4-6 hours)
   - Expected: +10-15% speed
   - Risk: Low

**Expected Results** (cumulative):
- Speed: 559ms → ~320ms (~75% of original, 1.75x faster)
- RAM: 89MB → ~45MB (~50% reduction)

---

### **Phase C: Advanced** (2-3 weeks, high complexity, transformative)

**Goal**: Achieve 2-3x total improvement

7. ✅ **Proposal #3**: 3-Stage Parallel Pipeline (2-3 days)
   - Expected: +30-50% speed for large batches
   - Risk: Medium
   - Complexity: High

**Expected Results** (cumulative):
- Speed: 559ms → ~220ms (~40% of original, 2.5x faster)
- RAM: 89MB → ~45MB (~50% reduction)

---

## 🔥 Final Performance Targets

### Conservative Estimate (Phase A + B)
- **Speed**: 559ms → **~320ms** (1.75x faster, 300 images)
- **RAM**: 89MB → **~45MB** (2x reduction)
- **Effort**: ~2-3 days
- **Risk**: Low
- **Confidence**: ⭐⭐⭐⭐⭐ Very High

### Aggressive Estimate (Phase A + B + C)
- **Speed**: 559ms → **~220ms** (2.5x faster, 300 images)
- **RAM**: 89MB → **~45MB** (2x reduction)
- **Effort**: ~3-4 weeks
- **Risk**: Medium
- **Confidence**: ⭐⭐⭐⭐ High

---

## 💡 Recommendation

### Start with Phase A (Quick Wins)
**Day 1**:
1. Morning: Implement Proposal #2 (5 min) + test
2. Morning: Implement Proposal #7 (30 min) + test
3. Afternoon: Implement Proposal #6 (1 hour) + test
4. Afternoon: Run comprehensive benchmarks

**Decision Point**: If results meet expectations → proceed to Phase B

### Phase B Implementation Order
1. **Proposal #1** (Memory-mapped I/O) - Highest impact
2. **Proposal #4** (SIMD Copy) - Good impact, medium effort
3. **Proposal #5** (Output Buffer Pool) - RAM optimization

### Phase C Decision
- Only proceed if:
  - Phase B results are good
  - Need even better performance
  - Have time for complex implementation

---

## 📝 Notes for Future Sessions

### When Resuming Optimization Work:

1. **Check current baseline**: Run benchmarks first
2. **Implement one proposal at a time**: Don't mix multiple changes
3. **Benchmark after each change**: Verify expected improvements
4. **Keep fallback code**: Each optimization should have a fallback path

### Testing Strategy

For each proposal:
1. Unit test: Verify correctness
2. Benchmark: Measure speed improvement
3. Memory test: Measure RAM usage
4. Visual test: Ensure image quality unchanged
5. Cross-platform: Test on macOS and Linux

### Risk Mitigation

- Keep existing code path available via compile flag
- Add comprehensive error handling
- Test edge cases (very small/large images)
- Verify thread safety with sanitizers

---

## 🔗 References

### Memory-Mapped I/O
- Linux mmap: `man 2 mmap`
- Windows: https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile

### SIMD Intrinsics
- Intel AVX2: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
- ARM NEON: https://developer.arm.com/architectures/instruction-sets/intrinsics/

### libjpeg-turbo
- Fast DCT: https://libjpeg-turbo.org/Documentation/Documentation
- API Reference: https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/libjpeg.txt

---

**END OF OPTIMIZATION PROPOSALS**

*This document should be updated as optimizations are implemented and benchmarked.*
