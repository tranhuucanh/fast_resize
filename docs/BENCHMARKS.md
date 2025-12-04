# ⚡ FastResize Benchmarks

Detailed performance analysis comparing **FastResize** with **libvips** and **imageflow**.

---

## 🔬 Test Environment

| Component | Specification |
|-----------|---------------|
| **Machine** | MacBook Pro M2 |
| **OS** | macOS Sonoma |
| **RAM** | 16 GB |
| **FastResize** | v1.0.0 |
| **libvips** | v8.15 (via vips CLI) |
| **imageflow** | v2.0 (via imageflow_tool) |

---

## 📋 Test Setup

- **Images per format:** 1,000 images
- **Target width:** 800px (auto height)
- **Output format:** JPEG quality 85
- **Runs per test:** 2 (averaged)

---

## 🚀 Speed Benchmarks

### 🏆 Summary

| Format | FastResize | libvips (parallel) | imageflow | vs libvips | vs imageflow |
|--------|------------|-------------------|-----------|------------|--------------|
| **JPG** | **2.10s** | 5.24s | 6.60s | **2.5x faster** | **3.1x faster** |
| **PNG** | **3.43s** | 6.18s | 8.41s | **1.8x faster** | **2.5x faster** |
| **BMP** | **1.65s** | 4.72s | N/A | **2.9x faster** | - |
| **WEBP** | **14.03s** | 23.52s | 15.69s | **1.7x faster** | **1.1x faster** |

---

### 📸 JPG Performance

**Input:** 1000 images, 1179×1409, 3 channels (RGB)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 2.02s | 2.17s | **2.10s** | **~477 img/s** |
| libvips (parallel) | 5.20s | 5.28s | 5.24s | ~191 img/s |
| libvips (sequential) | 16.32s | 15.91s | 16.11s | ~62 img/s |
| imageflow | 6.62s | 6.58s | 6.60s | ~151 img/s |

**Result:** FastResize is **2.5x faster** than libvips (parallel), **3.1x faster** than imageflow 🏆

---

### 🖼️ PNG Performance

**Input:** 1000 images, 2104×1160, 4 channels (RGBA)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 3.55s | 3.32s | **3.43s** | **~292 img/s** |
| libvips (parallel) | 6.18s | 6.18s | 6.18s | ~162 img/s |
| libvips (sequential) | 31.95s | 31.46s | 31.71s | ~32 img/s |
| imageflow | 8.45s | 8.37s | 8.41s | ~119 img/s |

**Result:** FastResize is **1.8x faster** than libvips (parallel), **2.5x faster** than imageflow 🏆

---

### 🎨 BMP Performance

**Input:** 1000 images, 1045×1045, 3 channels (RGB)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 1.65s | 1.64s | **1.65s** | **~606 img/s** |
| libvips (parallel) | 4.75s | 4.70s | 4.72s | ~212 img/s |
| libvips (sequential) | 17.38s | 17.56s | 17.47s | ~57 img/s |
| imageflow | N/A | N/A | Not supported | - |

**Result:** FastResize is **2.9x faster** than libvips (parallel). Imageflow does not support BMP. 🏆

---

### 🌐 WEBP Performance

**Input:** 1000 images, 4275×2451, 3 channels (RGB)

| Tool | Run 1 | Run 2 | Avg Time | Throughput |
|------|-------|-------|----------|------------|
| **FastResize** | 13.99s | 14.07s | **14.03s** | **~71 img/s** |
| libvips (parallel) | 23.94s | 23.10s | 23.52s | ~43 img/s |
| libvips (sequential) | 105.36s | 105.61s | 105.49s | ~9 img/s |
| imageflow | 15.64s | 15.73s | 15.69s | ~64 img/s |

**Result:** FastResize is **1.7x faster** than libvips (parallel), **1.1x faster** than imageflow 🏆

---

## 💾 RAM Usage Benchmarks

### 🖼️ PNG (1000 images, 2104×1160, 4 channels)

| Library | Time | Throughput | RAM Peak | RAM Delta |
|---------|------|------------|----------|-----------|
| **FastResize** | 3.19s | 313 img/s | **138 MB** | 121 MB |
| libvips | 6.11s | 164 img/s | 439 MB | 303 MB |
| imageflow | 8.39s | 119 img/s | 514 MB | 78 MB |

**FastResize uses 3.2x less RAM than libvips, 3.7x less than imageflow** ⚡

---

### 📸 JPG (1000 images, 1179×1409, 3 channels)

| Library | Time | Throughput | RAM Peak | RAM Delta |
|---------|------|------------|----------|-----------|
| **FastResize** | 2.03s | 493 img/s | **271 MB** | 254 MB |
| libvips | 5.07s | 197 img/s | 460 MB | 189 MB |
| imageflow | 6.59s | 152 img/s | 541 MB | 85 MB |

**FastResize uses 1.7x less RAM than libvips, 2.0x less than imageflow** ⚡

---

## 📊 Summary

### 🏅 Speed Winner by Format

| Format | Winner | Speedup |
|--------|--------|---------|
| JPG | **FastResize** | 3.1x faster than imageflow |
| PNG | **FastResize** | 2.5x faster than imageflow |
| BMP | **FastResize** | 2.9x faster than libvips |
| WEBP | **FastResize** | 1.7x faster than libvips |

### 💪 RAM Efficiency

| Metric | FastResize | libvips | imageflow |
|--------|------------|---------|-----------|
| PNG Peak RAM | **138 MB** | 439 MB (3.2x more) | 514 MB (3.7x more) |
| JPG Peak RAM | **271 MB** | 460 MB (1.7x more) | 541 MB (2.0x more) |

---

## 🎯 Key Findings

1. **🏆 FastResize wins in ALL formats** - Consistently faster than both libvips and imageflow

2. **⚡ Best performance gains:**
   - JPG: **3.1x faster** than imageflow
   - BMP: **2.9x faster** than libvips

3. **💾 Memory efficiency:**
   - PNG: Uses **3-4x less RAM** than alternatives
   - JPG: Uses **1.7-2x less RAM** than alternatives

4. **🚄 Throughput:**
   - JPG: Up to **477 images/second**
   - BMP: Up to **606 images/second**
   - PNG: Up to **292 images/second**

---

## 🔥 Why FastResize is Faster

1. **SIMD Optimization** - Uses SSE2/AVX on x86, NEON on ARM
2. **Zero-copy Pipeline** - Minimal memory allocation and copying
3. **libjpeg-turbo** - 2-6x faster JPEG than standard libjpeg
4. **Memory-mapped I/O** - Efficient file reading
5. **Optimized Thread Pool** - Smart work distribution
6. **stb_image_resize2** - Modern resize algorithm with SIMD

---

[← Back to README](../README.md)
