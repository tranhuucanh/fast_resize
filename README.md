# FastResize

Fast, lightweight, cross-platform image resizing library in C++ with Ruby bindings.

## Overview

FastResize is a high-performance image resizing library designed to:
- Resize images with minimal CPU and RAM usage
- Support batch processing (e.g., 300 images simultaneously)
- Provide Ruby bindings for easy integration
- Work across macOS (x64, ARM) and Linux (x64, ARM)

## Features

- **Image Formats**: PNG, JPG, JPEG, WEBP, BMP
- **Resize Modes**:
  - Percentage scaling (e.g., 50% of original)
  - Fixed width (height scales proportionally)
  - Fixed width & height (exact dimensions)
  - Keep aspect ratio option
- **Output Options**:
  - Save to new file
  - Overwrite original file
- **Batch Processing**: Process multiple images in parallel with configurable thread pool

## Current Status

**Phase 1 Complete** - Basic infrastructure and image I/O:
- ✅ Project structure setup
- ✅ CMake build system
- ✅ stb headers integrated (stb_image, stb_image_write, stb_image_resize2)
- ✅ Image format detection
- ✅ Basic decoder/encoder (all formats via stb)
- ✅ Core API defined
- ✅ Basic tests

## Building

### Requirements

- CMake 3.15+
- C++14 compiler (GCC, Clang, MSVC)

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

### Run Tests

```bash
make test
# or
./tests/test_basic
```

### Run Example

```bash
./examples/basic_resize input.jpg output.jpg 800
./examples/basic_resize input.jpg output.jpg 800 600
```

## C++ API Example

```cpp
#include <fastresize.h>

// Simple resize
fastresize::ResizeOptions opts;
opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
opts.target_width = 800;
opts.keep_aspect_ratio = true;

fastresize::resize("input.jpg", "output.jpg", opts);
```

## Development Roadmap

- [x] **Phase 1**: Project Setup & Core Infrastructure
- [ ] **Phase 2**: Image Resizing Core
- [ ] **Phase 3**: Advanced Image Codecs (libjpeg-turbo, libpng, libwebp)
- [ ] **Phase 4**: Batch Processing & Threading
- [ ] **Phase 5**: Ruby Binding
- [ ] **Phase 6**: Cross-Platform Build & CI/CD
- [ ] **Phase 7**: Documentation & Examples
- [ ] **Phase 8**: Testing & Optimization

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed implementation plan.

## License

MIT License - see [LICENSE](LICENSE) for details.

## Contributing

This project is in active development. See ARCHITECTURE.md for implementation details.
