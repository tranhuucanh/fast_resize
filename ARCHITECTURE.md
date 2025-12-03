# FastResize - Architecture & Implementation Plan

> **Fast, lightweight, cross-platform image resizing library in C++ with Ruby bindings**
>
> Last updated: 2025-11-29

---

## 📋 Table of Contents

- [Overview](#overview)
- [Requirements](#requirements)
- [Architecture Decision](#architecture-decision)
- [Technology Stack](#technology-stack)
- [Directory Structure](#directory-structure)
- [API Design](#api-design)
- [Implementation Phases](#implementation-phases)
- [Build System](#build-system)
- [Cross-Platform Strategy](#cross-platform-strategy)
- [Performance Optimization](#performance-optimization)
- [Testing Strategy](#testing-strategy)
- [Deployment](#deployment)

---

## 🎯 Overview

FastResize is a high-performance image resizing library designed to:
- Resize images with minimal CPU and RAM usage
- Support batch processing (e.g., 300 images simultaneously)
- Provide Ruby bindings for easy integration
- Work across macOS (x64, ARM) and Linux (x64, ARM)

### Key Features

✅ **Image Formats**: PNG, JPG, JPEG, WEBP, BMP
✅ **Resize Modes**:
  - Percentage scaling (e.g., 50% of original)
  - Fixed width (height scales proportionally)
  - Fixed width & height (exact dimensions)
  - Keep aspect ratio option
✅ **Output Options**:
  - Save to new file
  - Overwrite original file
✅ **Batch Processing**: Process multiple images in parallel with configurable thread pool

---

## 📊 Requirements

### Performance Goals

| Metric | Target |
|--------|--------|
| Speed | Resize 300 images (2000x2000 → 800x600) in < 3 seconds |
| CPU Usage | Configurable thread pool (default: 4-8 threads) |
| Memory | Pre-allocated buffer pool, no unnecessary allocations |
| Footprint | Static library < 2MB |

### License Requirements

⚠️ **CRITICAL**: Must use permissive licenses compatible with static linking:
- ✅ MIT, BSD, Public Domain → OK
- ❌ LGPL, GPL → Not suitable for static linking

---

## 🏗️ Architecture Decision

### Why NOT libvips?

| Issue | Problem |
|-------|---------|
| **Size** | 5-20MB (requires glib, gobject) |
| **License** | LGPL 2.1 - complex with static linking |
| **Dependencies** | glib-2.0 (heavyweight framework) |
| **Build Complexity** | Cross-compiling glib is difficult |

### ✅ Chosen Solution: stb_image_resize2 + Image Codecs

```
┌────────────────────────────────────────────────┐
│           FastResize Core Library              │
├────────────────────────────────────────────────┤
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │  Image Decode (Input)                    │ │
│  ├──────────────────────────────────────────┤ │
│  │  • libjpeg-turbo  (JPG/JPEG) - BSD      │ │
│  │  • libpng         (PNG)      - PNG Lic  │ │
│  │  • libwebp        (WEBP)     - BSD      │ │
│  │  • stb_image.h    (BMP)      - Public   │ │
│  └──────────────────────────────────────────┘ │
│                     ↓                          │
│  ┌──────────────────────────────────────────┐ │
│  │  Image Resize                            │ │
│  ├──────────────────────────────────────────┤ │
│  │  • stb_image_resize2.h - MIT/Public     │ │
│  │  • SIMD support (SSE2, AVX, NEON)       │ │
│  │  • Multiple filters (Mitchell, Catmull) │ │
│  └──────────────────────────────────────────┘ │
│                     ↓                          │
│  ┌──────────────────────────────────────────┐ │
│  │  Image Encode (Output)                   │ │
│  ├──────────────────────────────────────────┤ │
│  │  • libjpeg-turbo  (JPG/JPEG)            │ │
│  │  • libpng         (PNG)                  │ │
│  │  • libwebp        (WEBP)                 │ │
│  │  • stb_image_write.h (BMP)              │ │
│  └──────────────────────────────────────────┘ │
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │  Threading & Memory Management           │ │
│  ├──────────────────────────────────────────┤ │
│  │  • Custom thread pool (4-8 threads)     │ │
│  │  • Buffer pool (pre-allocated)          │ │
│  │  • Smart memory reuse                    │ │
│  └──────────────────────────────────────────┘ │
│                                                │
└────────────────────────────────────────────────┘
                      ↓
         ┌────────────────────────┐
         │   Ruby C Extension     │
         │   (via extconf.rb)     │
         └────────────────────────┘
```

---

## 🔧 Technology Stack

### Core Dependencies

| Library | License | Purpose | Static Link |
|---------|---------|---------|-------------|
| **stb_image_resize2.h** | MIT/Public Domain | Image resizing with SIMD | ✅ Header-only |
| **stb_image.h** | Public Domain | Decode BMP | ✅ Header-only |
| **stb_image_write.h** | Public Domain | Encode BMP | ✅ Header-only |
| **libjpeg-turbo** | BSD-style | Decode/Encode JPG | ✅ Compatible |
| **libpng** | PNG License (permissive) | Decode/Encode PNG | ✅ Compatible |
| **libwebp** | BSD | Decode/Encode WEBP | ✅ Compatible |

### Build Tools

- **CMake** 3.15+ - Cross-platform build system
- **pkg-config** - Dependency management
- **GitHub Actions** - CI/CD for multi-platform builds

---

## 📁 Directory Structure

```
fastresize/
├── CMakeLists.txt              # Main build configuration
├── VERSION                     # Version number (e.g., 1.0.0)
├── LICENSE                     # MIT License
├── README.md                   # Project overview
├── ARCHITECTURE.md             # This file
├── CHANGELOG.md                # Version history
├── fastresize.gemspec          # Ruby gem specification
│
├── .github/
│   └── workflows/
│       ├── build.yml           # Build & test workflow
│       └── release.yml         # Release workflow (multi-platform)
│
├── include/
│   ├── fastresize.h            # Public C++ API
│   ├── stb_image.h             # Image loading (BMP)
│   ├── stb_image_write.h       # Image writing (BMP)
│   └── stb_image_resize2.h     # Image resizing
│
├── src/
│   ├── fastresize.cpp          # Core implementation
│   ├── decoder.cpp             # Image decoding logic
│   ├── encoder.cpp             # Image encoding logic
│   ├── resizer.cpp             # Resize operations
│   ├── thread_pool.cpp         # Thread pool for batch
│   └── cli.cpp                 # CLI tool (optional)
│
├── bindings/
│   └── ruby/
│       ├── ext/
│       │   ├── fastresize/
│       │   │   ├── extconf.rb  # Ruby extension config
│       │   │   └── fastresize_ruby.cpp  # Ruby C extension
│       ├── lib/
│       │   └── fastresize.rb   # Ruby wrapper
│       ├── spec/               # RSpec tests
│       └── README.md           # Ruby usage guide
│
├── examples/
│   ├── basic_resize.cpp        # Single image resize
│   ├── batch_resize.cpp        # Batch processing
│   └── CMakeLists.txt
│
├── tests/
│   ├── test_decode.cpp         # Decoder tests
│   ├── test_resize.cpp         # Resize tests
│   ├── test_encode.cpp         # Encoder tests
│   └── test_batch.cpp          # Batch processing tests
│
├── benchmark/
│   ├── bench_single.cpp        # Single image benchmark
│   └── bench_batch.cpp         # Batch benchmark
│
├── docs/
│   ├── README.md               # Documentation index
│   ├── API.md                  # C++ API reference
│   ├── RUBY_USAGE.md           # Ruby usage guide
│   └── BENCHMARKS.md           # Performance benchmarks
│
├── scripts/
│   ├── build_static_deps.sh    # Build static libraries
│   ├── cross_compile.sh        # Cross-compilation helper
│   └── package_gem.sh          # Package Ruby gem
│
└── prebuilt/                   # Prebuilt static libraries
    ├── README.md
    ├── macos-x64/
    ├── macos-arm64/
    ├── linux-x64/
    └── linux-aarch64/
```

---

## 🎨 API Design

### C++ API

```cpp
#include <fastresize.h>

namespace fastresize {

// ============================================
// Core Structures
// ============================================

struct ResizeOptions {
    // Resize mode
    enum Mode {
        SCALE_PERCENT,      // Scale by percentage
        FIT_WIDTH,          // Fixed width, height auto
        FIT_HEIGHT,         // Fixed height, width auto
        EXACT_SIZE          // Exact width & height
    } mode;

    // Dimensions
    int target_width;       // Target width (pixels)
    int target_height;      // Target height (pixels)
    float scale_percent;    // Scale percentage (0.0-1.0)

    // Options
    bool keep_aspect_ratio; // Preserve aspect ratio (default: true)
    bool overwrite_input;   // Overwrite input file (default: false)

    // Quality
    int quality;            // JPEG/WEBP quality 1-100 (default: 85)

    // Filter
    enum Filter {
        MITCHELL,           // Default, good balance
        CATMULL_ROM,        // Sharp edges
        BOX,                // Fast, lower quality
        TRIANGLE            // Bilinear
    } filter;

    // Constructor with defaults
    ResizeOptions()
        : mode(EXACT_SIZE)
        , target_width(0)
        , target_height(0)
        , scale_percent(1.0f)
        , keep_aspect_ratio(true)
        , overwrite_input(false)
        , quality(85)
        , filter(MITCHELL)
    {}
};

struct ImageInfo {
    int width;
    int height;
    int channels;           // 1=Gray, 3=RGB, 4=RGBA
    std::string format;     // "jpg", "png", "webp", "bmp"
};

// ============================================
// Single Image Resize
// ============================================

// Resize single image (auto-detect format)
bool resize(
    const std::string& input_path,
    const std::string& output_path,
    const ResizeOptions& options
);

// Resize with explicit format
bool resize_with_format(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& output_format,  // "jpg", "png", "webp", "bmp"
    const ResizeOptions& options
);

// Get image info without loading
ImageInfo get_image_info(const std::string& path);

// ============================================
// Batch Processing
// ============================================

struct BatchItem {
    std::string input_path;
    std::string output_path;
    ResizeOptions options;  // Per-image options
};

struct BatchOptions {
    int num_threads;        // Thread pool size (default: 8)
    bool stop_on_error;     // Stop if any image fails (default: false)

    BatchOptions()
        : num_threads(8)
        , stop_on_error(false)
    {}
};

struct BatchResult {
    int total;              // Total images
    int success;            // Successfully processed
    int failed;             // Failed to process
    std::vector<std::string> errors;  // Error messages
};

// Batch resize - same options for all images
BatchResult batch_resize(
    const std::vector<std::string>& input_paths,
    const std::string& output_dir,
    const ResizeOptions& options,
    const BatchOptions& batch_opts = BatchOptions()
);

// Batch resize - individual options per image
BatchResult batch_resize_custom(
    const std::vector<BatchItem>& items,
    const BatchOptions& batch_opts = BatchOptions()
);

// ============================================
// Error Handling
// ============================================

// Get last error message
std::string get_last_error();

// Error codes
enum ErrorCode {
    OK = 0,
    FILE_NOT_FOUND,
    UNSUPPORTED_FORMAT,
    DECODE_ERROR,
    RESIZE_ERROR,
    ENCODE_ERROR,
    WRITE_ERROR
};

ErrorCode get_last_error_code();

} // namespace fastresize
```

### Ruby API

```ruby
require 'fastresize'

# ============================================
# Single Image Resize
# ============================================

# Simple resize (exact size)
FastResize.resize("input.jpg", "output.jpg", width: 800, height: 600)

# Resize by percentage
FastResize.resize("input.jpg", "output.jpg", scale: 0.5)  # 50%

# Resize width only (height auto)
FastResize.resize("input.jpg", "output.jpg", width: 800)

# Resize with quality
FastResize.resize("input.jpg", "output.jpg",
                  width: 800,
                  quality: 90)

# Overwrite original
FastResize.resize("input.jpg", "input.jpg", width: 800, overwrite: true)

# Change format
FastResize.resize("input.jpg", "output.webp", width: 800, format: "webp")

# ============================================
# Batch Processing
# ============================================

# Batch resize - same size for all
files = ["img1.jpg", "img2.jpg", "img3.jpg"]
FastResize.batch_resize(files, "output_dir/", width: 800, height: 600)

# Batch resize - by percentage
FastResize.batch_resize(files, "output_dir/", scale: 0.5)

# Batch resize - custom per file
items = [
  { input: "img1.jpg", output: "out1.jpg", width: 800 },
  { input: "img2.jpg", output: "out2.jpg", width: 1024 },
  { input: "img3.jpg", output: "out3.jpg", scale: 0.5 }
]
result = FastResize.batch_resize_custom(items)
puts "Success: #{result[:success]}, Failed: #{result[:failed]}"

# Batch with thread control
FastResize.batch_resize(files, "output_dir/",
                        width: 800,
                        threads: 4)

# ============================================
# Image Info
# ============================================

info = FastResize.image_info("input.jpg")
# => { width: 2000, height: 1500, channels: 3, format: "jpg" }
```

---

## 🚀 Implementation Phases

### Phase 1: Project Setup & Core Infrastructure

**Goal**: Set up project structure, build system, and basic image I/O

**Tasks**:
1. ✅ Create directory structure (following fastqr pattern)
2. ✅ Initialize CMake build system
3. ✅ Add stb headers (stb_image.h, stb_image_write.h, stb_image_resize2.h)
4. ✅ Create basic `fastresize.h` API header
5. ✅ Implement image format detection
6. ✅ Implement basic decoder (BMP via stb_image)
7. ✅ Implement basic encoder (BMP via stb_image_write)
8. ✅ Write unit tests for decode/encode

**Deliverables**:
- Working CMake project
- Can decode/encode BMP images
- Basic test suite passes

**Implementation Details**:

```cpp
// src/decoder.cpp - Image format detection
namespace fastresize {
namespace internal {

enum ImageFormat {
    FORMAT_UNKNOWN,
    FORMAT_JPEG,
    FORMAT_PNG,
    FORMAT_WEBP,
    FORMAT_BMP
};

// Detect format from file header (magic bytes)
ImageFormat detect_format(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return FORMAT_UNKNOWN;

    unsigned char header[12];
    size_t n = fread(header, 1, 12, f);
    fclose(f);

    if (n < 4) return FORMAT_UNKNOWN;

    // JPEG: FF D8 FF
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF)
        return FORMAT_JPEG;

    // PNG: 89 50 4E 47
    if (header[0] == 0x89 && header[1] == 0x50 &&
        header[2] == 0x4E && header[3] == 0x47)
        return FORMAT_PNG;

    // WEBP: RIFF...WEBP
    if (header[0] == 'R' && header[1] == 'I' &&
        header[2] == 'F' && header[3] == 'F' &&
        header[8] == 'W' && header[9] == 'E' &&
        header[10] == 'B' && header[11] == 'P')
        return FORMAT_WEBP;

    // BMP: BM
    if (header[0] == 'B' && header[1] == 'M')
        return FORMAT_BMP;

    return FORMAT_UNKNOWN;
}

// Decode image to raw pixels (using stb_image for Phase 1)
struct ImageData {
    unsigned char* pixels;
    int width;
    int height;
    int channels;
};

ImageData decode_image_stb(const std::string& path) {
    ImageData data;
    data.pixels = stbi_load(path.c_str(),
                           &data.width,
                           &data.height,
                           &data.channels,
                           0);  // 0 = keep original channels
    return data;
}

void free_image_data(ImageData& data) {
    if (data.pixels) {
        stbi_image_free(data.pixels);
        data.pixels = nullptr;
    }
}

} // namespace internal
} // namespace fastresize
```

---

### Phase 2: Image Resizing Core

**Goal**: Implement resize functionality using stb_image_resize2

**Tasks**:
1. ✅ Integrate stb_image_resize2.h
2. ✅ Implement resize logic for all modes (SCALE_PERCENT, FIT_WIDTH, etc.)
3. ✅ Add aspect ratio preservation
4. ✅ Support different filters (Mitchell, Catmull-Rom, etc.)
5. ✅ Write resize unit tests
6. ✅ Benchmark resize performance

**Deliverables**:
- Working resize() function
- Support all resize modes
- Basic performance benchmarks

**Implementation Details**:

```cpp
// src/resizer.cpp
namespace fastresize {
namespace internal {

// Calculate output dimensions based on mode
void calculate_dimensions(
    int in_w, int in_h,
    const ResizeOptions& opts,
    int& out_w, int& out_h
) {
    switch (opts.mode) {
        case ResizeOptions::SCALE_PERCENT:
            out_w = static_cast<int>(in_w * opts.scale_percent);
            out_h = static_cast<int>(in_h * opts.scale_percent);
            break;

        case ResizeOptions::FIT_WIDTH:
            out_w = opts.target_width;
            if (opts.keep_aspect_ratio) {
                float ratio = static_cast<float>(out_w) / in_w;
                out_h = static_cast<int>(in_h * ratio);
            } else {
                out_h = in_h;
            }
            break;

        case ResizeOptions::FIT_HEIGHT:
            out_h = opts.target_height;
            if (opts.keep_aspect_ratio) {
                float ratio = static_cast<float>(out_h) / in_h;
                out_w = static_cast<int>(in_w * ratio);
            } else {
                out_w = in_w;
            }
            break;

        case ResizeOptions::EXACT_SIZE:
            out_w = opts.target_width;
            out_h = opts.target_height;
            break;
    }
}

// Perform resize using stb_image_resize2
bool resize_image(
    const unsigned char* input_pixels,
    int input_w, int input_h, int channels,
    unsigned char** output_pixels,
    int output_w, int output_h,
    const ResizeOptions& opts
) {
    // Allocate output buffer
    size_t output_size = output_w * output_h * channels;
    *output_pixels = new unsigned char[output_size];

    // Map our filter enum to stb filter
    stbir_filter stb_filter;
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

    // Perform resize
    stbir_datatype datatype = STBIR_TYPE_UINT8;
    stbir_pixel_layout pixel_layout =
        (channels == 4) ? STBIR_RGBA :
        (channels == 3) ? STBIR_RGB :
        STBIR_1CHANNEL;

    int result = stbir_resize(
        input_pixels, input_w, input_h, 0,
        *output_pixels, output_w, output_h, 0,
        pixel_layout, datatype,
        STBIR_EDGE_CLAMP,
        stb_filter
    );

    return result != 0;
}

} // namespace internal
} // namespace fastresize
```

---

### Phase 3: Advanced Image Codecs

**Goal**: Add support for JPEG, PNG, WEBP using specialized libraries

**Tasks**:
1. ✅ Integrate libjpeg-turbo for JPEG decode/encode
2. ✅ Integrate libpng for PNG decode/encode
3. ✅ Integrate libwebp for WEBP decode/encode
4. ✅ Implement quality control for lossy formats
5. ✅ Update CMake to find and link these libraries statically
6. ✅ Write tests for all formats
7. ✅ Compare performance: stb vs specialized libraries

**Deliverables**:
- Full support for JPG, PNG, WEBP
- Static linking works
- Performance comparison document

**Implementation Details**:

```cmake
# CMakeLists.txt - Finding dependencies
find_package(PkgConfig REQUIRED)

# Find libjpeg-turbo
pkg_check_modules(JPEG libjpeg)
if(NOT JPEG_FOUND)
    find_package(JPEG REQUIRED)
endif()

# Find libpng
pkg_check_modules(PNG REQUIRED libpng)

# Find libwebp
pkg_check_modules(WEBP libwebp libwebpdecoder libwebpdemux)
if(NOT WEBP_FOUND)
    message(WARNING "libwebp not found, WEBP support disabled")
endif()

# Static library option
option(FASTRESIZE_STATIC "Build static library" ON)

if(FASTRESIZE_STATIC)
    # Find static versions
    find_library(JPEG_STATIC_LIB NAMES libjpeg.a libturbojpeg.a)
    find_library(PNG_STATIC_LIB NAMES libpng.a libpng16.a)
    find_library(WEBP_STATIC_LIB NAMES libwebp.a)
    find_library(Z_STATIC_LIB NAMES libz.a)

    # Use static libraries
    target_link_libraries(fastresize PRIVATE
        ${JPEG_STATIC_LIB}
        ${PNG_STATIC_LIB}
        ${WEBP_STATIC_LIB}
        ${Z_STATIC_LIB}
    )
else()
    # Use dynamic libraries
    target_link_libraries(fastresize PRIVATE
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
        ${WEBP_LIBRARIES}
    )
endif()
```

```cpp
// src/decoder.cpp - JPEG decoding with libjpeg-turbo
#include <jpeglib.h>
#include <csetjmp>

namespace fastresize {
namespace internal {

// Error handling for libjpeg
struct jpeg_error_mgr_ext {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

void jpeg_error_exit(j_common_ptr cinfo) {
    jpeg_error_mgr_ext* myerr = (jpeg_error_mgr_ext*)cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);
}

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
    jpeg_start_decompress(&cinfo);

    data.width = cinfo.output_width;
    data.height = cinfo.output_height;
    data.channels = cinfo.output_components;

    size_t row_stride = data.width * data.channels;
    data.pixels = new unsigned char[data.height * row_stride];

    JSAMPROW row_pointer[1];
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &data.pixels[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return data;
}

} // namespace internal
} // namespace fastresize
```

---

### Phase 4: Batch Processing & Threading

**Goal**: Implement efficient batch processing with thread pool

**Tasks**:
1. ✅ Design and implement thread pool
2. ✅ Add buffer pool for memory reuse
3. ✅ Implement batch_resize() functions
4. ✅ Add progress tracking
5. ✅ Implement error handling (continue on error vs stop)
6. ✅ Write batch processing tests
7. ✅ Benchmark: 300 images performance test

**Deliverables**:
- Working batch processing
- Thread pool with configurable size
- Memory-efficient buffer reuse
- Batch performance meets target (< 3s for 300 images)

**Implementation Details**:

```cpp
// src/thread_pool.h
namespace fastresize {
namespace internal {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit task to queue
    template<typename F>
    void enqueue(F&& task);

    // Wait for all tasks to complete
    void wait();

    size_t get_thread_count() const { return threads_.size(); }

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<int> active_tasks_;
};

// src/thread_pool.cpp
ThreadPool::ThreadPool(size_t num_threads)
    : stop_(false)
    , active_tasks_(0)
{
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if (stop_ && tasks_.empty())
                        return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                ++active_tasks_;
                task();
                --active_tasks_;
            }
        });
    }
}

template<typename F>
void ThreadPool::enqueue(F&& task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::forward<F>(task));
    }
    condition_.notify_one();
}

void ThreadPool::wait() {
    while (active_tasks_ > 0 || !tasks_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Buffer pool for memory reuse
class BufferPool {
public:
    struct Buffer {
        unsigned char* data;
        size_t capacity;
    };

    Buffer acquire(size_t size);
    void release(Buffer buffer);

private:
    std::vector<Buffer> pool_;
    std::mutex mutex_;
};

} // namespace internal
} // namespace fastresize
```

---

### Phase 5: Ruby Binding

**Goal**: Create Ruby C extension for easy integration

**Tasks**:
1. ✅ Create `bindings/ruby/` structure
2. ✅ Write `extconf.rb` for compilation
3. ✅ Implement Ruby C extension (`fastresize_ruby.cpp`)
4. ✅ Write Ruby wrapper (`lib/fastresize.rb`)
5. ✅ Create `fastresize.gemspec`
6. ✅ Write RSpec tests
7. ✅ Test on macOS and Linux

**Deliverables**:
- Working Ruby gem
- Can install via `gem install fastresize`
- Ruby API matches design spec
- RSpec tests pass

**Implementation Details**:

```ruby
# bindings/ruby/ext/fastresize/extconf.rb
require 'mkmf'

# Find fastresize library
unless have_library('fastresize')
  abort "fastresize library not found"
end

# Find header
unless have_header('fastresize.h')
  abort "fastresize.h not found"
end

# Create Makefile
create_makefile('fastresize/fastresize')
```

```cpp
// bindings/ruby/ext/fastresize/fastresize_ruby.cpp
#include <ruby.h>
#include <fastresize.h>

static VALUE rb_fastresize_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_path, output_path, options;
    rb_scan_args(argc, argv, "21", &input_path, &output_path, &options);

    // Convert Ruby strings to C++
    std::string input = StringValueCStr(input_path);
    std::string output = StringValueCStr(output_path);

    // Parse options hash
    fastresize::ResizeOptions opts;
    if (!NIL_P(options)) {
        VALUE width = rb_hash_aref(options, ID2SYM(rb_intern("width")));
        if (!NIL_P(width)) {
            opts.target_width = NUM2INT(width);
            opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
        }

        VALUE height = rb_hash_aref(options, ID2SYM(rb_intern("height")));
        if (!NIL_P(height)) {
            opts.target_height = NUM2INT(height);
            if (!NIL_P(width)) {
                opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
            } else {
                opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
            }
        }

        VALUE scale = rb_hash_aref(options, ID2SYM(rb_intern("scale")));
        if (!NIL_P(scale)) {
            opts.scale_percent = NUM2DBL(scale);
            opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
        }

        VALUE quality = rb_hash_aref(options, ID2SYM(rb_intern("quality")));
        if (!NIL_P(quality)) {
            opts.quality = NUM2INT(quality);
        }
    }

    // Perform resize
    bool success = fastresize::resize(input, output, opts);

    return success ? Qtrue : Qfalse;
}

static VALUE rb_fastresize_batch_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_files, output_dir, options;
    rb_scan_args(argc, argv, "21", &input_files, &output_dir, &options);

    // Convert Ruby array to C++ vector
    std::vector<std::string> inputs;
    long len = RARRAY_LEN(input_files);
    for (long i = 0; i < len; ++i) {
        VALUE item = rb_ary_entry(input_files, i);
        inputs.push_back(StringValueCStr(item));
    }

    std::string out_dir = StringValueCStr(output_dir);

    // Parse options (similar to above)
    fastresize::ResizeOptions resize_opts;
    fastresize::BatchOptions batch_opts;
    // ... parse options ...

    // Perform batch resize
    fastresize::BatchResult result =
        fastresize::batch_resize(inputs, out_dir, resize_opts, batch_opts);

    // Return result hash
    VALUE rb_result = rb_hash_new();
    rb_hash_aset(rb_result, ID2SYM(rb_intern("total")), INT2NUM(result.total));
    rb_hash_aset(rb_result, ID2SYM(rb_intern("success")), INT2NUM(result.success));
    rb_hash_aset(rb_result, ID2SYM(rb_intern("failed")), INT2NUM(result.failed));

    return rb_result;
}

void Init_fastresize(void) {
    VALUE mFastResize = rb_define_module("FastResize");

    rb_define_singleton_method(mFastResize, "resize",
        RUBY_METHOD_FUNC(rb_fastresize_resize), -1);
    rb_define_singleton_method(mFastResize, "batch_resize",
        RUBY_METHOD_FUNC(rb_fastresize_batch_resize), -1);
}
```

```ruby
# fastresize.gemspec
Gem::Specification.new do |spec|
  spec.name          = "fastresize"
  spec.version       = File.read("VERSION").strip
  spec.authors       = ["Your Name"]
  spec.email         = ["your.email@example.com"]

  spec.summary       = "Fast image resizing library"
  spec.description   = "High-performance image resizing with C++ backend"
  spec.homepage      = "https://github.com/yourusername/fastresize"
  spec.license       = "MIT"

  spec.files         = Dir["lib/**/*", "ext/**/*", "README.md", "LICENSE"]
  spec.require_paths = ["lib"]
  spec.extensions    = ["bindings/ruby/ext/fastresize/extconf.rb"]

  spec.required_ruby_version = ">= 2.5.0"

  spec.add_development_dependency "rake", "~> 13.0"
  spec.add_development_dependency "rspec", "~> 3.0"
end
```

---

### Phase 6: Cross-Platform Build & CI/CD

**Goal**: Set up automated builds for all target platforms

**Tasks**:
1. ✅ Create GitHub Actions workflow for builds
2. ✅ Set up macOS x64 build
3. ✅ Set up macOS ARM build
4. ✅ Set up Linux x64 build
5. ✅ Set up Linux ARM (aarch64) build
6. ✅ Static linking validation on all platforms
7. ✅ Create prebuilt binaries
8. ✅ Automate gem packaging with prebuilt binaries

**Deliverables**:
- Automated multi-platform builds
- Prebuilt static libraries for all platforms
- Ruby gems with platform-specific binaries
- Release workflow

**Implementation Details**:

```yaml
# .github/workflows/release.yml
name: Build and Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build-macos-x64:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          brew install cmake pkg-config jpeg libpng webp

      - name: Build static libraries
        run: |
          mkdir build && cd build
          cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_ARCHITECTURES=x86_64 \
            -DFASTRESIZE_STATIC=ON
          make -j$(sysctl -n hw.ncpu)

      - name: Package
        run: |
          mkdir -p prebuilt/macos-x64
          cp build/libfastresize.a prebuilt/macos-x64/
          tar -czf fastresize-macos-x64.tar.gz prebuilt/

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: macos-x64
          path: fastresize-macos-x64.tar.gz

  build-macos-arm64:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          brew install cmake pkg-config jpeg libpng webp

      - name: Build static libraries
        run: |
          mkdir build && cd build
          cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_ARCHITECTURES=arm64 \
            -DFASTRESIZE_STATIC=ON
          make -j$(sysctl -n hw.ncpu)

      - name: Package
        run: |
          mkdir -p prebuilt/macos-arm64
          cp build/libfastresize.a prebuilt/macos-arm64/
          tar -czf fastresize-macos-arm64.tar.gz prebuilt/

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: macos-arm64
          path: fastresize-macos-arm64.tar.gz

  build-linux-x64:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake pkg-config \
            libjpeg-turbo8-dev libpng-dev libwebp-dev

      - name: Build static libraries
        run: |
          mkdir build && cd build
          cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DFASTRESIZE_STATIC=ON
          make -j$(nproc)

      - name: Package
        run: |
          mkdir -p prebuilt/linux-x64
          cp build/libfastresize.a prebuilt/linux-x64/
          tar -czf fastresize-linux-x64.tar.gz prebuilt/

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: linux-x64
          path: fastresize-linux-x64.tar.gz

  build-linux-arm64:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Set up QEMU
        uses: docker/setup-qemu-action@v2

      - name: Build in ARM64 container
        run: |
          docker run --rm --platform linux/arm64 \
            -v $PWD:/workspace \
            -w /workspace \
            arm64v8/ubuntu:22.04 \
            bash -c "
              apt-get update && \
              apt-get install -y cmake pkg-config g++ \
                libjpeg-turbo8-dev libpng-dev libwebp-dev && \
              mkdir build && cd build && \
              cmake .. -DCMAKE_BUILD_TYPE=Release -DFASTRESIZE_STATIC=ON && \
              make -j\$(nproc)
            "

      - name: Package
        run: |
          mkdir -p prebuilt/linux-aarch64
          cp build/libfastresize.a prebuilt/linux-aarch64/
          tar -czf fastresize-linux-aarch64.tar.gz prebuilt/

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: linux-aarch64
          path: fastresize-linux-aarch64.tar.gz

  release:
    needs: [build-macos-x64, build-macos-arm64, build-linux-x64, build-linux-arm64]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Download all artifacts
        uses: actions/download-artifact@v3

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            macos-x64/fastresize-macos-x64.tar.gz
            macos-arm64/fastresize-macos-arm64.tar.gz
            linux-x64/fastresize-linux-x64.tar.gz
            linux-aarch64/fastresize-linux-aarch64.tar.gz
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

      - name: Build Ruby gem
        run: |
          gem build fastresize.gemspec

      - name: Publish to RubyGems
        run: |
          gem push fastresize-*.gem
        env:
          GEM_HOST_API_KEY: ${{ secrets.RUBYGEMS_API_KEY }}
```

---

### Phase 7: Documentation & Examples

**Goal**: Comprehensive documentation and usage examples

**Tasks**:
1. ✅ Write README.md with quick start
2. ✅ Write API.md with full C++ API reference
3. ✅ Write RUBY_USAGE.md with Ruby examples
4. ✅ Create example programs (basic, batch, advanced)
5. ✅ Write BENCHMARKS.md with performance data
6. ✅ Add inline code documentation
7. ✅ Create tutorial videos/GIFs

**Deliverables**:
- Complete documentation
- Working examples
- Performance benchmarks published

---

### Phase 8: Testing & Optimization

**Goal**: Comprehensive testing and performance tuning

**Tasks**:
1. ✅ Unit tests for all modules (decode, resize, encode)
2. ✅ Integration tests (end-to-end)
3. ✅ Batch processing stress tests (1000+ images)
4. ✅ Memory leak detection (valgrind)
5. ✅ Performance profiling (perf, Instruments)
6. ✅ Optimize hot paths
7. ✅ Final benchmark suite

**Deliverables**:
- Test coverage > 80%
- No memory leaks
- Performance meets all targets
- Benchmark report

---

## 🛠️ Build System

### CMake Structure

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(fastresize VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")

# Options
option(FASTRESIZE_STATIC "Build static library" ON)
option(FASTRESIZE_BUILD_TESTS "Build tests" ON)
option(FASTRESIZE_BUILD_EXAMPLES "Build examples" ON)
option(FASTRESIZE_BUILD_RUBY "Build Ruby binding" ON)

# Find dependencies
find_package(PkgConfig REQUIRED)

# JPEG (libjpeg-turbo preferred)
pkg_check_modules(JPEG libjpeg)
if(NOT JPEG_FOUND)
    find_package(JPEG REQUIRED)
endif()

# PNG
pkg_check_modules(PNG REQUIRED libpng)

# WebP
pkg_check_modules(WEBP libwebp libwebpdecoder libwebpdemux)

# Source files
set(FASTRESIZE_SOURCES
    src/fastresize.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/resizer.cpp
    src/thread_pool.cpp
)

# Create library
add_library(fastresize ${FASTRESIZE_SOURCES})

target_include_directories(fastresize
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${JPEG_INCLUDE_DIRS}
        ${PNG_INCLUDE_DIRS}
        ${WEBP_INCLUDE_DIRS}
)

# Link libraries
if(FASTRESIZE_STATIC)
    # Static linking
    find_library(JPEG_STATIC_LIB NAMES libjpeg.a libturbojpeg.a)
    find_library(PNG_STATIC_LIB NAMES libpng.a libpng16.a)
    find_library(WEBP_STATIC_LIB NAMES libwebp.a)
    find_library(Z_STATIC_LIB NAMES libz.a)

    target_link_libraries(fastresize PRIVATE
        ${JPEG_STATIC_LIB}
        ${PNG_STATIC_LIB}
        ${WEBP_STATIC_LIB}
        ${Z_STATIC_LIB}
    )
else()
    target_link_libraries(fastresize PRIVATE
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
        ${WEBP_LIBRARIES}
    )
endif()

# Tests
if(FASTRESIZE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(FASTRESIZE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS fastresize
    EXPORT fastresize-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
```

---

## 🌍 Cross-Platform Strategy

### Platform-Specific Considerations

| Platform | Compiler | Arch | Notes |
|----------|----------|------|-------|
| macOS x64 | Clang | x86_64 | Use Homebrew dependencies |
| macOS ARM | Clang | arm64 | M1/M2 native, use Homebrew |
| Linux x64 | GCC | x86_64 | Use apt/yum packages |
| Linux ARM | GCC | aarch64 | Cross-compile or use QEMU |

### Static Linking Strategy

```bash
# Build script for static dependencies
#!/bin/bash
# scripts/build_static_deps.sh

PREFIX="$(pwd)/deps"
mkdir -p "$PREFIX"

# libjpeg-turbo
wget https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.5.tar.gz
tar xzf 2.1.5.tar.gz
cd libjpeg-turbo-2.1.5
cmake -B build -DCMAKE_INSTALL_PREFIX="$PREFIX" -DENABLE_SHARED=OFF
cmake --build build
cmake --install build
cd ..

# libpng
wget https://download.sourceforge.net/libpng/libpng-1.6.40.tar.gz
tar xzf libpng-1.6.40.tar.gz
cd libpng-1.6.40
./configure --prefix="$PREFIX" --enable-shared=no
make -j$(nproc)
make install
cd ..

# libwebp
wget https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.3.2.tar.gz
tar xzf libwebp-1.3.2.tar.gz
cd libwebp-1.3.2
./configure --prefix="$PREFIX" --enable-shared=no
make -j$(nproc)
make install
cd ..

echo "Static libraries built in: $PREFIX"
```

---

## ⚡ Performance Optimization

### Key Optimization Strategies

1. **Memory Management**
   - Pre-allocate output buffers
   - Reuse buffers in batch mode via buffer pool
   - Avoid unnecessary copies

2. **SIMD Utilization**
   - stb_image_resize2 auto-detects SIMD (SSE2, AVX, NEON)
   - Compile with `-march=native` for maximum SIMD usage
   - Fallback to scalar code on unsupported platforms

3. **Threading**
   - Thread pool size = CPU cores (default: 8)
   - Each image processed on separate thread
   - Decode/encode parallelized separately from resize

4. **I/O Optimization**
   - Use libjpeg-turbo (faster than libjpeg)
   - Buffered file I/O
   - Memory-mapped files for large images (future)

5. **Compiler Optimizations**
   - `-O3` for aggressive optimization
   - `-flto` for link-time optimization
   - `-march=native` for CPU-specific instructions

### Expected Performance

| Task | Images | Resolution | Expected Time |
|------|--------|------------|---------------|
| Single resize | 1 | 2000x2000 → 800x600 | < 50ms |
| Batch resize | 100 | 2000x2000 → 800x600 | < 1s |
| Batch resize | 300 | 2000x2000 → 800x600 | < 3s |
| Batch resize (huge) | 1000 | 2000x2000 → 800x600 | < 10s |

---

## 🧪 Testing Strategy

### Test Coverage

1. **Unit Tests**
   - Format detection
   - Each decoder (JPEG, PNG, WEBP, BMP)
   - Each encoder
   - Resize with different filters
   - Dimension calculation logic

2. **Integration Tests**
   - End-to-end: load → resize → save
   - All format combinations (JPG→PNG, PNG→WEBP, etc.)
   - Edge cases (1x1 pixel, 10000x10000 pixel)

3. **Batch Tests**
   - Batch with same size
   - Batch with different sizes
   - Error handling (missing files, corrupt images)

4. **Performance Tests**
   - Benchmark suite
   - Memory usage monitoring
   - Thread scaling test

### Test Framework

```cpp
// tests/test_resize.cpp
#include <gtest/gtest.h>
#include <fastresize.h>

TEST(ResizeTest, BasicResize) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    opts.target_width = 800;
    opts.target_height = 600;

    bool success = fastresize::resize("test_input.jpg", "test_output.jpg", opts);
    EXPECT_TRUE(success);

    // Verify output dimensions
    auto info = fastresize::get_image_info("test_output.jpg");
    EXPECT_EQ(info.width, 800);
    EXPECT_EQ(info.height, 600);
}

TEST(ResizeTest, AspectRatioPreserved) {
    fastresize::ResizeOptions opts;
    opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    opts.target_width = 800;
    opts.keep_aspect_ratio = true;

    // Input: 2000x1500 (4:3 ratio)
    fastresize::resize("test_4x3.jpg", "output_4x3.jpg", opts);

    auto info = fastresize::get_image_info("output_4x3.jpg");
    EXPECT_EQ(info.width, 800);
    EXPECT_EQ(info.height, 600);  // 800 * (1500/2000) = 600
}
```

---

## 🚀 Deployment

### Ruby Gem Distribution

**Strategy**: Prebuilt native extensions per platform

```ruby
# fastresize.gemspec
Gem::Specification.new do |spec|
  # ... basic info ...

  # Platform-specific gems
  if RUBY_PLATFORM =~ /darwin/
    # macOS - include prebuilt .dylib
    spec.platform = Gem::Platform::CURRENT
    spec.files += Dir["lib/fastresize/*.{dylib,bundle}"]
  elsif RUBY_PLATFORM =~ /linux/
    # Linux - include prebuilt .so
    spec.platform = Gem::Platform::CURRENT
    spec.files += Dir["lib/fastresize/*.so"]
  end
end
```

### Installation Methods

1. **Via RubyGems** (primary)
   ```bash
   gem install fastresize
   ```

2. **Via Bundler**
   ```ruby
   # Gemfile
   gem 'fastresize'
   ```

3. **From source** (for developers)
   ```bash
   git clone https://github.com/yourusername/fastresize.git
   cd fastresize
   gem build fastresize.gemspec
   gem install fastresize-*.gem
   ```

---

## 📝 Notes for Future Sessions

### When Resuming This Project:

1. **Read this file first** - Contains complete architecture and plan
2. **Check current phase** - See which phase is in progress
3. **Review dependencies** - Ensure all libraries are available
4. **Check test status** - Run tests to verify current state

### Critical Reminders:

⚠️ **License**: Must use MIT/BSD libraries for static linking
⚠️ **Static linking**: Essential for Ruby gem distribution
⚠️ **Cross-platform**: Must support macOS x64/ARM + Linux x64/ARM
⚠️ **Performance**: Target is < 3s for 300 images (2000x2000 → 800x600)
⚠️ **Thread pool**: Default 8 threads, configurable
⚠️ **Buffer reuse**: Critical for memory efficiency

### Key Files to Review:

- `CMakeLists.txt` - Build configuration
- `include/fastresize.h` - Public API
- `src/fastresize.cpp` - Main implementation
- `bindings/ruby/ext/fastresize/fastresize_ruby.cpp` - Ruby binding
- `.github/workflows/release.yml` - CI/CD pipeline

---

## 🔗 References

### Libraries Documentation

- **stb_image_resize2**: https://github.com/nothings/stb/blob/master/stb_image_resize2.h
- **libjpeg-turbo**: https://libjpeg-turbo.org/
- **libpng**: http://www.libpng.org/pub/png/libpng.html
- **libwebp**: https://developers.google.com/speed/webp

### Similar Projects (for reference)

- **fastqr** (our reference): /Users/canh.th/Desktop/fastqr
- **ImageMagick**: https://imagemagick.org/
- **vips**: https://libvips.github.io/libvips/

### Tutorials

- Ruby C Extensions: https://guides.rubygems.org/gems-with-extensions/
- CMake Tutorial: https://cmake.org/cmake/help/latest/guide/tutorial/index.html

---

**End of Architecture Document**

*This document should be updated as the project evolves.*
