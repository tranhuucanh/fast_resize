# 💎 FastResize Ruby Usage Guide

Complete Ruby integration guide for FastResize.

---

## 📑 Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Single Image Resize](#single-image-resize)
- [Batch Processing](#batch-processing)
- [Image Info](#image-info)
- [Options Reference](#options-reference)
- [Rails Integration](#rails-integration)
- [Error Handling](#error-handling)
- [Examples](#examples)

---

## 📦 Installation

### Bundler (Gemfile)

```ruby
gem 'fast_resize'
```

Then run:
```bash
bundle install
```

### Direct Install

```bash
gem install fast_resize
```

### Requirements

**When installing from RubyGems:**

- Ruby >= 2.5.0

The gem includes **pre-built binaries** for:
- ✅ macOS ARM64 (Apple Silicon)
- ✅ macOS x86_64 (Intel)
- ✅ Linux x86_64
- ✅ Linux ARM64 (aarch64)

**Note:** If your platform is not supported, the gem will automatically compile from source (requires CMake and development libraries)

---

## 🚀 Quick Start

```ruby
require 'fastresize'

# Resize single image
FastResize.resize('input.jpg', 'output.jpg', width: 800)

# Batch resize
files = Dir['images/*.jpg']
FastResize.batch_resize(files, 'thumbnails/', width: 200)
```

---

## 🖼️ Single Image Resize

### 🎯 Basic Resize

```ruby
# Resize to width (auto height)
FastResize.resize('input.jpg', 'output.jpg', width: 800)

# Resize to height (auto width)
FastResize.resize('input.jpg', 'output.jpg', height: 600)

# Resize to exact dimensions (fit within)
FastResize.resize('input.jpg', 'output.jpg', width: 800, height: 600)

# Scale by percentage
FastResize.resize('input.jpg', 'output.jpg', scale: 0.5)  # 50%
```

### ⭐ With Quality Options

```ruby
# Set JPEG/WebP quality (1-100)
FastResize.resize('input.jpg', 'output.jpg',
  width: 800,
  quality: 95
)
```

### 🎨 With Filter Options

```ruby
# Available filters: :mitchell, :catmull_rom, :box, :triangle
FastResize.resize('input.jpg', 'output.jpg',
  width: 800,
  filter: :catmull_rom  # Sharp edges, good for text
)
```

### 🔄 Format Conversion

```ruby
# Convert JPG to PNG
FastResize.resize('input.jpg', 'output.png', width: 800)

# Convert PNG to WebP
FastResize.resize('input.png', 'output.webp', width: 800)

# Explicit format
FastResize.resize_with_format('input.jpg', 'output', 'png',
  width: 800
)
```

### ♻️ Overwrite Input

```ruby
# Resize and overwrite original file
FastResize.resize('photo.jpg', 'photo.jpg',
  width: 800,
  overwrite: true
)
```

---

## ⚡ Batch Processing

### 📦 Basic Batch Resize

```ruby
# Get all images
files = Dir['images/*.jpg']

# Resize all to width 800
result = FastResize.batch_resize(files, 'output/', width: 800)

puts "Total: #{result[:total]}"
puts "Success: #{result[:success]}"
puts "Failed: #{result[:failed]}"
result[:errors].each { |e| puts "Error: #{e}" }
```

### ⚙️ Batch with Options

```ruby
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  height: 600,
  quality: 90,
  filter: :mitchell
)
```

### 🧵 Thread Control

```ruby
# Use 8 threads
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  threads: 8
)

# Auto-detect thread count (default)
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  threads: 0  # auto
)
```

### 🚄 Maximum Speed Mode

Enable pipeline mode for faster processing (uses more RAM):

```ruby
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  max_speed: true
)
```

### ⚠️ Stop on Error

```ruby
result = FastResize.batch_resize(files, 'output/',
  width: 800,
  stop_on_error: true
)
```

### 🎯 Custom Batch (Per-Image Options)

```ruby
items = [
  { input: 'photo1.jpg', output: 'thumb1.jpg', width: 200, height: 200 },
  { input: 'photo2.jpg', output: 'thumb2.jpg', width: 400 },
  { input: 'photo3.png', output: 'thumb3.webp', width: 300, quality: 95 }
]

result = FastResize.batch_resize_custom(items)
```

---

## ℹ️ Image Info

Get image information without loading the full image:

```ruby
info = FastResize.image_info('photo.jpg')

puts "Width: #{info[:width]}"
puts "Height: #{info[:height]}"
puts "Channels: #{info[:channels]}"  # 1=Gray, 3=RGB, 4=RGBA
puts "Format: #{info[:format]}"      # "jpg", "png", "webp", "bmp"
```

---

## 📖 Options Reference

### 🎨 Resize Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | Integer | nil | Target width in pixels |
| `height` | Integer | nil | Target height in pixels |
| `scale` | Float | nil | Scale factor (0.5 = 50%) |
| `quality` | Integer | 85 | JPEG/WebP quality (1-100) |
| `keep_aspect_ratio` | Boolean | true | Maintain aspect ratio |
| `overwrite` | Boolean | false | Allow overwriting input file |
| `filter` | Symbol | `:mitchell` | Resize filter algorithm |

### ⚡ Batch Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `threads` | Integer | 0 | Number of threads (0 = auto) |
| `stop_on_error` | Boolean | false | Stop on first error |
| `max_speed` | Boolean | false | Enable pipeline mode |

### 🎯 Filter Options

| Filter | Description | Use Case |
|--------|-------------|----------|
| `:mitchell` | Balanced quality/speed | General use (default) |
| `:catmull_rom` | Sharp, high quality | Text, logos, sharp edges |
| `:triangle` | Bilinear interpolation | Smooth gradients |
| `:box` | Fastest, lower quality | Thumbnails, previews |

---

## 🛤️ Rails Integration

### 🎮 Basic Usage in Controller

```ruby
class ImagesController < ApplicationController
  def resize
    uploaded = params[:image]

    # Save original
    original_path = Rails.root.join('storage', 'original.jpg')
    File.binwrite(original_path, uploaded.read)

    # Create thumbnail
    thumb_path = Rails.root.join('storage', 'thumb.jpg')
    FastResize.resize(original_path.to_s, thumb_path.to_s, width: 200)

    render json: { status: 'ok' }
  end
end
```

### 💾 Active Storage Integration

```ruby
class ImageProcessor
  def self.process(blob)
    blob.open do |file|
      # Create variants
      thumb_path = "/tmp/thumb_#{blob.id}.jpg"
      medium_path = "/tmp/medium_#{blob.id}.jpg"

      FastResize.resize(file.path, thumb_path, width: 200, height: 200)
      FastResize.resize(file.path, medium_path, width: 800)

      # Upload variants...
    end
  end
end
```

### 🔄 Background Job (Sidekiq)

```ruby
class ImageResizeJob
  include Sidekiq::Job

  def perform(image_paths, output_dir, options = {})
    result = FastResize.batch_resize(
      image_paths,
      output_dir,
      **options.symbolize_keys
    )

    Rails.logger.info "Resized #{result[:success]}/#{result[:total]} images"
  end
end

# Usage
ImageResizeJob.perform_async(
  Dir[Rails.root.join('uploads/*.jpg')],
  Rails.root.join('thumbnails').to_s,
  { width: 200, quality: 85 }
)
```

---

## ⚠️ Error Handling

### 🛡️ Basic Error Handling

```ruby
begin
  FastResize.resize('input.jpg', 'output.jpg', width: 800)
rescue FastResize::Error => e
  puts "Resize failed: #{e.message}"
rescue => e
  puts "Unexpected error: #{e.message}"
end
```

### 📦 Batch Error Handling

```ruby
result = FastResize.batch_resize(files, 'output/', width: 800)

if result[:failed] > 0
  puts "#{result[:failed]} images failed:"
  result[:errors].each do |error|
    puts "  - #{error}"
  end
end
```

### ✅ Validation Before Resize

```ruby
def safe_resize(input, output, options)
  # Check if file exists
  unless File.exist?(input)
    raise FastResize::Error, "Input file not found: #{input}"
  end

  # Get info and validate
  info = FastResize.image_info(input)
  if info[:width] == 0
    raise FastResize::Error, "Invalid image: #{input}"
  end

  # Perform resize
  FastResize.resize(input, output, **options)
end
```

---

## 💡 Examples

### 🖼️ Create Thumbnails

```ruby
# Single thumbnail
FastResize.resize('photo.jpg', 'thumb.jpg',
  width: 200,
  height: 200,
  filter: :box  # Fast for thumbnails
)
```

### 📐 Create Multiple Sizes

```ruby
def create_variants(input)
  base = File.basename(input, '.*')
  ext = File.extname(input)
  dir = File.dirname(input)

  variants = {
    thumb: { width: 100, height: 100 },
    small: { width: 320 },
    medium: { width: 800 },
    large: { width: 1920 }
  }

  variants.each do |name, opts|
    output = File.join(dir, "#{base}_#{name}#{ext}")
    FastResize.resize(input, output, **opts)
  end
end

create_variants('photo.jpg')
```

### 📊 Batch Process with Progress

```ruby
require 'fastresize'

files = Dir['images/**/*.{jpg,png,webp}']
total = files.size
processed = 0

# Process in chunks for progress reporting
files.each_slice(100) do |chunk|
  result = FastResize.batch_resize(chunk, 'output/',
    width: 800,
    max_speed: true
  )

  processed += chunk.size
  percent = (processed.to_f / total * 100).round(1)
  puts "Progress: #{percent}% (#{processed}/#{total})"
end
```

### 🌐 Convert Directory to WebP

```ruby
jpgs = Dir['images/*.jpg']

items = jpgs.map do |jpg|
  {
    input: jpg,
    output: jpg.sub('.jpg', '.webp'),
    width: 800,
    quality: 85
  }
end

result = FastResize.batch_resize_custom(items, max_speed: true)
puts "Converted #{result[:success]} images to WebP"
```

### 💾 Memory-Efficient Large Batch

```ruby
# Process in smaller batches to control memory
def process_large_batch(files, output_dir, options = {})
  batch_size = options.delete(:batch_size) || 500
  total_success = 0
  total_failed = 0
  all_errors = []

  files.each_slice(batch_size) do |batch|
    result = FastResize.batch_resize(batch, output_dir, **options)
    total_success += result[:success]
    total_failed += result[:failed]
    all_errors.concat(result[:errors])
  end

  {
    total: files.size,
    success: total_success,
    failed: total_failed,
    errors: all_errors
  }
end

files = Dir['massive_dataset/**/*.jpg']  # 100,000 files
result = process_large_batch(files, 'output/',
  width: 800,
  batch_size: 1000,
  max_speed: true
)
```

---

## 🚀 Performance Tips

1. **Use `max_speed: true`** for large batches
2. **Use `:box` filter** for thumbnails (fastest)
3. **Use `:catmull_rom`** for high-quality resizes
4. **Process in batches** instead of one-by-one
5. **Match thread count** to CPU cores
6. **Use WebP output** for smallest file sizes

---

[← Back to README](../README.md)
