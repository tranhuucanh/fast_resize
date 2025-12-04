# 💻 FastResize CLI Usage Guide

Complete command-line reference for FastResize.

---

## 📑 Table of Contents

- [Installation](#installation)
- [Basic Usage](#basic-usage)
- [Resize Modes](#resize-modes)
- [Batch Processing](#batch-processing)
- [Format Conversion](#format-conversion)
- [Options Reference](#options-reference)
- [Examples](#examples)

---

## 📦 Installation

### macOS (Homebrew)

```bash
brew tap tranhuucanh/fast_resize
brew install fast_resize
```

### Linux

```bash
# x86_64
VERSION="1.0.0"
wget https://github.com/tranhuucanh/fast_resize/releases/download/v${VERSION}/fast_resize-${VERSION}-linux-x86_64-cli.tar.gz
tar -xzf fast_resize-${VERSION}-linux-x86_64-cli.tar.gz
sudo cp fast_resize /usr/local/bin/
sudo chmod +x /usr/local/bin/fast_resize

# ARM64 (aarch64)
VERSION="1.0.0"
wget https://github.com/tranhuucanh/fast_resize/releases/download/v${VERSION}/fast_resize-${VERSION}-linux-aarch64-cli.tar.gz
tar -xzf fast_resize-${VERSION}-linux-aarch64-cli.tar.gz
sudo cp fast_resize /usr/local/bin/
sudo chmod +x /usr/local/bin/fast_resize
```

### Verify Installation

```bash
fast_resize --version
```

---

## 🎯 Basic Usage

### 🖼️ Single Image Resize

```bash
# Syntax
fast_resize <input> <output> <width> [height]

# Resize to width 800 (height auto-calculated)
fast_resize input.jpg output.jpg 800

# Resize to exact 800x600
fast_resize input.jpg output.jpg 800 600
```

### ℹ️ Get Image Info

```bash
fast_resize info image.jpg
```

Output:
```
Image: image.jpg
  Format: jpg
  Size: 1920x1080
  Channels: 3 (RGB)
```

---

## 📐 Resize Modes

### 1. ↔️ Fit Width (Auto Height)

Resize to a specific width, height is calculated automatically to maintain aspect ratio.

```bash
fast_resize input.jpg output.jpg 800
```

Example: 1920x1080 → 800x450

### 2. ↕️ Fit Height (Auto Width)

Resize to a specific height, width is calculated automatically.

```bash
fast_resize input.jpg output.jpg --height 600
```

Example: 1920x1080 → 1067x600

### 3. 📏 Exact Size

Resize to exact dimensions. Image will fit within the box while maintaining aspect ratio.

```bash
fast_resize input.jpg output.jpg 800 600
```

Example: 1920x1080 → 800x450 (fits within 800x600)

### 4. 📊 Scale Percent

Resize by a percentage.

```bash
# Scale to 50%
fast_resize input.jpg output.jpg --scale 0.5

# Scale to 200%
fast_resize input.jpg output.jpg --scale 2.0
```

---

## ⚡ Batch Processing

Process multiple images at once using multi-threaded processing.

### 📂 Batch Resize Directory

```bash
# Resize all images in directory
fast_resize batch input_dir/ output_dir/ --width 800

# With specific height
fast_resize batch input_dir/ output_dir/ --width 800 --height 600

# Scale all images to 50%
fast_resize batch input_dir/ output_dir/ --scale 0.5
```

### Batch Options

```bash
# Set number of threads (default: auto-detect)
fast_resize batch input_dir/ output_dir/ --width 800 --threads 8

# Stop on first error
fast_resize batch input_dir/ output_dir/ --width 800 --stop-on-error

# Maximum speed mode (uses more RAM)
fast_resize batch input_dir/ output_dir/ --width 800 --max-speed
```

### 📋 Batch with File List

```bash
# Create file list
cat > files.txt << EOF
/path/to/image1.jpg
/path/to/image2.png
/path/to/image3.webp
EOF

# Process from file list
fast_resize batch --file-list files.txt output_dir/ --width 800
```

---

## 🔄 Format Conversion

FastResize automatically converts formats based on output file extension.

### 📝 Supported Formats

| Format | Extensions | Read | Write |
|--------|------------|------|-------|
| JPEG | `.jpg`, `.jpeg` | ✅ | ✅ |
| PNG | `.png` | ✅ | ✅ |
| WebP | `.webp` | ✅ | ✅ |
| BMP | `.bmp` | ✅ | ✅ |

### 💡 Examples

```bash
# JPG → PNG
fast_resize input.jpg output.png 800

# PNG → WebP
fast_resize input.png output.webp 800

# WebP → JPG
fast_resize input.webp output.jpg 800
```

---

## 📖 Options Reference

### 🎨 Resize Options

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--width` | `-w` | - | Target width in pixels |
| `--height` | `-h` | - | Target height in pixels |
| `--scale` | `-s` | - | Scale factor (0.5 = 50%) |
| `--quality` | `-q` | 85 | JPEG/WebP quality (1-100) |
| `--filter` | `-f` | mitchell | Resize filter |
| `--no-aspect-ratio` | - | false | Don't maintain aspect ratio |
| `--overwrite` | `-o` | false | Overwrite input file |

### Batch Options

| Option | Default | Description |
|--------|---------|-------------|
| `--threads` | auto | Number of threads |
| `--stop-on-error` | false | Stop on first error |
| `--max-speed` | false | Enable pipeline mode |
| `--file-list` | - | Read paths from file |

### 🎯 Filter Options

| Filter | Description |
|--------|-------------|
| `mitchell` | Balanced quality/speed (default) |
| `catmull_rom` | Sharp edges, good for text |
| `triangle` | Bilinear, smooth gradients |
| `box` | Fastest, lower quality |

```bash
# Use catmull_rom for sharp images
fast_resize input.jpg output.jpg 800 --filter catmull_rom

# Use box for maximum speed
fast_resize input.jpg output.jpg 800 --filter box
```

---

## 💡 Examples

### 🔰 Basic Examples

```bash
# Resize to width 800
fast_resize photo.jpg thumbnail.jpg 800

# Resize to exact dimensions
fast_resize photo.jpg thumbnail.jpg 800 600

# High quality output
fast_resize photo.jpg output.jpg 800 --quality 95

# Maximum speed, lower quality
fast_resize photo.jpg output.jpg 800 --filter box
```

### 📦 Batch Examples

```bash
# Process all JPGs in directory
fast_resize batch photos/ thumbnails/ --width 200

# Process with 4 threads
fast_resize batch photos/ thumbnails/ --width 200 --threads 4

# Maximum speed batch processing
fast_resize batch photos/ thumbnails/ --width 200 --max-speed

# Convert all PNGs to WebP
fast_resize batch pngs/ webps/ --width 800
# (output files will have .webp extension)
```

### ⚖️ Quality vs Speed

```bash
# Highest quality (slower)
fast_resize input.jpg output.jpg 800 \
  --filter catmull_rom \
  --quality 95

# Balanced (default)
fast_resize input.jpg output.jpg 800

# Maximum speed (lower quality)
fast_resize input.jpg output.jpg 800 \
  --filter box \
  --quality 75
```

### 🔧 Scripting

```bash
# Resize all images in current directory
for img in *.jpg; do
  fast_resize "$img" "resized_$img" 800
done

# Batch with progress
fast_resize batch input/ output/ --width 800 2>&1 | tee resize.log
```

---

## ⚠️ Error Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | File not found |
| 2 | Unsupported format |
| 3 | Decode error |
| 4 | Resize error |
| 5 | Encode error |
| 6 | Write error |

---

## 🚀 Performance Tips

1. **Use `--max-speed`** for large batch jobs (uses more RAM but faster)
2. **Use `--filter box`** when quality is less important
3. **Match thread count** to your CPU cores for optimal batch performance
4. **Use WebP output** for smallest file sizes
5. **Use JPEG output** for photos (faster encoding)

---

[← Back to README](../README.md)
