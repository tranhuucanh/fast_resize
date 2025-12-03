# FastResize - Phase E: Performance Optimization
## Mục tiêu: Đánh bại libvips trong batch processing

---

## 📊 HIỆN TRẠNG

### Vấn đề
- **libvips batch nhanh hơn FastResize** trong benchmark
- FastResize chỉ làm resize nhưng chậm hơn libvips (thư viện to, đa năng)
- Điều này KHÔNG THỂ CHẤP NHẬN được

### Benchmark hiện tại
```bash
cd /Users/canh.th/Desktop/fastgems/fastresize
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark/bench_vs_libvips
```

### Root Cause Analysis

**Bottleneck chính đã phát hiện:**

1. **PNG Encoding - Global Mutex Bottleneck** 🔴 CRITICAL
   - File: `src/encoder.cpp:133`
   - Vấn đề: `std::lock_guard<std::mutex> lock(png_encode_mutex);`
   - Impact: PNG encoding hoàn toàn SERIAL, không thể parallel
   - Mất mát: ~60% performance cho PNG batch

2. **JPEG Encoding - Suboptimal** 🔴 HIGH
   - File: `src/encoder.cpp:109-112`
   - Vấn đề: Encode từng scanline một, không tận dụng libjpeg-turbo tối đa
   - Impact: JPEG encoding chậm hơn 20-30% so với potential

3. **Memory Allocation trong Hot Path** 🟠 MEDIUM
   - File: `src/encoder.cpp:76-90`
   - Vấn đề: Mỗi lần RGBA→RGB allocate buffer mới
   - Impact: 15-20% overhead cho images có alpha channel

4. **Pipeline Threshold quá cao** 🟠 MEDIUM
   - File: `src/fastresize.cpp:270`
   - Vấn đề: Pipeline chỉ activate với batch ≥50
   - Impact: Batch nhỏ không được hưởng lợi từ pipeline architecture

---

## 🎯 KẾ HOẠCH TỐI ƯU (3 PHASES)

### Tổng quan chiến lược
```
Phase E1: Quick Wins        → Expected: 50-70% faster  (2-3 days)
Phase E2: Deep Optimization → Expected: +40-50% faster (3-5 days)
Phase E3: Advanced Tuning   → Expected: +10-20% faster (1 week)

Tổng cộng: 2-3x nhanh hơn hiện tại
```

---

## 📋 PHASE E1: QUICK WINS (High Impact, Low Risk)

### Mục tiêu
- **50-70% faster** so với hiện tại
- **100% backward compatible**
- Không thay đổi API, output giống hệt

### E1.1: Remove PNG Global Mutex (Impact: 60% faster PNG)

**Files cần sửa:**
- `src/encoder.cpp:130-200`

**Hiện tại:**
```cpp
bool encode_png(...) {
    std::lock_guard<std::mutex> lock(png_encode_mutex);  // ← BOTTLENECK
    // ... encode code ...
}
```

**Vấn đề:**
- Global mutex serialize TẤT CẢ PNG encoding
- Chỉ 1 thread encode PNG tại 1 thời điểm
- Các thread khác phải chờ → waste CPU cores

**Giải pháp:**
```cpp
bool encode_png(...) {
    // NO MUTEX - libpng 1.6+ is thread-safe per png_struct
    // Each thread creates its own png_struct → safe parallel encoding

    png_structp png = png_create_write_struct(...);  // Thread-local
    png_infop info = png_create_info_struct(png);
    // ... rest of code unchanged ...
}
```

**Verification cần làm:**
1. Check libpng version: `pkg-config --modversion libpng` (cần ≥ 1.6.0)
2. Nếu < 1.6.0: thêm per-thread mutex thay vì global mutex

**Testing:**
```bash
# Run PNG batch test
ruby bindings/ruby/test_all_features.rb
# Phải PASS 100% - output files giống hệt

# Benchmark PNG performance
cd build/benchmark
./bench_formats  # So sánh PNG speed
```

**Expected result:**
- PNG batch: 40-60% faster
- Other formats: không ảnh hưởng

---

### E1.2: Buffer Pool cho RGBA→RGB Conversion (Impact: 20% faster)

**Files cần sửa:**
- `src/encoder.cpp:34-124` (JPEG encoding)
- `src/internal.h` (buffer pool interface)

**Hiện tại:**
```cpp
// encoder.cpp:76-90
if (data.channels == 4) {
    // Allocate NEW buffer mỗi lần
    rgb_buffer = new unsigned char[rgb_size];  // ← SLOW

    // Convert RGBA → RGB
    for (int i = 0; i < data.width * data.height; i++) {
        *dst++ = *src++;  // R
        *dst++ = *src++;  // G
        *dst++ = *src++;  // B
        src++;            // Skip A
    }

    delete[] rgb_buffer;  // ← SLOW
}
```

**Vấn đề:**
- Mỗi image allocate/deallocate buffer mới
- Với batch 500 images → 500 lần allocate/free
- Memory allocator overhead đáng kể

**Giải pháp:**
```cpp
// Thêm vào ThreadPool - mỗi thread có buffer pool riêng
struct ThreadLocalBuffers {
    std::vector<unsigned char*> rgb_buffers;
    size_t buffer_capacity;

    unsigned char* get_buffer(size_t size) {
        if (!rgb_buffers.empty() && buffer_capacity >= size) {
            auto buf = rgb_buffers.back();
            rgb_buffers.pop_back();
            return buf;
        }
        return new unsigned char[size];
    }

    void return_buffer(unsigned char* buf) {
        if (rgb_buffers.size() < 4) {  // Keep max 4 buffers per thread
            rgb_buffers.push_back(buf);
        } else {
            delete[] buf;
        }
    }
};

// encoder.cpp - sử dụng buffer pool
if (data.channels == 4) {
    rgb_buffer = thread_local_buffers->get_buffer(rgb_size);
    // ... convert ...
    // Trả lại thay vì delete
    thread_local_buffers->return_buffer(rgb_buffer);
}
```

**Testing:**
```bash
# Test với PNG → JPG conversion (có alpha channel)
ruby bindings/ruby/test_all_features.rb

# Benchmark RGBA images
cd build/benchmark
./bench_formats  # Test với PNG input → JPG output
```

**Expected result:**
- Images với alpha channel: 15-20% faster
- RGB images: không ảnh hưởng (không dùng buffer pool)

---

### E1.3: Lower Pipeline Threshold (Impact: 25% faster cho batch nhỏ)

**Files cần sửa:**
- `src/fastresize.cpp:270`
- `src/fastresize.cpp:369`

**Hiện tại:**
```cpp
// fastresize.cpp:270
if (batch_opts.max_speed && input_paths.size() >= 50) {
    // Use 3-stage pipeline
}
```

**Vấn đề:**
- Pipeline chỉ cho batch ≥50
- Batch 10-49 dùng thread pool cũ (chậm hơn)
- Pipeline architecture tốt hơn nhưng không được dùng

**Giải pháp:**
```cpp
// Conservative threshold: 20 images
// Với batch >= 20, pipeline đủ hiệu quả
if (batch_opts.max_speed && input_paths.size() >= 20) {
    // Use 3-stage pipeline
    // Queue capacity (32) đủ cho batch 20-50
}
```

**Rationale:**
- Queue capacity = 32 (pipeline.h:125)
- Với batch 20: queue util ~60% (ok)
- Với batch < 20: overhead > benefit (skip pipeline)

**Testing:**
```bash
# Test batch sizes: 10, 20, 30, 50
ruby bindings/ruby/benchmark_500.rb  # Có batch test

# Benchmark khác nhau
cd build/benchmark
./bench_pipeline  # Test pipeline với batch sizes khác nhau
```

**Expected result:**
- Batch 20-49: 20-30% faster
- Batch < 20: không ảnh hưởng
- Batch ≥ 50: không ảnh hưởng

---

### E1 Summary

**Changes:**
1. Remove PNG global mutex → thread-safe parallel encoding
2. Buffer pool cho RGBA→RGB → reuse memory
3. Pipeline threshold 50→20 → more batches use pipeline

**Expected improvements:**
- PNG batch: 40-60% faster
- JPEG batch (RGBA input): 15-20% faster
- Small batch (20-49): 20-30% faster
- **Overall: 50-70% faster**

**Risk level:** 🟢 LOW
- Không thay đổi output
- Backward compatible 100%
- Có test coverage

**Validation:**
```bash
# 1. Run all tests
ruby bindings/ruby/test_all_features.rb
# Phải: ALL TESTS PASSED

# 2. Run benchmark
./build/benchmark/bench_vs_libvips
# Expected: FastResize gần bằng hoặc nhanh hơn libvips parallel

# 3. Visual check
# So sánh output images trước/sau optimization
# Phải: BIT-IDENTICAL (hoặc PSNR > 50dB)
```

---

## 📋 PHASE E2: DEEP OPTIMIZATION (Medium Impact, Medium Risk)

### Mục tiêu
- **+40-50% faster** so với Phase E1
- Cần testing kỹ hơn
- Có thể thay đổi internal implementation

### E2.1: SIMD cho RGBA→RGB Conversion (Impact: 15% faster)

**Files cần sửa:**
- Tạo mới: `src/simd_convert.h`, `src/simd_convert.cpp`
- Sửa: `src/encoder.cpp:81-88`

**Hiện tại:**
```cpp
// Scalar code - process 1 pixel mỗi lần
for (int i = 0; i < data.width * data.height; i++) {
    *dst++ = *src++;  // R
    *dst++ = *src++;  // G
    *dst++ = *src++;  // B
    src++;            // Skip A
}
```

**Vấn đề:**
- Chỉ process 1 pixel/iteration
- CPU modern có thể process 16-32 pixels cùng lúc (SIMD)
- Apple Silicon (NEON): 16 bytes/instruction
- x86 AVX2: 32 bytes/instruction

**Giải pháp:**
```cpp
// NEON (Apple Silicon) - process 16 pixels at once
#ifdef __ARM_NEON
    size_t num_pixels = data.width * data.height;
    size_t simd_pixels = num_pixels - (num_pixels % 16);

    for (size_t i = 0; i < simd_pixels; i += 16) {
        // Load 16 RGBA pixels (64 bytes)
        uint8x16x4_t rgba = vld4q_u8(src);

        // Store RGB only (48 bytes) - drop A
        uint8x16x3_t rgb;
        rgb.val[0] = rgba.val[0];  // R
        rgb.val[1] = rgba.val[1];  // G
        rgb.val[2] = rgba.val[2];  // B
        vst3q_u8(dst, rgb);

        src += 64;
        dst += 48;
    }

    // Handle remaining pixels (< 16) with scalar code
    for (size_t i = simd_pixels; i < num_pixels; i++) {
        *dst++ = *src++;  // R
        *dst++ = *src++;  // G
        *dst++ = *src++;  // B
        src++;            // Skip A
    }
#else
    // Fallback: scalar code (current implementation)
#endif
```

**Platform support:**
- ✅ Apple Silicon (M1/M2/M3): NEON built-in
- ✅ x86_64: AVX2 (Intel 2013+, AMD 2015+)
- ✅ ARM Linux: NEON (Raspberry Pi 3+)
- ✅ Fallback: scalar code cho old CPUs

**Testing:**
```bash
# 1. Correctness test
ruby bindings/ruby/test_all_features.rb
# Test với PNG (RGBA) → JPG conversion

# 2. Performance test
cd build/benchmark
./bench_formats  # RGBA images

# 3. Platform test
# macOS (NEON): should be faster
# Linux x86: AVX2 detection
```

**Expected result:**
- RGBA→RGB: 10-15% faster
- RGB images: không ảnh hưởng (không conversion)

---

### E2.2: Optimize JPEG Encoding với libjpeg-turbo (Impact: 25% faster)

**Files cần sửa:**
- `src/encoder.cpp:34-124`

**Hiện tại:**
```cpp
while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &encode_pixels[cinfo.next_scanline * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);  // 1 dòng/lần
}
```

**Vấn đề:**
- Write 1 scanline mỗi lần → nhiều function calls
- libjpeg-turbo có thể write nhiều scanlines cùng lúc
- Không tận dụng hết libjpeg-turbo optimization

**Giải pháp 1: Batch scanlines**
```cpp
// Write 16 scanlines mỗi lần
const int SCANLINES_PER_BATCH = 16;
JSAMPROW row_pointers[SCANLINES_PER_BATCH];

while (cinfo.next_scanline < cinfo.image_height) {
    int rows_left = cinfo.image_height - cinfo.next_scanline;
    int rows_to_write = std::min(rows_left, SCANLINES_PER_BATCH);

    for (int i = 0; i < rows_to_write; i++) {
        row_pointers[i] = &encode_pixels[(cinfo.next_scanline + i) * row_stride];
    }

    jpeg_write_scanlines(&cinfo, row_pointers, rows_to_write);
}
```

**Giải pháp 2: libjpeg-turbo specific optimizations**
```cpp
// Enable libjpeg-turbo specific features
#ifdef LIBJPEG_TURBO_VERSION
    // Use SIMD-optimized DCT
    cinfo.dct_method = JDCT_IFAST;  // Already done in Phase A

    // Optimize for batch encoding
    cinfo.optimize_coding = FALSE;  // Faster, slightly larger files

    // Disable smoothing (faster, minimal quality impact)
    cinfo.smoothing_factor = 0;
#endif
```

**Testing:**
```bash
# Quality test - ensure output quality ok
cd bindings/ruby
ruby test_all_features.rb

# Performance test
cd build/benchmark
./bench_formats  # JPEG encoding speed

# Quality comparison (PSNR should be > 40dB)
# Compare before/after với ImageMagick
compare -metric PSNR before.jpg after.jpg diff.png
```

**Expected result:**
- JPEG encoding: 20-30% faster
- Quality: PSNR > 40dB (acceptable)
- File size: +2-5% larger (acceptable trade-off)

---

### E2.3: Better Thread Pool Work Distribution (Impact: 10% faster)

**Files cần sửa:**
- `src/thread_pool.cpp`
- `src/fastresize.cpp:304-343`

**Hiện tại:**
```cpp
// Simple work queue - FIFO
for (const std::string& input_path : input_paths) {
    thread_pool_enqueue(pool, [&, input_path]() {
        // Process image
    });
}
```

**Vấn đề:**
- Tất cả images được treat như nhau
- Large images (5MB) và small images (500KB) cùng queue
- Thread process small image xong phải chờ thread khác (large image)
- Load imbalance → waste CPU time

**Giải pháp: Work stealing với size-based priority**
```cpp
// Sort images by size (largest first)
struct ImageTask {
    std::string path;
    size_t file_size;
};

std::vector<ImageTask> tasks;
for (const auto& path : input_paths) {
    struct stat st;
    stat(path.c_str(), &st);
    tasks.push_back({path, st.st_size});
}

// Sort by size DESC - process large images first
std::sort(tasks.begin(), tasks.end(),
    [](const ImageTask& a, const ImageTask& b) {
        return a.file_size > b.file_size;
    });

// Enqueue sorted tasks
for (const auto& task : tasks) {
    thread_pool_enqueue(pool, [&, task]() {
        // Process image
    });
}
```

**Rationale:**
- Large images first → better load balancing
- Khi large images xong, threads cùng finish small images
- Giảm tail latency (thời gian thread cuối cùng)

**Testing:**
```bash
# Test với mixed image sizes
cd bindings/ruby
ruby benchmark_500.rb  # Mixed sizes

# Check CPU utilization
# Trước: uneven (some threads idle)
# Sau: even (all threads busy)
```

**Expected result:**
- Better CPU utilization
- 5-15% faster (depending on image size variance)

---

### E2 Summary

**Changes:**
1. SIMD RGBA→RGB → 16-32 pixels/instruction
2. Optimize JPEG encoding → batch scanlines
3. Better work distribution → large images first

**Expected improvements:**
- RGBA conversion: 10-15% faster
- JPEG encoding: 20-30% faster
- Load balancing: 5-15% faster
- **Overall: +40-50% faster than E1**

**Risk level:** 🟡 MEDIUM
- SIMD cần fallback cho old CPUs
- JPEG optimization có thể ảnh hưởng quality (cần verify)
- Work distribution cần testing với mixed sizes

**Validation:**
```bash
# 1. Quality test
ruby bindings/ruby/test_all_features.rb

# 2. SIMD correctness
# So sánh output SIMD vs scalar → phải giống hệt

# 3. Benchmark
./build/benchmark/bench_vs_libvips
# Expected: FastResize nhanh hơn libvips parallel 10-20%
```

---

## 📋 PHASE E3: ADVANCED TUNING (Low Impact, Higher Risk)

### Mục tiêu
- **+10-20% faster** so với Phase E2
- Advanced optimizations
- Có thể có trade-offs (memory, compatibility)

### E3.1: Memory-mapped I/O (Impact: 10% faster)

**Files cần sửa:**
- Tạo mới: `src/io_mmap.h`, `src/io_mmap.cpp`
- Sửa: `src/decoder.cpp`, `src/encoder.cpp`

**Hiện tại:**
```cpp
// decoder.cpp - sử dụng fread()
FILE* fp = fopen(path.c_str(), "rb");
fread(buffer, 1, size, fp);  // Copy từ kernel → userspace
```

**Vấn đề:**
- `fread()` copy data từ kernel space → user space
- Mỗi image: 1 copy operation
- Batch 500 images: 500 copy operations

**Giải pháp:**
```cpp
// mmap - zero-copy I/O
int fd = open(path.c_str(), O_RDONLY);
struct stat st;
fstat(fd, &st);

void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
// mapped points directly to kernel page cache
// NO COPY - decode directly từ mmap memory

// Decode...
munmap(mapped, st.st_size);
close(fd);
```

**Caveats:**
- mmap có thể fail với network drives
- Very large files (>2GB) trên 32-bit có vấn đề
- Cần fallback to fread()

**Implementation:**
```cpp
bool decode_with_mmap(const std::string& path, ImageData& data) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return false;
    }

    // Skip mmap for very large files or network drives
    if (st.st_size > 100 * 1024 * 1024) {  // > 100MB
        close(fd);
        return false;  // Fallback to fread
    }

    void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return false;  // Fallback to fread
    }

    // Decode từ mmap memory
    // ...

    munmap(mapped, st.st_size);
    close(fd);
    return true;
}
```

**Testing:**
```bash
# Test với files khác nhau
ruby bindings/ruby/test_all_features.rb

# Test edge cases
# - Very large files
# - Network drives
# - Permission issues

# Benchmark
cd build/benchmark
./bench_vs_libvips
```

**Expected result:**
- Local files: 5-10% faster
- Network files: fallback to fread (no regression)

---

### E3.2: Pipeline Tuning (Impact: 5% faster)

**Files cần sửa:**
- `src/pipeline.cpp`
- `src/pipeline.h`

**Hiện tại:**
```cpp
// pipeline.h:370
PipelineProcessor pipeline(4, 8, 4, 32);
// decode_threads=4, resize_threads=8, encode_threads=4, queue=32
```

**Vấn đề:**
- Fixed thread counts (4-8-4)
- Không adapt to workload
- JPEG-heavy vs PNG-heavy có optimal khác nhau

**Giải pháp: Dynamic thread allocation**
```cpp
// Detect workload characteristics
size_t jpeg_count = 0, png_count = 0, webp_count = 0;
for (const auto& item : items) {
    auto fmt = detect_format(item.input_path);
    if (fmt == FORMAT_JPEG) jpeg_count++;
    else if (fmt == FORMAT_PNG) png_count++;
    else if (fmt == FORMAT_WEBP) webp_count++;
}

// Adjust thread counts based on workload
size_t decode_threads, resize_threads, encode_threads;

if (jpeg_count > items.size() * 0.8) {
    // JPEG-heavy: encoding is fast, more decode threads
    decode_threads = 6;
    resize_threads = 8;
    encode_threads = 2;
} else if (png_count > items.size() * 0.8) {
    // PNG-heavy: encoding is slow, more encode threads
    decode_threads = 3;
    resize_threads = 8;
    encode_threads = 5;
} else {
    // Mixed: balanced
    decode_threads = 4;
    resize_threads = 8;
    encode_threads = 4;
}

PipelineProcessor pipeline(decode_threads, resize_threads,
                           encode_threads, 32);
```

**Testing:**
```bash
# Test với workloads khác nhau
# 1. 100% JPEG
# 2. 100% PNG
# 3. Mixed (50/50)

cd build/benchmark
./bench_pipeline
```

**Expected result:**
- JPEG-heavy: 5-10% faster
- PNG-heavy: 5-10% faster
- Mixed: ~5% faster

---

### E3.3: Prefetch Optimization (Impact: 5% faster)

**Files cần sửa:**
- `src/pipeline.cpp:40-78` (decode stage)

**Hiện tại:**
```cpp
// Sequential processing
for (size_t i = 0; i < items.size(); ++i) {
    decode_task(items[i]);  // Load từ disk khi cần
}
```

**Vấn đề:**
- Wait for disk I/O mỗi task
- CPU idle trong khi chờ I/O
- Không tận dụng OS page cache

**Giải pháp: Prefetch next N images**
```cpp
// Prefetch using posix_fadvise
for (size_t i = 0; i < items.size(); ++i) {
    // Prefetch next 3 images
    for (size_t j = i + 1; j < std::min(i + 4, items.size()); ++j) {
        int fd = open(items[j].input_path.c_str(), O_RDONLY);
        posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);  // Hint to OS
        close(fd);
    }

    // Process current image
    decode_task(items[i]);
}
```

**Testing:**
```bash
# Test với cold vs warm cache
echo 3 > /proc/sys/vm/drop_caches  # Linux only

cd build/benchmark
./bench_vs_libvips
```

**Expected result:**
- Cold cache: 10-15% faster
- Warm cache: 0-5% faster (already in cache)

---

### E3 Summary

**Changes:**
1. mmap I/O → zero-copy reads
2. Dynamic pipeline tuning → adapt to workload
3. Prefetch optimization → hide I/O latency

**Expected improvements:**
- I/O bound: 5-10% faster
- Mixed workloads: 5-10% faster
- **Overall: +10-20% faster than E2**

**Risk level:** 🟡 MEDIUM-HIGH
- mmap cần fallback cẩn thận
- Dynamic tuning có thể backfire với edge cases
- Prefetch có thể waste memory với large files

**Validation:**
```bash
# Full test suite
ruby bindings/ruby/test_all_features.rb

# Stress test với edge cases
# - Very large files (>100MB)
# - Many small files (<10KB)
# - Network drives
# - Low memory conditions

# Final benchmark
./build/benchmark/bench_vs_libvips
# Expected: FastResize nhanh hơn libvips parallel 20-30%
```

---

## 🎯 TỔNG KẾT PHASE E

### Performance Targets

| Phase | Optimization | Expected Gain | Cumulative | vs libvips |
|-------|-------------|---------------|------------|------------|
| **Baseline** | Current | 0% | 1.0x | Slower |
| **E1** | Quick Wins | +50-70% | 1.6x | ~Equal |
| **E2** | Deep Opt | +40-50% | 2.4x | +20% faster |
| **E3** | Advanced | +10-20% | 2.8x | +30% faster |

### Features Guarantee

✅ **100% Backward Compatible:**
- JPG, PNG, BMP, WEBP support
- Single + Batch resize
- All resize modes (%, width, height, exact)
- Custom params per image
- Quality settings
- Filter options

### Testing Strategy

**After each phase:**
```bash
# 1. Correctness
ruby bindings/ruby/test_all_features.rb
# Must: ALL TESTS PASSED ✅

# 2. Performance
./build/benchmark/bench_vs_libvips
# Track: Time, Throughput, per-image latency

# 3. Quality
# Visual inspection + PSNR check
# Must: PSNR > 40dB, visually identical
```

### Benchmark Command

```bash
# Build optimized
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native"
cmake --build build -j8

# Run benchmark
cd /Users/canh.th/Desktop/fastgems/fastresize
./build/benchmark/bench_vs_libvips

# Expected output format:
# ============================================================
# COMPARISON
# ============================================================
# Library           Time(s)  Throughput   Per Image
# ------------------------------------------------------------
# FastResize (E1)    4.23    118.5/s      8.43ms     ← Phase E1
# FastResize (E2)    2.95    169.5/s      5.90ms     ← Phase E2
# FastResize (E3)    2.50    200.0/s      5.00ms     ← Phase E3
# libvips parallel   3.80    131.6/s      7.60ms
#
# 🏆 Winner: FastResize (E3)
#    34% faster than libvips parallel
```

---

## 📝 KHI TAG VÀO FILE NÀY

### Phase E1: Quick Wins
**Tag format:** `@claude implement E1.1` hoặc `@claude start Phase E1`

**Tôi sẽ làm:**
1. ✅ Read file này để hiểu context
2. ✅ Implement E1.1 (PNG mutex), E1.2 (buffer pool), E1.3 (pipeline threshold)
3. ✅ Run tests: `ruby test_all_features.rb`
4. ✅ Run benchmark: `./build/benchmark/bench_vs_libvips`
5. ✅ Report results: performance gain, pass/fail tests
6. ✅ Commit changes với detailed message

### Phase E2: Deep Optimization
**Tag format:** `@claude implement E2.1` hoặc `@claude start Phase E2`

**Tôi sẽ làm:**
1. ✅ Implement SIMD, JPEG opt, work distribution
2. ✅ Test correctness + performance
3. ✅ Benchmark so sánh với E1
4. ✅ Report & commit

### Phase E3: Advanced Tuning
**Tag format:** `@claude implement E3` hoặc `@claude start Phase E3`

**Tôi sẽ làm:**
1. ✅ Implement mmap, dynamic tuning, prefetch
2. ✅ Test với edge cases
3. ✅ Final benchmark vs libvips
4. ✅ Summary report với charts

### Benchmark After Each Phase
**Tag format:** `@claude benchmark E1` hoặc `@claude compare performance`

**Tôi sẽ làm:**
1. ✅ Build release binary
2. ✅ Run `bench_vs_libvips`
3. ✅ Parse results
4. ✅ Generate comparison table
5. ✅ Analyze bottlenecks if any

---

## 🚀 READY TO START

**Recommended order:**
1. `@claude start Phase E1` - Quick wins, low risk
2. `@claude benchmark E1` - Verify gains
3. `@claude start Phase E2` - Deep optimization
4. `@claude benchmark E2` - Verify gains
5. `@claude start Phase E3` - Advanced tuning
6. `@claude benchmark E3` - Final comparison

**Estimated timeline:**
- E1: 2-3 days (worth 50-70% gain) ⚡⚡⚡
- E2: 3-5 days (worth 40-50% gain) ⚡⚡
- E3: 5-7 days (worth 10-20% gain) ⚡

**Total: ~2 weeks để đạt 2-3x faster than current, thắng libvips**

---

## 📊 SUCCESS CRITERIA

### Phase E1 Success
- [ ] Tests pass 100%
- [ ] PNG batch: ≥40% faster
- [ ] JPEG (RGBA): ≥15% faster
- [ ] Overall: ≥50% faster
- [ ] vs libvips: ~equal or faster

### Phase E2 Success
- [ ] Tests pass 100%
- [ ] SIMD working on Apple Silicon
- [ ] JPEG: ≥20% faster than E1
- [ ] Overall: ≥40% faster than E1
- [ ] vs libvips: ≥15% faster

### Phase E3 Success
- [ ] Tests pass 100%
- [ ] Edge cases handled (mmap fallback, etc)
- [ ] Overall: ≥10% faster than E2
- [ ] vs libvips: ≥25% faster
- [ ] **Memory usage stable** (no leaks)

### Final Success
- [ ] **FastResize ≥2.5x faster than baseline**
- [ ] **FastResize ≥20% faster than libvips parallel**
- [ ] 100% backward compatible
- [ ] All tests passing
- [ ] No memory leaks (valgrind clean)
- [ ] Production ready ✅

---

**Version:** 1.0
**Created:** 2025-12-03
**Status:** Ready for implementation
**Contact:** Tag @claude with phase name to start
