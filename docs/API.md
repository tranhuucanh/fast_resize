# 📚 FastResize API Reference

Complete API documentation for FastResize library.

---

## 📑 Table of Contents

- [Ruby API](#ruby-api)
  - [Single Image Resize](#single-image-resize)
  - [Batch Processing](#batch-processing)
  - [Image Information](#image-information)
- [C++ API](#c-api)
  - [Core Functions](#core-functions)
  - [Data Structures](#data-structures)
  - [Error Handling](#error-handling)
- [Resize Options](#resize-options)
- [Filter Types](#filter-types)
- [Format Support](#format-support)
- [Error Codes](#error-codes)

---

## 💎 Ruby API

### 🖼️ Single Image Resize

#### `FastResize.resize(input, output, options = {})`

Resize a single image file.

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `input` | String | Yes | Path to input image file |
| `output` | String | Yes | Path to output image file |
| `options` | Hash | No | Resize options (see [Resize Options](#resize-options)) |

**Returns:** `true` on success

**Raises:** `FastResize::Error` on failure

**Examples:**

```ruby
# Resize to width 800 (auto height)
FastResize.resize('input.jpg', 'output.jpg', width: 800)

# Resize to exact dimensions
FastResize.resize('input.jpg', 'output.jpg', width: 800, height: 600)

# With quality and filter
FastResize.resize('input.jpg', 'output.jpg',
  width: 800,
  quality: 95,
  filter: :catmull_rom
)

# Scale by percentage
FastResize.resize('input.jpg', 'output.jpg', scale: 0.5)  # 50%
```

---

### ⚡ Batch Processing

#### `FastResize.batch_resize(files, output_dir, options = {})`

Resize multiple image files in parallel.

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `files` | Array\<String\> | Yes | Array of input file paths |
| `output_dir` | String | Yes | Directory for output files |
| `options` | Hash | No | Resize and batch options |

**Returns:** Hash with results:

```ruby
{
  total: 100,        # Total number of files
  success: 98,       # Successfully processed
  failed: 2,         # Failed to process
  errors: ["..."]    # Array of error messages
}
```

**Examples:**

```ruby
files = Dir['images/*.jpg']
result = FastResize.batch_resize(files, 'thumbnails/',
  width: 200,
  threads: 8,
  max_speed: true
)

puts "Processed: #{result[:success]}/#{result[:total]}"
```

---

#### `FastResize.batch_resize_custom(items, options = {})`

Batch resize with per-image options.

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `items` | Array\<Hash\> | Yes | Array of resize specifications |
| `options` | Hash | No | Global batch options |

**Item Hash Format:**

```ruby
{
  input: 'path/to/input.jpg',
  output: 'path/to/output.jpg',
  width: 800,
  height: 600,
  quality: 90,
  filter: :mitchell
}
```

**Examples:**

```ruby
items = [
  { input: 'photo1.jpg', output: 'thumb1.jpg', width: 200 },
  { input: 'photo2.png', output: 'thumb2.webp', width: 400, quality: 95 }
]

result = FastResize.batch_resize_custom(items, max_speed: true)
```

---

### ℹ️ Image Information

#### `FastResize.image_info(path)`

Get image metadata without loading the full image.

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `path` | String | Yes | Path to image file |

**Returns:** Hash with image information:

```ruby
{
  width: 1920,      # Image width in pixels
  height: 1080,     # Image height in pixels
  channels: 3,      # Number of channels (1=Gray, 3=RGB, 4=RGBA)
  format: "jpg"     # Image format ("jpg", "png", "webp", "bmp")
}
```

**Examples:**

```ruby
info = FastResize.image_info('photo.jpg')
puts "#{info[:width]}x#{info[:height]}"  # => "1920x1080"
```

---

## ⚙️ C++ API

### 🔧 Core Functions

#### `resize_image()`

```cpp
bool resize_image(
    const std::string& input_path,
    const std::string& output_path,
    const ResizeOptions& options
);
```

Resize a single image file.

**Parameters:**
- `input_path`: Path to input image
- `output_path`: Path to output image
- `options`: Resize options (see [ResizeOptions](#resizeoptions))

**Returns:** `true` on success, `false` on failure

---

#### `batch_resize()`

```cpp
BatchResult batch_resize(
    const std::vector<std::string>& input_files,
    const std::string& output_dir,
    const ResizeOptions& options,
    int num_threads = 0,
    bool stop_on_error = false
);
```

Batch resize multiple images in parallel.

**Parameters:**
- `input_files`: Vector of input file paths
- `output_dir`: Output directory path
- `options`: Resize options
- `num_threads`: Number of threads (0 = auto-detect)
- `stop_on_error`: Stop processing on first error

**Returns:** `BatchResult` struct with processing results

---

#### `get_image_info()`

```cpp
ImageInfo get_image_info(const std::string& path);
```

Get image metadata without loading the full image.

**Returns:** `ImageInfo` struct with image properties

---

### 📊 Data Structures

#### `ResizeOptions`

```cpp
struct ResizeOptions {
    enum Mode {
        FIT_WIDTH,      // Resize to width, auto height
        FIT_HEIGHT,     // Resize to height, auto width
        EXACT_SIZE,     // Resize to exact dimensions
        SCALE_PERCENT   // Scale by percentage
    };

    enum Filter {
        MITCHELL,       // Balanced quality/speed (default)
        CATMULL_ROM,    // Sharp, high quality
        BOX,            // Fast, lower quality
        TRIANGLE        // Bilinear interpolation
    };

    Mode mode = FIT_WIDTH;
    Filter filter = MITCHELL;

    int target_width = 0;
    int target_height = 0;
    float scale_percent = 1.0f;

    int quality = 85;              // JPEG/WebP quality (1-100)
    bool keep_aspect_ratio = true;
};
```

---

#### `ImageInfo`

```cpp
struct ImageInfo {
    int width;        // Image width in pixels
    int height;       // Image height in pixels
    int channels;     // Number of channels (1, 3, or 4)
    std::string format;  // "jpg", "png", "webp", "bmp"
};
```

---

#### `BatchResult`

```cpp
struct BatchResult {
    int total;                        // Total files processed
    int success;                      // Successfully processed
    int failed;                       // Failed to process
    std::vector<std::string> errors;  // Error messages
};
```

---

### ⚠️ Error Handling

FastResize uses exceptions for error handling in C++:

```cpp
try {
    resize_image("input.jpg", "output.jpg", options);
} catch (const std::runtime_error& e) {
    std::cerr << "Resize failed: " << e.what() << std::endl;
}
```

Common exceptions:
- `std::runtime_error` - General resize/decode/encode errors
- `std::invalid_argument` - Invalid parameters
- `std::ios_base::failure` - File I/O errors

---

## 🎨 Resize Options

### 📐 Dimension Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | Integer | - | Target width in pixels |
| `height` | Integer | - | Target height in pixels |
| `scale` | Float | - | Scale factor (0.5 = 50%, 2.0 = 200%) |
| `keep_aspect_ratio` | Boolean | `true` | Maintain aspect ratio when both width and height are specified |

### 🔄 Resize Modes

The resize mode is automatically determined from the provided options:

| Mode | When Used | Example | Result |
|------|-----------|---------|--------|
| **FIT_WIDTH** | Only `width` specified | `width: 800` | Resizes to width 800, height auto-calculated |
| **FIT_HEIGHT** | Only `height` specified | `height: 600` | Resizes to height 600, width auto-calculated |
| **EXACT_SIZE** | Both `width` and `height` | `width: 800, height: 600` | Fits within 800x600 box |
| **SCALE_PERCENT** | `scale` specified | `scale: 0.5` | Scales to 50% of original size |

**Examples:**

```ruby
# FIT_WIDTH: 1920x1080 → 800x450
FastResize.resize('input.jpg', 'output.jpg', width: 800)

# FIT_HEIGHT: 1920x1080 → 1067x600
FastResize.resize('input.jpg', 'output.jpg', height: 600)

# EXACT_SIZE: 1920x1080 → 800x450 (fits within 800x600)
FastResize.resize('input.jpg', 'output.jpg', width: 800, height: 600)

# SCALE_PERCENT: 1920x1080 → 960x540
FastResize.resize('input.jpg', 'output.jpg', scale: 0.5)
```

---

### ⭐ Quality Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `quality` | Integer | 85 | Output quality for JPEG and WebP (1-100) |

Higher quality = larger file size, better image quality.

**Quality Guidelines:**

| Quality | Use Case | File Size |
|---------|----------|-----------|
| 95-100 | Professional photography | Very large |
| 85-94 | High quality web images | Large |
| 75-84 | Standard web images | Medium |
| 60-74 | Thumbnails, previews | Small |
| 1-59 | Placeholders, ultra-compressed | Very small |

---

### 🎯 Filter Options

| Filter | Ruby Symbol | C++ Enum | Description |
|--------|-------------|----------|-------------|
| **Mitchell** | `:mitchell` | `MITCHELL` | Balanced quality and speed (default) |
| **Catmull-Rom** | `:catmull_rom` | `CATMULL_ROM` | Sharp edges, high quality |
| **Triangle** | `:triangle` | `TRIANGLE` | Bilinear interpolation, smooth |
| **Box** | `:box` | `BOX` | Fastest, lower quality |

**Filter Comparison:**

| Filter | Quality | Speed | Best For |
|--------|---------|-------|----------|
| `:mitchell` | ★★★★☆ | ★★★☆☆ | General purpose, balanced |
| `:catmull_rom` | ★★★★★ | ★★☆☆☆ | Text, logos, sharp edges |
| `:triangle` | ★★★☆☆ | ★★★★☆ | Smooth gradients, photos |
| `:box` | ★★☆☆☆ | ★★★★★ | Thumbnails, maximum speed |

**Examples:**

```ruby
# High quality, sharp edges
FastResize.resize('input.jpg', 'output.jpg',
  width: 800,
  filter: :catmull_rom
)

# Maximum speed
FastResize.resize('input.jpg', 'output.jpg',
  width: 200,
  filter: :box
)
```

---

### ⚡ Batch Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `threads` | Integer | 0 | Number of threads (0 = auto-detect CPU cores) |
| `stop_on_error` | Boolean | `false` | Stop processing on first error |
| `max_speed` | Boolean | `false` | Enable pipeline mode (faster, uses more RAM) |

**`max_speed` Mode:**

When enabled, uses a pipelined processing approach:
- **Faster**: Up to 30% faster for large batches
- **More RAM**: Uses ~2x more RAM due to buffering
- **Best for**: Large batches (100+ images) with available RAM

**Examples:**

```ruby
# Auto-detect threads
result = FastResize.batch_resize(files, 'output/', width: 800)

# Use 8 threads explicitly
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  threads: 8
)

# Maximum speed mode
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  max_speed: true
)

# Stop on first error
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  stop_on_error: true
)
```

---

### 📁 File Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `overwrite` | Boolean | `false` | Allow overwriting input file |

**Example:**

```ruby
# Resize and overwrite original
FastResize.resize('photo.jpg', 'photo.jpg',
  width: 800,
  overwrite: true
)
```

---

## 🎨 Filter Types

FastResize uses the high-quality `stb_image_resize2` library with SIMD optimization.

### 🔷 Mitchell Filter (Default)

**Characteristics:**
- Balanced quality and performance
- Good for general-purpose resizing
- Mitchell-Netravali cubic filter

**Best for:** Photos, general images, when you want good quality without sacrificing speed

---

### ⚡ Catmull-Rom Filter

**Characteristics:**
- Sharpest results
- Highest quality
- Slower than Mitchell
- Catmull-Rom cubic interpolation

**Best for:** Images with text, logos, diagrams, screenshots, images where sharpness matters

---

### 🔺 Triangle Filter (Bilinear)

**Characteristics:**
- Smooth results
- Good for gradients
- Faster than cubic filters
- Bilinear interpolation

**Best for:** Photos with smooth gradients, artistic images, when smoothness is preferred

---

### ⬜ Box Filter

**Characteristics:**
- Fastest resize
- Lower quality than other filters
- Box (nearest neighbor) sampling

**Best for:** Thumbnails, previews, when speed is critical and quality is less important

---

## 🖼️ Format Support

### 📥 Supported Input Formats

| Format | Extensions | Decoder | Features |
|--------|------------|---------|----------|
| **JPEG** | `.jpg`, `.jpeg` | libjpeg-turbo | SIMD-accelerated, progressive support |
| **PNG** | `.png` | libpng | All PNG types, transparency support |
| **WebP** | `.webp` | libwebp | Lossy and lossless, transparency |
| **BMP** | `.bmp` | stb_image | All BMP variants |

### 📤 Supported Output Formats

| Format | Extensions | Encoder | Quality Control |
|--------|------------|---------|-----------------|
| **JPEG** | `.jpg`, `.jpeg` | libjpeg-turbo | Yes (1-100) |
| **PNG** | `.png` | libpng | No (lossless) |
| **WebP** | `.webp` | libwebp | Yes (1-100) |
| **BMP** | `.bmp` | stb_image_write | No (lossless) |

### 🔍 Format Auto-Detection

FastResize automatically detects input format from file contents (not extension) and output format from file extension:

```ruby
# Auto-detect input, output as PNG
FastResize.resize('photo.jpg', 'output.png', width: 800)

# Auto-detect input, output as WebP
FastResize.resize('image.png', 'output.webp', width: 800)
```

---

## ⚠️ Error Codes

### 💎 Ruby Errors

All Ruby errors are raised as `FastResize::Error` with descriptive messages:

```ruby
begin
  FastResize.resize('input.jpg', 'output.jpg', width: 800)
rescue FastResize::Error => e
  puts "Error: #{e.message}"
end
```

**Common error messages:**

| Error | Cause |
|-------|-------|
| `"File not found: ..."` | Input file does not exist |
| `"Unsupported format: ..."` | Image format not supported |
| `"Failed to decode image: ..."` | Corrupted or invalid image |
| `"Failed to resize image: ..."` | Resize operation failed |
| `"Failed to encode image: ..."` | Output encoding failed |
| `"Width must be positive"` | Invalid width value |
| `"Height must be positive"` | Invalid height value |
| `"Quality must be between 1 and 100"` | Invalid quality value |
| `"Scale must be positive"` | Invalid scale value |

---

### ⚙️ C++ Error Handling

C++ API uses exceptions:

```cpp
#include <fastresize.h>

try {
    fastresize::ResizeOptions opts;
    opts.target_width = 800;
    fastresize::resize_image("input.jpg", "output.jpg", opts);
} catch (const std::runtime_error& e) {
    std::cerr << "Resize failed: " << e.what() << std::endl;
}
```

---

## 🚀 Performance Characteristics

### ⏱️ Time Complexity

- **Single resize:** O(W×H×C) where W=width, H=height, C=channels
- **Batch resize:** O(N×W×H×C / T) where N=number of images, T=threads

### 💾 Memory Usage

- **Normal mode:** ~3x input image size
- **Pipeline mode (`max_speed: true`):** ~5-6x input image size

### 📊 Throughput Examples

Based on MacBook Pro M2:

| Format | Image Size | Throughput |
|--------|------------|------------|
| JPG | 1179×1409 | ~477 images/sec |
| PNG | 2104×1160 | ~292 images/sec |
| BMP | 1045×1045 | ~606 images/sec |
| WebP | 4275×2451 | ~71 images/sec |

---

[← Back to README](../README.md)
