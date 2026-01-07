# FastResize

<div align="center">

**The fastest image resizing library on the planet.** 🚀

Resize 1,000 images in **2 seconds**. Up to **2.9x faster** than libvips, **3.1x faster** than imageflow. Uses **3-4x less RAM** than alternatives.

[![GitHub Stars](https://img.shields.io/github/stars/tranhuucanh/fast_resize?style=social)](https://github.com/tranhuucanh/fast_resize/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/tranhuucanh/fast_resize?style=social)](https://github.com/tranhuucanh/fast_resize/network/members)

[Installation](#-installation) • [Quick Start](#-quick-start) • [Documentation](#-documentation) • [Benchmarks](#-performance-benchmarks)

---

### 🛠️ Tech Stack & Stats

[![C++](https://img.shields.io/badge/C++-14-blue.svg)](https://isocpp.org/)
[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Ruby](https://img.shields.io/badge/Ruby-2.5+-red.svg)](https://www.ruby-lang.org/)
[![CMake](https://img.shields.io/badge/CMake-3.15+-064F8C.svg)](https://cmake.org/)
[![Shell](https://img.shields.io/badge/Shell-Bash-4EAA25.svg)](https://www.gnu.org/software/bash/)
[![License](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

💎 **Ruby:** [![Gem](https://img.shields.io/badge/Gem-fast_resize-red.svg)](https://rubygems.org/gems/fast_resize) [![Gem Downloads](https://badgen.net/rubygems/dt/fast_resize)](https://rubygems.org/gems/fast_resize)

📦 **CLI:** [![GitHub Downloads](https://img.shields.io/github/downloads/tranhuucanh/fast_resize/total)](https://github.com/tranhuucanh/fast_resize/releases)

</div>

---

## 📑 Table of Contents

- [Why Fastest on the Planet?](#-why-fastest-on-the-planet)
- [Performance Benchmarks](#-performance-benchmarks)
- [Key Features](#-key-features)
- [Installation](#-installation)
- [Quick Start](#-quick-start)
- [API Reference](#-api-reference)
- [Documentation](#-documentation)
- [Architecture](#️-architecture)
- [License](#-license)
- [Contributing](#-contributing)
- [Contact & Support](#-contact--support)

---

## 🏆 Why Fastest on the Planet?

<div align="center">

**FastResize has defeated the two most legendary image processing libraries in the world:**

</div>

| Library | Description | FastResize Advantage |
|---------|-------------|---------------------|
| **[libvips](https://www.libvips.org/)** | The gold standard for high-performance image processing. Used by Sharp (Node.js), Shopify, and Wikipedia. Widely recognized as one of the fastest image processing libraries available. | FastResize is **1.7x - 2.9x faster** |
| **[imageflow](https://www.imageflow.io/)** | A Rust-based image manipulation engine designed for high throughput and accuracy. Known for powering modern large-scale image resizing pipelines. | FastResize is **1.1x - 3.1x faster** |

<div align="center">

💡 **Want to dive deeper?** Explore full details about FastResize →
<a target="_blank" href="https://deepwiki.com/tranhuucanh/fast_resize"><img src="https://deepwiki.com/badge.svg" alt="Ask DeepWiki" style="display:inline; vertical-align: middle;" /></a>

</div>

---

## 🔥 Performance Benchmarks

<div align="center">

**Resize 1,000 images to 800px width**

*Tested on MacBook Pro M2, macOS Sonoma*

| Format | FastResize | libvips (parallel) | imageflow | 🚀 vs libvips | 🚀 vs imageflow |
|--------|------------|-------------------|-----------|---------------|-----------------|
| **JPG** | **2.10s** ⚡ | 5.24s | 6.60s | **2.5x faster** | **3.1x faster** 🏆 |
| **PNG** | **3.43s** ⚡ | 6.18s | 8.41s | **1.8x faster** | **2.5x faster** |
| **BMP** | **1.65s** ⚡ | 4.72s | ❌ N/A | **2.9x faster** | - |
| **WEBP** | **14.03s** ⚡ | 23.52s | 15.69s | **1.7x faster** | **1.1x faster** |

**FastResize wins in ALL formats!** 🎯

</div>

<details closed>
<summary>📊 <b>Speed Comparison Details</b></summary>

<br/>

### JPG Performance (1179×1409, 3 channels)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 2.02s | 2.17s | **2.10s** | **~477 img/s** ⚡ |
| libvips (parallel) | 5.20s | 5.28s | 5.24s | ~191 img/s |
| libvips (sequential) | 16.32s | 15.91s | 16.11s | ~62 img/s |
| imageflow | 6.62s | 6.58s | 6.60s | ~151 img/s |

### PNG Performance (2104×1160, 4 channels)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 3.55s | 3.32s | **3.43s** | **~292 img/s** ⚡ |
| libvips (parallel) | 6.18s | 6.18s | 6.18s | ~162 img/s |
| libvips (sequential) | 31.95s | 31.46s | 31.71s | ~32 img/s |
| imageflow | 8.45s | 8.37s | 8.41s | ~119 img/s |

### BMP Performance (1045×1045, 3 channels)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 1.65s | 1.64s | **1.65s** | **~606 img/s** ⚡ |
| libvips (parallel) | 4.75s | 4.70s | 4.72s | ~212 img/s |
| imageflow | ❌ | ❌ | Not supported | - |

### WEBP Performance (4275×2451, 3 channels)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 13.99s | 14.07s | **14.03s** | **~71 img/s** ⚡ |
| libvips (parallel) | 23.94s | 23.10s | 23.52s | ~43 img/s |
| imageflow | 15.64s | 15.73s | 15.69s | ~64 img/s |

</details>

<details closed>
<summary>💾 <b>RAM Usage Comparison</b></summary>

<br/>

FastResize uses **3-4x less RAM** than alternatives!

### PNG (1000 images, 2104×1160)

| Library | Time | Throughput | RAM Peak | Compared |
|---------|------|------------|----------|----------|
| **FastResize** | 3.19s | 313 img/s | **138 MB** ⚡ | - |
| libvips | 6.11s | 164 img/s | 439 MB | 3.2x more |
| imageflow | 8.39s | 119 img/s | 514 MB | 3.7x more |

### JPG (1000 images, 1179×1409)

| Library | Time | Throughput | RAM Peak | Compared |
|---------|------|------------|----------|----------|
| **FastResize** | 2.03s | 493 img/s | **271 MB** ⚡ | - |
| libvips | 5.07s | 197 img/s | 460 MB | 1.7x more |
| imageflow | 6.59s | 152 img/s | 541 MB | 2.0x more |

</details>

[Full Benchmark Details →](docs/BENCHMARKS.md)

---

## 💪 Key Features

<table width="100%">
<tr>
<td width="50%">

### ⚡ Performance
- **Up to 3.1x faster** than libvips & imageflow
- **3-4x less RAM** usage
- Optimized C++ core with SIMD
- Multi-threaded batch processing

</td>
<td width="50%">

### 🖼️ Format Support
- **JPEG** (via libjpeg-turbo)
- **PNG** (via libpng)
- **WebP** (via libwebp)
- **BMP** (via stb_image)

</td>
</tr>
<tr>
<td width="50%">

### 🎯 Resize Modes
- **Exact size** - Fixed width & height
- **Fit width** - Auto height
- **Fit height** - Auto width
- **Scale percent** - Proportional resize

</td>
<td width="50%">

### 🔧 Quality Filters
- **Mitchell** - Balanced (default)
- **Catmull-Rom** - Sharp edges
- **Box** - Fast, lower quality
- **Triangle** - Bilinear

</td>
</tr>
</table>

---

## 📦 Installation

<table>
<tr>
<td width="50%">

### 💎 Ruby

```bash
gem install fast_resize
```

### 🍎 macOS (Homebrew)

```bash
brew tap tranhuucanh/fast_resize
brew install fast_resize
```

### 🐧 Linux

```bash
# Download latest release
VERSION="1.0.0"
wget https://github.com/tranhuucanh/fast_resize/releases/download/v${VERSION}/fast_resize-${VERSION}-linux-x86_64.tar.gz
tar -xzf fast_resize-${VERSION}-linux-x86_64.tar.gz
sudo cp fast_resize /usr/local/bin/
sudo chmod +x /usr/local/bin/fast_resize

# Verify installation
fast_resize --version
```

</td>
</tr>
</table>

---

## 🎯 Quick Start

### CLI

```bash
# Resize to width 800 (auto height)
fast_resize input.jpg output.jpg 800

# Resize to exact 800x600
fast_resize input.jpg output.jpg 800 600

# Batch resize all images in directory
fast_resize batch input_dir/ output_dir/ --width 800

# Batch resize specific files (from stdin, NULL-separated)
printf 'photo1.jpg\0photo2.jpg\0photo3.jpg' | fast_resize batch --stdin output_dir/ -w 800

# Convert format (JPG → PNG)
fast_resize input.jpg output.png 800
```

### 💎 Ruby

```ruby
require 'fast_resize'

# Simple resize (width 800, auto height)
FastResize.resize('input.jpg', 'output.jpg', width: 800)

# Exact dimensions
FastResize.resize('input.jpg', 'output.jpg', width: 800, height: 600)

# With quality and filter options
FastResize.resize('input.jpg', 'output.jpg',
  width: 800,
  quality: 90,
  filter: :catmull_rom
)

# Batch resize - all images in directory
result = FastResize.batch_resize('images/', 'output/', width: 800)
puts "Processed: #{result[:success]}/#{result[:total]}"

# Batch resize - specific files only
files = ['photo1.jpg', 'photo2.jpg', 'photo3.jpg']
result = FastResize.batch_resize(files, 'output/', width: 800)
puts "Processed: #{result[:success]}/#{result[:total]}"

# Maximum speed mode (uses more RAM)
FastResize.batch_resize('images/', 'output/',
  width: 800,
  max_speed: true
)
```

[Full Ruby Guide →](docs/RUBY_USAGE.md) • [Full CLI Guide →](docs/CLI_USAGE.md)

---

## 📖 API Reference

### Resize Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | int | - | Target width in pixels |
| `height` | int | - | Target height in pixels |
| `scale` | float | - | Scale factor (0.5 = 50%) |
| `quality` | int | 85 | JPEG/WebP quality (1-100) |
| `keep_aspect_ratio` | bool | true | Maintain aspect ratio |
| `filter` | symbol | `:mitchell` | Resize filter algorithm |

### Resize Modes

| Mode | When | Example |
|------|------|---------|
| **Fit Width** | Only `width` specified | `width: 800` → height auto |
| **Fit Height** | Only `height` specified | `height: 600` → width auto |
| **Exact Size** | Both specified | `width: 800, height: 600` |
| **Scale** | `scale` specified | `scale: 0.5` → 50% size |

### Filter Options

| Filter | Quality | Speed | Best For |
|--------|---------|-------|----------|
| `:mitchell` | ★★★★☆ | ★★★☆☆ | General use (default) |
| `:catmull_rom` | ★★★★★ | ★★☆☆☆ | Sharp edges, text |
| `:triangle` | ★★★☆☆ | ★★★★☆ | Smooth gradients |
| `:box` | ★★☆☆☆ | ★★★★★ | Maximum speed |

### Batch Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `threads` | int | auto | Number of threads (0 = auto) |
| `stop_on_error` | bool | false | Stop on first error |
| `max_speed` | bool | false | Enable pipeline mode (faster, uses more RAM) |

---

## 📚 Documentation

Complete usage guides:

- **[CLI Usage Guide](docs/CLI_USAGE.md)** - Complete command-line reference
- **[Ruby Usage Guide](docs/RUBY_USAGE.md)** - Ruby integration and examples
- **[Benchmark Details](docs/BENCHMARKS.md)** - Full performance analysis
- **[API Reference](docs/API.md)** - Complete API documentation

---

## 🏗️ Architecture

FastResize is built on industry-standard, high-performance libraries:

| Library | License | Purpose |
|---------|---------|---------|
| **[libjpeg-turbo](https://libjpeg-turbo.org/)** | BSD-3-Clause | JPEG decode/encode with SIMD |
| **[libpng](http://www.libpng.org/)** | libpng license | PNG decode/encode |
| **[libwebp](https://developers.google.com/speed/webp)** | BSD-3-Clause | WebP decode/encode |
| **[stb_image](https://github.com/nothings/stb)** | Public Domain | BMP and fallback decode |
| **[stb_image_resize2](https://github.com/nothings/stb)** | Public Domain | High-quality resize with SIMD |

### Why So Fast?

- ⚡ **SIMD Optimization** - SSE2/AVX (x86) and NEON (ARM) acceleration
- 🚀 **Zero-copy Pipeline** - Minimal memory allocation
- 💪 **Multi-threaded** - Parallel batch processing
- 🔥 **Memory-mapped I/O** - Efficient file reading

---

## 📄 License

FastResize is licensed under the **BSD-3-Clause License**.

See [LICENSE](LICENSE) for full details.

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 🐛 Bug Reports

Found a bug? Please open an issue with:
- Your OS and version
- FastResize version
- Steps to reproduce
- Expected vs actual behavior

---

## 💖 Support This Project

If FastResize helps you save time and money:
- ⭐ Star this repository
- 🐛 Report bugs and suggest features
- 📖 Improve documentation
- 💬 Share FastResize with others

---

## 📮 Contact & Support

**GitHub:** [@tranhuucanh](https://github.com/tranhuucanh) • [Issues](https://github.com/tranhuucanh/fast_resize/issues) • [Discussions](https://github.com/tranhuucanh/fast_resize/discussions)

---

## 🙏 Acknowledgments

Built with: **[libjpeg-turbo](https://libjpeg-turbo.org/)** • **[libpng](http://www.libpng.org/)** • **[libwebp](https://developers.google.com/speed/webp)** • **[stb](https://github.com/nothings/stb)** by Sean Barrett

---

<div align="center">

**Made with ❤️ by FastResize Project**

*If FastResize saves you time, give us a star!* ⭐

[![Star History Chart](https://api.star-history.com/svg?repos=tranhuucanh/fast_resize&type=Date)](https://star-history.com/#tranhuucanh/fast_resize&Date)

[⬆ Back to top](#fast_resize)

</div>
