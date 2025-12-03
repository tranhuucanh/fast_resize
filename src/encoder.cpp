#include "internal.h"
#include <cstdio>
#include <csetjmp>
#include <mutex>

// Phase 3: Specialized codec headers
#include <jpeglib.h>
#include <png.h>
#include <webp/encode.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fastresize {
namespace internal {

// Thread safety: libpng is not thread-safe, need mutex
static std::mutex png_encode_mutex;

// ============================================
// JPEG Encoding (libjpeg-turbo)
// ============================================

struct jpeg_error_mgr_ext {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void jpeg_encode_error_exit(j_common_ptr cinfo) {
    jpeg_error_mgr_ext* myerr = (jpeg_error_mgr_ext*)cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);
}

bool encode_jpeg(const std::string& path, const ImageData& data, int quality) {
    FILE* outfile = fopen(path.c_str(), "wb");
    if (!outfile) return false;

    // Buffer for RGBA → RGB conversion
    unsigned char* rgb_buffer = nullptr;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr_ext jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_encode_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        if (rgb_buffer) delete[] rgb_buffer;
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = data.width;
    cinfo.image_height = data.height;

    // Handle different channel counts
    const unsigned char* encode_pixels = data.pixels;
    int encode_components = data.channels;

    if (data.channels == 1) {
        cinfo.in_color_space = JCS_GRAYSCALE;
        cinfo.input_components = 1;
    } else if (data.channels == 3) {
        cinfo.in_color_space = JCS_RGB;
        cinfo.input_components = 3;
    } else if (data.channels == 4) {
        // JPEG doesn't support alpha - strip alpha channel (RGBA → RGB)
        cinfo.in_color_space = JCS_RGB;
        cinfo.input_components = 3;
        encode_components = 3;

        // Allocate RGB buffer (without alpha)
        size_t rgb_size = static_cast<size_t>(data.width) * data.height * 3;
        rgb_buffer = new unsigned char[rgb_size];

        // Strip alpha channel: RGBA → RGB
        const unsigned char* src = data.pixels;
        unsigned char* dst = rgb_buffer;
        for (int i = 0; i < data.width * data.height; i++) {
            *dst++ = *src++;  // R
            *dst++ = *src++;  // G
            *dst++ = *src++;  // B
            src++;            // Skip A
        }

        encode_pixels = rgb_buffer;
    } else {
        // Unsupported channel count
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        return false;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    // Phase A Optimization #2: Use fast DCT for 5-10% speed improvement
    cinfo.dct_method = JDCT_IFAST;

    jpeg_start_compress(&cinfo, TRUE);

    size_t row_stride = data.width * encode_components;
    JSAMPROW row_pointer[1];

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = const_cast<JSAMPROW>(&encode_pixels[cinfo.next_scanline * row_stride]);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    // Cleanup RGB buffer if allocated
    if (rgb_buffer) {
        delete[] rgb_buffer;
    }

    return true;
}

// ============================================
// PNG Encoding (libpng)
// ============================================

bool encode_png(const std::string& path, const ImageData& data, int quality) {
    // Thread safety: Lock during entire PNG encoding
    // libpng uses global state and is not thread-safe
    std::lock_guard<std::mutex> lock(png_encode_mutex);

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);

    // Set compression level (0-9, higher = better compression)
    // PNG quality is inverse: we convert 1-100 to 0-9
    int compression_level = 9 - ((quality - 1) * 9 / 99);
    if (compression_level < 0) compression_level = 0;
    if (compression_level > 9) compression_level = 9;
    png_set_compression_level(png, compression_level);

    // Determine color type
    int color_type;
    switch (data.channels) {
        case 1: color_type = PNG_COLOR_TYPE_GRAY; break;
        case 2: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case 3: color_type = PNG_COLOR_TYPE_RGB; break;
        case 4: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
        default:
            png_destroy_write_struct(&png, &info);
            fclose(fp);
            return false;
    }

    png_set_IHDR(png, info, data.width, data.height, 8,
                 color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    size_t row_bytes = data.width * data.channels;
    png_bytep* row_pointers = new png_bytep[data.height];
    for (int y = 0; y < data.height; y++) {
        row_pointers[y] = data.pixels + y * row_bytes;
    }

    png_write_image(png, row_pointers);
    png_write_end(png, nullptr);

    delete[] row_pointers;
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    return true;
}

// ============================================
// WEBP Encoding (libwebp)
// ============================================

bool encode_webp(const std::string& path, const ImageData& data, int quality) {
    uint8_t* output = nullptr;
    size_t output_size = 0;

    // Encode based on channels
    if (data.channels == 4) {
        output_size = WebPEncodeRGBA(data.pixels, data.width, data.height,
                                     data.width * 4, quality, &output);
    } else if (data.channels == 3) {
        output_size = WebPEncodeRGB(data.pixels, data.width, data.height,
                                    data.width * 3, quality, &output);
    } else {
        // libwebp doesn't have direct grayscale encode, convert to RGB
        return false;
    }

    if (output_size == 0 || !output) {
        if (output) WebPFree(output);
        return false;
    }

    // Write to file
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        WebPFree(output);
        return false;
    }

    size_t written = fwrite(output, 1, output_size, fp);
    fclose(fp);
    WebPFree(output);

    return written == output_size;
}

// ============================================
// Image Encoding (Phase 3: specialized encoders)
// ============================================

bool encode_image(const std::string& path, const ImageData& data, ImageFormat format, int quality) {
    if (!data.pixels || data.width <= 0 || data.height <= 0) {
        return false;
    }

    // Use specialized encoders for better performance
    switch (format) {
        case FORMAT_JPEG:
            return encode_jpeg(path, data, quality);

        case FORMAT_PNG:
            return encode_png(path, data, quality);

        case FORMAT_WEBP:
            return encode_webp(path, data, quality);

        case FORMAT_BMP:
            // Use stb for BMP (simple format)
            {
                int result = stbi_write_bmp(path.c_str(), data.width, data.height, data.channels, data.pixels);
                if (!result) {
                    set_last_error(ENCODE_ERROR, "Failed to encode BMP image");
                    return false;
                }
                return true;
            }

        default:
            set_last_error(UNSUPPORTED_FORMAT, "Unsupported output format");
            return false;
    }
}

} // namespace internal
} // namespace fastresize
