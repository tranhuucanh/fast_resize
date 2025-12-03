#ifndef FASTRESIZE_H
#define FASTRESIZE_H

#include <string>
#include <vector>

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
    int num_threads;        // Thread pool size (0 = auto-detect, default: 0)
    bool stop_on_error;     // Stop if any image fails (default: false)
    bool max_speed;         // Enable Phase C pipeline (faster but uses more RAM, default: false)

    BatchOptions()
        : num_threads(0)    // Phase A Optimization #7: Auto-detect thread count
        , stop_on_error(false)
        , max_speed(false)  // Phase C: Default to balanced mode (no extra RAM)
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

#endif // FASTRESIZE_H
