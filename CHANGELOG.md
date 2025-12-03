# Changelog

All notable changes to FastResize will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Phase 1 - Completed (2025-11-29)

#### Added
- Initial project structure and directory layout
- CMake build system with cross-platform support
- Integration of stb headers (stb_image.h, stb_image_write.h, stb_image_resize2.h)
- Public API header (fastresize.h) with complete interface definition
- Image format detection for JPEG, PNG, WEBP, BMP
- Basic image decoder using stb_image (supports all formats)
- Basic image encoder using stb_image_write (BMP, PNG, JPEG)
- Image resizing implementation with stb_image_resize2
- Support for multiple resize modes (SCALE_PERCENT, FIT_WIDTH, FIT_HEIGHT, EXACT_SIZE)
- Support for multiple resize filters (Mitchell, Catmull-Rom, Box, Triangle)
- Aspect ratio preservation
- Thread pool infrastructure (ready for Phase 4)
- Basic batch processing (sequential for now)
- Error handling with error codes and messages
- Unit test framework and basic tests
- Example program (basic_resize)
- Documentation (README.md, ARCHITECTURE.md, LICENSE)

#### Technical Details
- C++14 standard
- Header-only stb libraries for Phase 1
- Optimized compilation flags (-O3, -flto)
- Thread-safe error handling
- Clean separation of internal and public APIs

### Phase 2 - Completed (2025-11-29)

#### Added
- Comprehensive test suite with 22 tests covering all resize modes
- Test coverage for dimension calculation logic
- Edge case tests (1x1 images, very large images, extreme aspect ratios)
- Filter comparison tests (Mitchell, Catmull-Rom, Box, Triangle)
- Upscaling and downscaling tests
- Performance benchmark suite (bench_phase2)
- Benchmark results documentation

#### Validated
- All resize modes working correctly (SCALE_PERCENT, FIT_WIDTH, FIT_HEIGHT, EXACT_SIZE)
- Aspect ratio preservation logic
- All 4 resize filters (Mitchell, Catmull-Rom, Box, Triangle)
- Edge cases: 1x1 minimum size, large images (2000x2000+), extreme ratios (10:1, 1:10)
- Performance: 21.49ms for 2000x2000→800x600 resize (186 MP/s throughput)

#### Performance Benchmarks
- Small images (100x100): 1.19ms, 8.37 MP/s
- Medium images (800x600): 3.84ms, 124.84 MP/s
- Large images (2000x2000): 21.49ms, 186.15 MP/s
- Very large images (3000x2000): 35.14ms, 170.73 MP/s
- Filter performance: Box (fastest), Triangle, Mitchell (default), Catmull-Rom
- Consistent performance across different aspect ratios (~150 MP/s)

### Next Steps

#### Phase 3 - Advanced Image Codecs
- Integration of libjpeg-turbo for faster JPEG processing
- Integration of libpng for PNG processing
- Integration of libwebp for WEBP support
- Static linking configuration
- Performance comparison with stb implementations

## [1.0.0] - TBD

Initial release (after Phase 8 completion)
