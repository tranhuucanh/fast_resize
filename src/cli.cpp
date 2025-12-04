/*
 * FastResize - The Fastest Image Resizing Library On The Planet
 * Copyright (C) 2025 Tran Huu Canh (0xTh3OKrypt) and FastResize Contributors
 *
 * Resize 1,000 images in 2 seconds. Up to 2.9x faster than libvips,
 * 3.1x faster than imageflow. Uses 3-4x less RAM than alternatives.
 *
 * Author: Tran Huu Canh (0xTh3OKrypt)
 * Email: tranhuucanh39@gmail.com
 * Homepage: https://github.com/tranhuucanh/fast_resize
 *
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fastresize.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// Version from CMake (passed via -DFASTRESIZE_VERSION)
#ifndef FASTRESIZE_VERSION
#define FASTRESIZE_VERSION "1.0.0"
#endif

static const char* get_version() {
    return FASTRESIZE_VERSION;
}

void print_usage(const char* program_name) {
    std::cout << "FastResize v" << get_version() << " - The Fastest Image Resizing Library\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] <input> <output> [width] [height]\n";
    std::cout << "       " << program_name << " batch [OPTIONS] <input_dir> <output_dir>\n";
    std::cout << "       " << program_name << " info <image>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  (default)     Resize single image\n";
    std::cout << "  batch         Batch resize all images in directory\n";
    std::cout << "  info          Show image information\n\n";
    std::cout << "Resize Options:\n";
    std::cout << "  -w, --width WIDTH       Target width in pixels\n";
    std::cout << "  -h, --height HEIGHT     Target height in pixels\n";
    std::cout << "  -s, --scale SCALE       Scale factor (e.g., 0.5 = 50%, 2.0 = 200%)\n";
    std::cout << "  -q, --quality QUALITY   JPEG/WebP quality 1-100 (default: 85)\n";
    std::cout << "  -f, --filter FILTER     Resize filter: mitchell, catmull_rom, box, triangle\n";
    std::cout << "                          (default: mitchell)\n";
    std::cout << "  --no-aspect-ratio       Don't maintain aspect ratio\n";
    std::cout << "  -o, --overwrite         Overwrite input file\n\n";
    std::cout << "Batch Options:\n";
    std::cout << "  -t, --threads NUM       Number of threads (default: auto)\n";
    std::cout << "  --stop-on-error         Stop on first error\n";
    std::cout << "  --max-speed             Enable pipeline mode (uses more RAM)\n\n";
    std::cout << "Other Options:\n";
    std::cout << "  --help                  Show this help\n";
    std::cout << "  --version               Show version\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Resize to width 800 (height auto)\n";
    std::cout << "  " << program_name << " input.jpg output.jpg 800\n\n";
    std::cout << "  # Resize to exact 800x600\n";
    std::cout << "  " << program_name << " input.jpg output.jpg 800 600\n\n";
    std::cout << "  # Resize with options\n";
    std::cout << "  " << program_name << " input.jpg output.jpg -w 800 -q 95 -f catmull_rom\n\n";
    std::cout << "  # Scale to 50%\n";
    std::cout << "  " << program_name << " input.jpg output.jpg -s 0.5\n\n";
    std::cout << "  # Batch resize directory\n";
    std::cout << "  " << program_name << " batch photos/ thumbnails/ -w 800\n\n";
    std::cout << "  # Batch with max speed\n";
    std::cout << "  " << program_name << " batch photos/ thumbnails/ -w 800 --max-speed\n\n";
    std::cout << "  # Show image info\n";
    std::cout << "  " << program_name << " info photo.jpg\n\n";
}

bool parse_int(const char* str, int& value) {
    char* end;
    long val = strtol(str, &end, 10);
    if (end == str || *end != '\0' || val < 0 || val > INT_MAX) {
        return false;
    }
    value = static_cast<int>(val);
    return true;
}

bool parse_float(const char* str, float& value) {
    char* end;
    float val = strtof(str, &end);
    if (end == str || *end != '\0' || val <= 0) {
        return false;
    }
    value = val;
    return true;
}

bool parse_filter(const char* str, fastresize::ResizeOptions& opts) {
    std::string filter = str;
    if (filter == "mitchell") {
        opts.filter = fastresize::ResizeOptions::MITCHELL;
    } else if (filter == "catmull_rom" || filter == "catmull-rom") {
        opts.filter = fastresize::ResizeOptions::CATMULL_ROM;
    } else if (filter == "box") {
        opts.filter = fastresize::ResizeOptions::BOX;
    } else if (filter == "triangle") {
        opts.filter = fastresize::ResizeOptions::TRIANGLE;
    } else {
        return false;
    }
    return true;
}

// Create directory recursively
bool mkdir_p(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    // Try to create directory
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }

    if (errno != ENOENT) {
        return false;
    }

    // Parent doesn't exist, create it
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return false;
    }

    if (!mkdir_p(path.substr(0, pos))) {
        return false;
    }

    return mkdir(path.c_str(), 0755) == 0;
}

// Get all image files in directory
bool get_image_files(const std::string& dir, std::vector<std::string>& files) {
    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        std::cerr << "Error: Cannot open directory: " << dir << std::endl;
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        // Check if it's an image file
        size_t dot = name.find_last_of('.');
        if (dot == std::string::npos) continue;

        std::string ext = name.substr(dot);
        // Convert to lowercase
        for (char& c : ext) c = tolower(c);

        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".webp" || ext == ".bmp") {
            std::string full_path = dir;
            if (full_path.back() != '/') full_path += '/';
            full_path += name;
            files.push_back(full_path);
        }
    }

    closedir(dp);
    return true;
}

// Extract filename from path
std::string get_filename(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

// Command: info
int cmd_info(const std::string& image_path) {
    fastresize::ImageInfo info = fastresize::get_image_info(image_path);
    if (info.width == 0) {
        std::cerr << "Error: " << fastresize::get_last_error() << std::endl;
        return 1;
    }

    std::cout << "Image: " << image_path << std::endl;
    std::cout << "  Format: " << info.format << std::endl;
    std::cout << "  Size: " << info.width << "x" << info.height << std::endl;
    std::cout << "  Channels: " << info.channels;
    if (info.channels == 3) std::cout << " (RGB)";
    else if (info.channels == 4) std::cout << " (RGBA)";
    else if (info.channels == 1) std::cout << " (Grayscale)";
    std::cout << std::endl;

    return 0;
}

// Command: batch resize
int cmd_batch(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Error: batch command requires input_dir and output_dir\n";
        std::cerr << "Usage: " << argv[0] << " batch [OPTIONS] <input_dir> <output_dir>\n";
        return 1;
    }

    fastresize::ResizeOptions resize_opts;
    fastresize::BatchOptions batch_opts;
    std::string input_dir;
    std::string output_dir;

    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-w" || arg == "--width") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], resize_opts.target_width)) {
                std::cerr << "Error: Invalid width\n";
                return 1;
            }
        } else if (arg == "-h" || arg == "--height") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], resize_opts.target_height)) {
                std::cerr << "Error: Invalid height\n";
                return 1;
            }
        } else if (arg == "-s" || arg == "--scale") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_float(argv[i], resize_opts.scale_percent)) {
                std::cerr << "Error: Invalid scale\n";
                return 1;
            }
            resize_opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
        } else if (arg == "-q" || arg == "--quality") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], resize_opts.quality) ||
                resize_opts.quality < 1 || resize_opts.quality > 100) {
                std::cerr << "Error: Quality must be between 1 and 100\n";
                return 1;
            }
        } else if (arg == "-f" || arg == "--filter") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_filter(argv[i], resize_opts)) {
                std::cerr << "Error: Invalid filter. Use mitchell, catmull_rom, box, or triangle\n";
                return 1;
            }
        } else if (arg == "--no-aspect-ratio") {
            resize_opts.keep_aspect_ratio = false;
        } else if (arg == "-t" || arg == "--threads") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], batch_opts.num_threads) || batch_opts.num_threads < 0) {
                std::cerr << "Error: Invalid thread count\n";
                return 1;
            }
        } else if (arg == "--stop-on-error") {
            batch_opts.stop_on_error = true;
        } else if (arg == "--max-speed") {
            batch_opts.max_speed = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            return 1;
        } else {
            // Non-option arguments
            if (input_dir.empty()) {
                input_dir = arg;
            } else if (output_dir.empty()) {
                output_dir = arg;
            } else {
                std::cerr << "Error: Too many arguments\n";
                return 1;
            }
        }
    }

    if (input_dir.empty() || output_dir.empty()) {
        std::cerr << "Error: Missing input_dir or output_dir\n";
        return 1;
    }

    // Determine resize mode based on what was specified
    if (resize_opts.mode != fastresize::ResizeOptions::SCALE_PERCENT) {
        if (resize_opts.target_width > 0 && resize_opts.target_height > 0) {
            resize_opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        } else if (resize_opts.target_width > 0) {
            resize_opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
        } else if (resize_opts.target_height > 0) {
            resize_opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
        } else {
            std::cerr << "Error: Must specify width, height, or scale\n";
            return 1;
        }
    }

    // Get all image files
    std::vector<std::string> input_files;
    if (!get_image_files(input_dir, input_files)) {
        return 1;
    }

    if (input_files.empty()) {
        std::cerr << "Error: No image files found in " << input_dir << std::endl;
        return 1;
    }

    std::cout << "Processing " << input_files.size() << " images..." << std::endl;

    // Perform batch resize
    fastresize::BatchResult result = fastresize::batch_resize(
        input_files, output_dir, resize_opts, batch_opts);

    std::cout << "Done: " << result.success << " success, "
              << result.failed << " failed" << std::endl;

    if (!result.errors.empty()) {
        std::cerr << "\nErrors:" << std::endl;
        for (const auto& error : result.errors) {
            std::cerr << "  " << error << std::endl;
        }
    }

    return result.failed > 0 ? 1 : 0;
}

// Command: single image resize
int cmd_resize(int argc, char* argv[]) {
    fastresize::ResizeOptions opts;
    std::string input_path;
    std::string output_path;
    int positional_width = 0;
    int positional_height = 0;
    bool has_positional_args = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "FastResize v" << get_version() << std::endl;
            return 0;
        } else if (arg == "-w" || arg == "--width") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], opts.target_width)) {
                std::cerr << "Error: Invalid width\n";
                return 1;
            }
        } else if (arg == "-h" || arg == "--height") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], opts.target_height)) {
                std::cerr << "Error: Invalid height\n";
                return 1;
            }
        } else if (arg == "-s" || arg == "--scale") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_float(argv[i], opts.scale_percent)) {
                std::cerr << "Error: Invalid scale\n";
                return 1;
            }
            opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
        } else if (arg == "-q" || arg == "--quality") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_int(argv[i], opts.quality) ||
                opts.quality < 1 || opts.quality > 100) {
                std::cerr << "Error: Quality must be between 1 and 100\n";
                return 1;
            }
        } else if (arg == "-f" || arg == "--filter") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
            if (!parse_filter(argv[i], opts)) {
                std::cerr << "Error: Invalid filter. Use mitchell, catmull_rom, box, or triangle\n";
                return 1;
            }
        } else if (arg == "--no-aspect-ratio") {
            opts.keep_aspect_ratio = false;
        } else if (arg == "-o" || arg == "--overwrite") {
            opts.overwrite_input = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            return 1;
        } else {
            // Positional arguments: input output [width] [height]
            if (input_path.empty()) {
                input_path = arg;
            } else if (output_path.empty()) {
                output_path = arg;
            } else if (positional_width == 0) {
                if (!parse_int(arg.c_str(), positional_width)) {
                    std::cerr << "Error: Invalid width: " << arg << std::endl;
                    return 1;
                }
                has_positional_args = true;
            } else if (positional_height == 0) {
                if (!parse_int(arg.c_str(), positional_height)) {
                    std::cerr << "Error: Invalid height: " << arg << std::endl;
                    return 1;
                }
                has_positional_args = true;
            } else {
                std::cerr << "Error: Too many arguments\n";
                return 1;
            }
        }
    }

    // Validate required arguments
    if (input_path.empty() || output_path.empty()) {
        std::cerr << "Error: Missing required arguments\n";
        print_usage(argv[0]);
        return 1;
    }

    // Use positional width/height if provided
    if (has_positional_args) {
        if (positional_width > 0) opts.target_width = positional_width;
        if (positional_height > 0) opts.target_height = positional_height;
    }

    // Determine resize mode
    if (opts.mode != fastresize::ResizeOptions::SCALE_PERCENT) {
        if (opts.target_width > 0 && opts.target_height > 0) {
            opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        } else if (opts.target_width > 0) {
            opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
        } else if (opts.target_height > 0) {
            opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
        } else {
            std::cerr << "Error: Must specify width, height, or scale\n";
            return 1;
        }
    }

    // Perform resize
    if (!fastresize::resize(input_path, output_path, opts)) {
        std::cerr << "Error: " << fastresize::get_last_error() << std::endl;
        return 1;
    }

    std::cout << "✓ Resized successfully: " << output_path << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    // Check for global options
    if (command == "--help") {
        print_usage(argv[0]);
        return 0;
    } else if (command == "--version") {
        std::cout << "FastResize v" << get_version() << std::endl;
        return 0;
    }

    // Check for commands
    if (command == "batch") {
        return cmd_batch(argc, argv);
    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Error: info command requires image path\n";
            return 1;
        }
        return cmd_info(argv[2]);
    } else {
        // Default: single image resize
        return cmd_resize(argc, argv);
    }
}
