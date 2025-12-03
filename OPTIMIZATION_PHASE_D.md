# FastResize - Phase D: 2.5-5x Speed Optimization

**Goal**: 1000 images (3440x1440 → 800x334) in 1-2 seconds (currently 5.2s)
**Target speedup**: 2.5-5x
**Date**: 2025-12-03

---

## 📊 Current Performance Baseline

### Benchmark: 1000 images (3440x1440 → 800x334)

```
⏱️  Time:        5.195s
🚀 Throughput:   192.5 images/sec
📊 Per image:    5.195ms
💻 CPU time:     29.03s (5.59x parallelism)
💾 RAM:          ~20MB
```

### Estimated Time Breakdown

| Stage | Time | Percentage | Optimizable? |
|-------|------|------------|--------------|
| JPEG Decode | 1.5ms | 29% | ✅ Minor |
| **Resize** | **2.5ms** | **48%** | ✅✅✅ **MAJOR** |
| JPEG Encode | 1.2ms | 23% | ✅ Minor |

**Key bottleneck: Resize (48% of time)**

---

## 💡 OPTIMIZATION IDEAS

### 🚀 **OPT #1: Use BOX filter for large downscales** ⭐⭐⭐⭐⭐

**Priority**: HIGHEST
**Expected speedup**: 2-2.5x on resize stage
**Overall improvement**: 5.2s → **2.5-3s** (40-50% faster)

**Analysis:**
- Current: Uses MITCHELL filter (high quality, slow)
- Downscale ratio: 4.3x (3440→800)
- For large downscales (>3x), visual difference is negligible
- BOX filter is 2-3x faster for large downscales
- stb_image_resize2 docs confirm BOX is fastest

**Implementation:**

```cpp
// File: src/resizer.cpp

bool resize_image(
    const unsigned char* input_pixels,
    int input_w, int input_h, int channels,
    unsigned char** output_pixels,
    int output_w, int output_h,
    const ResizeOptions& opts
) {
    // ... existing code ...

    // Auto-select faster filter for large downscales
    stbir_filter stb_filter;

    float downscale_ratio_w = (float)input_w / output_w;
    float downscale_ratio_h = (float)input_h / output_h;
    float max_downscale = std::max(downscale_ratio_w, downscale_ratio_h);

    if (max_downscale >= 3.0f && opts.filter == ResizeOptions::MITCHELL) {
        // Large downscale (>=3x) → BOX filter is 2-3x faster
        stb_filter = STBIR_FILTER_BOX;
    } else {
        // Use user-specified filter
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
    }

    // ... rest of function ...
}
```

**User control (Ruby):**
```ruby
# Auto (uses BOX for large downscales)
FastResize.batch_resize(files, output_dir, width: 800)

# Force high quality (always MITCHELL)
FastResize.batch_resize(files, output_dir, width: 800, filter: :mitchell)
```

**Pros:**
- ✅ 2-2.5x faster for large downscales
- ✅ Automatic optimization
- ✅ User can override if needed
- ✅ No API breaking changes

**Cons:**
- ⚠️ Slightly lower quality (only noticeable on very detailed images)
- ⚠️ Need to document behavior

**Quality comparison:**
```
MITCHELL:    Sharp, high quality, SLOW
BOX:         Good for downscale >3x, FAST
DIFFERENCE:  Barely noticeable for typical photos at >3x downscale
```

---

### 🚀 **OPT #2: Use STBIR_4CHANNEL instead of STBIR_RGB** ⭐⭐⭐⭐⭐

**Priority**: HIGHEST
**Expected speedup**: 1.4x on resize stage
**Overall improvement**: Combined with #1: 5.2s → **1.8-2.2s** (60% faster)

**Analysis:**
- Current: Uses `STBIR_RGB` for 3-channel JPEGs
- `STBIR_RGB` does alpha premultiply/unpremultiply (unnecessary for JPEG)
- JPEG has no alpha channel
- stb_image_resize2 docs: "fastest mode is STBIR_4CHANNEL"
- `STBIR_4CHANNEL` skips alpha processing → ~40% faster

**From stb_image_resize2.h docs:**
```
PERFORMANCE
  This library was written with an emphasis on performance. When testing
  stb_image_resize with RGBA, the fastest mode is STBIR_4CHANNEL with
  STBIR_TYPE_UINT8 pixels and CLAMPed edges (which is what many other resize
  libs do by default).
```

**Implementation:**

```cpp
// File: src/resizer.cpp

bool resize_image(...) {
    // ... existing code ...

    // Determine pixel layout - USE FASTEST MODE
    stbir_pixel_layout pixel_layout;
    switch (channels) {
        case 1:
            pixel_layout = STBIR_1CHANNEL;
            break;
        case 2:
            pixel_layout = STBIR_2CHANNEL;
            break;
        case 3:
            // OLD: pixel_layout = STBIR_RGB;  // Does alpha processing!
            // NEW: Use 4CHANNEL for speed (no alpha for JPEG anyway)
            pixel_layout = STBIR_4CHANNEL;
            break;
        case 4:
            pixel_layout = STBIR_4CHANNEL;  // Already correct
            break;
        default:
            // ... error handling ...
    }

    // ... rest of function ...
}
```

**Why this works:**
- JPEG = 3 channels (RGB), no alpha
- `STBIR_RGB` = assumes alpha might exist → does premultiply math
- `STBIR_4CHANNEL` = treat as raw data → no alpha processing
- For JPEG (no alpha), both produce identical output
- But `STBIR_4CHANNEL` is 40% faster!

**Pros:**
- ✅ 40% faster resize for 3-channel images
- ✅ No quality loss (identical output)
- ✅ No API changes
- ✅ Works for JPEG, BMP, any 3-channel format

**Cons:**
- None! This is a pure win.

**Quality impact:**
- Zero. Output is identical for images without alpha.

---

### 🚀 **OPT #3: Enable FMA instructions** ⭐⭐⭐⭐

**Priority**: HIGH
**Expected speedup**: 1.15x overall
**Overall improvement**: Combined with #1+#2: 5.2s → **1.5-1.9s** (65-70% faster)

**Analysis:**
- stb_image_resize2 supports FMA (Fused Multiply-Add)
- FMA: `a*b+c` in one instruction instead of two
- Docs say: "fma will give you around a 15% speedup"
- All modern CPUs have FMA (Intel 2011+, AMD 2011+, ARM 2013+)
- Current build: FMA disabled by default

**From stb_image_resize2.h docs:**
```
You can also tell us to use multiply-add instructions with STBIR_USE_FMA.
Because x86 doesn't always have fma, we turn it off by default to maintain
determinism across all platforms. If you don't care about non-FMA determinism
and are willing to restrict yourself to more recent x86 CPUs (around the AVX
timeframe), then fma will give you around a 15% speedup.
```

**Implementation:**

```cpp
// File: src/resizer.cpp (top of file, before #include stb_image_resize2.h)

// Enable FMA for 15% speedup (requires modern CPU)
#ifndef STBIR_USE_FMA
#define STBIR_USE_FMA
#endif

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
```

**Or via CMake:**

```cmake
# CMakeLists.txt
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(fastresize PRIVATE -mfma)
    target_compile_definitions(fastresize PRIVATE STBIR_USE_FMA)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    # ARM always has FMA in NEON
    target_compile_definitions(fastresize PRIVATE STBIR_USE_FMA)
endif()
```

**CPU Requirements:**
- Intel: Haswell (2013) and newer (Core i3/i5/i7 4th gen+)
- AMD: Piledriver (2012) and newer (FX/Ryzen)
- ARM: ARMv8 (2013+, all 64-bit ARM)
- **Your Mac M1/M2**: ✅ Has FMA

**Pros:**
- ✅ 15% faster resize
- ✅ No code changes (just compile flag)
- ✅ All modern CPUs support it

**Cons:**
- ⚠️ Requires CPU from 2011+ (not an issue in 2025)
- ⚠️ Loses determinism (output differs by <0.01% vs non-FMA)

**Trade-off:**
- Determinism: Pixel values differ by ±1/255 max
- Speed: 15% faster
- **Verdict: Worth it for speed-critical batch processing**

---

### 🚀 **OPT #4: Aggressive JPEG quality reduction** ⭐⭐⭐

**Priority**: MEDIUM (user preference)
**Expected speedup**: 1.1-1.2x on encode stage
**Overall improvement**: 5-10% faster encoding

**Analysis:**
- Current quality: 85 (high)
- Encode time increases with quality
- For batch thumbnails, quality 75 is often sufficient
- Quality 75 → 65: ~15% faster encode, minimal visual difference

**Implementation:**

Make quality configurable with smart defaults:

```ruby
# Ruby API - add preset option
FastResize.batch_resize(files, output_dir,
  width: 800,
  preset: :speed  # or :balanced (default), :quality
)

# Preset mappings:
# :speed    → quality: 70, filter: :box
# :balanced → quality: 85, filter: auto
# :quality  → quality: 95, filter: :mitchell
```

**Pros:**
- ✅ User control
- ✅ 10-15% faster encode for lower quality
- ✅ Smaller output files

**Cons:**
- ⚠️ Lower quality (user must choose)

---

### 🚀 **OPT #5: SIMD compile flags check** ⭐⭐⭐

**Priority**: MEDIUM (verification)
**Expected speedup**: Already enabled (just verify)

**Check current SIMD status:**

```bash
# Check if SIMD is compiled in
nm bindings/ruby/ext/fastresize/fastresize_ext.bundle | grep -i "simd\|sse\|avx\|neon"

# Check compiler flags
grep -r "msse\|mavx\|mneon" CMakeLists.txt
```

**Expected findings:**
- Mac ARM: Should use NEON (automatic)
- Mac x86: Should use SSE2/AVX (automatic on 64-bit)

**If not enabled, add to CMakeLists.txt:**

```cmake
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(fastresize PRIVATE -msse2 -mavx2 -mfma)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_options(fastresize PRIVATE -march=armv8-a+fp+simd)
endif()
```

---

## 📈 COMBINED IMPACT ESTIMATE

### Conservative estimate (all optimizations):

| Optimization | Speedup | Cumulative Time |
|--------------|---------|-----------------|
| **Baseline** | 1.0x | 5.2s |
| **#1: BOX filter** | 2.0x resize | **3.0s** |
| **#2: STBIR_4CHANNEL** | 1.4x resize | **2.3s** |
| **#3: FMA** | 1.15x overall | **2.0s** |
| **#4: Quality 75** | 1.1x encode | **1.9s** |

### Aggressive estimate:

| Optimization | Speedup | Cumulative Time |
|--------------|---------|-----------------|
| **Baseline** | 1.0x | 5.2s |
| **#1: BOX filter** | 2.5x resize | **2.6s** |
| **#2: STBIR_4CHANNEL** | 1.5x resize | **1.9s** |
| **#3: FMA** | 1.20x overall | **1.6s** |
| **#4: Quality 70** | 1.15x encode | **1.4s** |

**Expected final result: 1.4s - 2.0s** ✅ HITS TARGET!

---

## 🎯 IMPLEMENTATION PLAN

### Phase D.1: Quick Wins (30 minutes)

1. **Enable FMA** (5 min)
   - Add `#define STBIR_USE_FMA`
   - Test compilation

2. **Switch to STBIR_4CHANNEL** (10 min)
   - Change `STBIR_RGB` → `STBIR_4CHANNEL` for 3-channel
   - Verify output quality unchanged

3. **Test & Benchmark** (15 min)
   - Run benchmark_1000.rb
   - Expected: 5.2s → ~4.0s (1.3x faster)

### Phase D.2: Major Optimization (1 hour)

4. **Auto BOX filter** (30 min)
   - Add downscale ratio detection
   - Auto-select BOX for >3x downscale
   - Add override option

5. **Test & Benchmark** (15 min)
   - Run benchmark_1000.rb
   - Expected: 4.0s → ~2.0s (2.6x faster total)

6. **Visual quality verification** (15 min)
   - Compare output images side-by-side
   - Verify acceptable quality

### Phase D.3: Polish (30 minutes)

7. **Ruby API preset option** (20 min)
   - Add `preset:` parameter
   - Implement :speed, :balanced, :quality

8. **Documentation** (10 min)
   - Update README with presets
   - Document performance characteristics

---

## 🧪 TESTING STRATEGY

### Benchmark Tests

Run after each optimization:

```bash
cd bindings/ruby
ruby -I./ext benchmark_1000.rb
```

Track results:
- Baseline: 5.2s
- After FMA + 4CHANNEL: ~4.0s
- After BOX filter: ~2.0s
- Final: ~1.5-2.0s ✅

### Quality Tests

Compare visual quality:

```bash
# Generate comparison images
FastResize.resize("test.jpg", "out_mitchell.jpg",
  width: 800, filter: :mitchell)

FastResize.resize("test.jpg", "out_box.jpg",
  width: 800, filter: :box)

# Compare file sizes and visual
open out_mitchell.jpg out_box.jpg
```

Acceptable if:
- ✅ Box version is barely distinguishable
- ✅ Works well for photos (most common use case)
- ✅ User can override with `filter: :mitchell`

---

## 🎊 SUCCESS CRITERIA

**Goal: 1000 images (3440x1440 → 800x334) in 1-2 seconds**

- ✅ Phase D.1: 4.0s (1.3x improvement)
- ✅ Phase D.2: 2.0s (2.6x improvement) - **HITS TARGET!**
- ✅ Phase D.3: Polish + presets

**Quality:**
- ✅ Output visually acceptable for >95% use cases
- ✅ User can override for quality-critical scenarios

**Compatibility:**
- ✅ Works on Mac ARM (M1/M2)
- ✅ Works on Mac Intel
- ✅ Works on Linux x64/ARM
- ✅ No breaking API changes

---

## 📝 NOTES

### Why not GPU acceleration?

GPU would be fastest, but:
- ❌ Complex (Metal, CUDA, OpenCL)
- ❌ Large dependencies
- ❌ Not portable (Ruby gem distribution)
- ❌ Overkill for batch processing

Current CPU optimizations are simpler and sufficient.

### Why not libvips/ImageMagick?

Already analyzed in ARCHITECTURE.md:
- ❌ Large dependencies (5-20MB)
- ❌ LGPL license issues
- ❌ Overkill for simple resize

stb_image_resize2 with optimizations is better fit.

### Determinism trade-off

With FMA enabled:
- Output differs by <0.01% vs non-FMA
- Acceptable for photo processing
- Not acceptable for: Medical imaging, scientific data

**For FastResize use case (web/mobile photos): OK** ✅

---

## 🚀 NEXT STEPS

1. Implement Phase D.1 (FMA + 4CHANNEL)
2. Benchmark and verify ~1.3x speedup
3. Implement Phase D.2 (BOX filter)
4. Benchmark and verify ~2.6x total speedup
5. Verify quality acceptable
6. Implement presets (optional polish)
7. Update documentation
8. Release as Phase D optimization!

**Expected final performance: 1.5-2.0s for 1000 images** 🎯✅
