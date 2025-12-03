#include <fastresize.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <input> <output> <width> [height]" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " input.jpg output.jpg 800       # Resize to width 800" << std::endl;
        std::cout << "  " << argv[0] << " input.jpg output.jpg 800 600   # Resize to 800x600" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];
    int width = std::atoi(argv[3]);

    // Get input image info
    fastresize::ImageInfo info = fastresize::get_image_info(input_path);
    if (info.width == 0) {
        std::cerr << "Error: Could not read input image: " << input_path << std::endl;
        std::cerr << "Error: " << fastresize::get_last_error() << std::endl;
        return 1;
    }

    std::cout << "Input image:" << std::endl;
    std::cout << "  Path: " << input_path << std::endl;
    std::cout << "  Format: " << info.format << std::endl;
    std::cout << "  Size: " << info.width << "x" << info.height << std::endl;
    std::cout << "  Channels: " << info.channels << std::endl;
    std::cout << std::endl;

    // Setup resize options
    fastresize::ResizeOptions opts;

    if (argc >= 5) {
        // Exact size mode
        int height = std::atoi(argv[4]);
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
        opts.target_width = width;
        opts.target_height = height;
        opts.keep_aspect_ratio = true;  // Fit within dimensions
        std::cout << "Resizing to fit within " << width << "x" << height << " (maintaining aspect ratio)" << std::endl;
    } else {
        // Width-only mode
        opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
        opts.target_width = width;
        opts.keep_aspect_ratio = true;
        std::cout << "Resizing to width " << width << " (maintaining aspect ratio)" << std::endl;
    }

    // Perform resize
    std::cout << "Resizing..." << std::flush;
    bool success = fastresize::resize(input_path, output_path, opts);

    if (success) {
        std::cout << " Done!" << std::endl;

        // Get output image info
        fastresize::ImageInfo output_info = fastresize::get_image_info(output_path);
        std::cout << std::endl;
        std::cout << "Output image:" << std::endl;
        std::cout << "  Path: " << output_path << std::endl;
        std::cout << "  Size: " << output_info.width << "x" << output_info.height << std::endl;
        return 0;
    } else {
        std::cout << " Failed!" << std::endl;
        std::cerr << "Error: " << fastresize::get_last_error() << std::endl;
        return 1;
    }
}
