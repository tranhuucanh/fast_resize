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

#include "internal.h"
#include <cstdio>
#include <cstring>
#include <csetjmp>

#include <jpeglib.h>
#include <png.h>
#include <webp/decode.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "simd_utils.h"

namespace fastresize {
namespace internal {

// ============================================
// Memory-Mapped File
// ============================================

struct MappedFile {
    void* data;
    size_t size;
    int fd;

#ifdef _WIN32
    HANDLE hFile;
    HANDLE hMapping;
#endif

    MappedFile() : data(nullptr), size(0), fd(-1) {
#ifdef _WIN32
        hFile = INVALID_HANDLE_VALUE;
        hMapping = nullptr;
#endif
    }

    ~MappedFile() {
        unmap();
    }

    bool map(const std::string& path) {
#ifdef _WIN32
        // Windows implementation
        hFile = CreateFileA(path.c_str(), GENERIC_READ,
                           FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMapping) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            return false;
        }

        data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        size = fileSize.QuadPart;

        if (!data) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            hMapping = nullptr;
            hFile = INVALID_HANDLE_VALUE;
            return false;
        }
#else
        // Unix/macOS implementation
        fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            close(fd);
            fd = -1;
            return false;
        }

        size = sb.st_size;
        if (size == 0) {
            close(fd);
            fd = -1;
            return false;
        }

        data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            close(fd);
            fd = -1;
            data = nullptr;
            return false;
        }
#endif
        return data != nullptr;
    }

    void unmap() {
        if (data) {
#ifdef _WIN32
            UnmapViewOfFile(data);
            if (hMapping) CloseHandle(hMapping);
            if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
            hMapping = nullptr;
            hFile = INVALID_HANDLE_VALUE;
#else
            munmap(data, size);
            if (fd >= 0) close(fd);
            fd = -1;
#endif
            data = nullptr;
            size = 0;
        }
    }
};

// ============================================
// Image Format Detection
// ============================================

ImageFormat detect_format(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return FORMAT_UNKNOWN;

    unsigned char header[12];
    size_t n = fread(header, 1, 12, f);
    fclose(f);

    if (n < 4) return FORMAT_UNKNOWN;

    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF)
        return FORMAT_JPEG;

    if (header[0] == 0x89 && header[1] == 0x50 &&
        header[2] == 0x4E && header[3] == 0x47)
        return FORMAT_PNG;

    if (n >= 12 &&
        header[0] == 'R' && header[1] == 'I' &&
        header[2] == 'F' && header[3] == 'F' &&
        header[8] == 'W' && header[9] == 'E' &&
        header[10] == 'B' && header[11] == 'P')
        return FORMAT_WEBP;

    if (header[0] == 'B' && header[1] == 'M')
        return FORMAT_BMP;

    return FORMAT_UNKNOWN;
}

std::string format_to_string(ImageFormat format) {
    switch (format) {
        case FORMAT_JPEG: return "jpg";
        case FORMAT_PNG: return "png";
        case FORMAT_WEBP: return "webp";
        case FORMAT_BMP: return "bmp";
        default: return "unknown";
    }
}

ImageFormat string_to_format(const std::string& str) {
    if (str == "jpg" || str == "jpeg") return FORMAT_JPEG;
    if (str == "png") return FORMAT_PNG;
    if (str == "webp") return FORMAT_WEBP;
    if (str == "bmp") return FORMAT_BMP;
    return FORMAT_UNKNOWN;
}

// ============================================
// JPEG Decoding
// ============================================

struct jpeg_error_mgr_ext {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void jpeg_error_exit(j_common_ptr cinfo) {
    jpeg_error_mgr_ext* myerr = (jpeg_error_mgr_ext*)cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);
}

ImageData decode_jpeg(const std::string& path, int target_width = 0, int target_height = 0) {
    ImageData data;
    data.pixels = nullptr;
    data.width = 0;
    data.height = 0;
    data.channels = 0;

    MappedFile mapped;
    if (!mapped.map(path)) {
        FILE* infile = fopen(path.c_str(), "rb");
        if (!infile) return data;

        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr_ext jerr;

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = jpeg_error_exit;

        if (setjmp(jerr.setjmp_buffer)) {
            jpeg_destroy_decompress(&cinfo);
            fclose(infile);
            return data;
        }

        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, infile);
        jpeg_read_header(&cinfo, TRUE);

        if (target_width > 0 && target_width < (int)cinfo.image_width) {
            int scale_factor = cinfo.image_width / target_width;
            if (scale_factor >= 8) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 8;
            } else if (scale_factor >= 4) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 4;
            } else if (scale_factor >= 2) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 2;
            }
        } else if (target_height > 0 && target_height < (int)cinfo.image_height) {
            int scale_factor = cinfo.image_height / target_height;
            if (scale_factor >= 8) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 8;
            } else if (scale_factor >= 4) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 4;
            } else if (scale_factor >= 2) {
                cinfo.scale_num = 1;
                cinfo.scale_denom = 2;
            }
        }

        cinfo.dct_method = JDCT_IFAST;
        cinfo.do_fancy_upsampling = FALSE;

        jpeg_start_decompress(&cinfo);

        data.width = cinfo.output_width;
        data.height = cinfo.output_height;
        data.channels = cinfo.output_components;

        size_t row_stride = data.width * data.channels;
        data.pixels = new unsigned char[data.height * row_stride];

        JSAMPROW row_pointer[1];
        while (cinfo.output_scanline < cinfo.output_height) {
            row_pointer[0] = &data.pixels[cinfo.output_scanline * row_stride];
            jpeg_read_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);

        return data;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr_ext jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return data;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char*)mapped.data, mapped.size);
    jpeg_read_header(&cinfo, TRUE);

    if (target_width > 0 && target_width < (int)cinfo.image_width) {
        int scale_factor = cinfo.image_width / target_width;
        if (scale_factor >= 8) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 8;
        } else if (scale_factor >= 4) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 4;
        } else if (scale_factor >= 2) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 2;
        }
    } else if (target_height > 0 && target_height < (int)cinfo.image_height) {
        int scale_factor = cinfo.image_height / target_height;
        if (scale_factor >= 8) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 8;
        } else if (scale_factor >= 4) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 4;
        } else if (scale_factor >= 2) {
            cinfo.scale_num = 1;
            cinfo.scale_denom = 2;
        }
    }

    cinfo.dct_method = JDCT_IFAST;
    cinfo.do_fancy_upsampling = FALSE;

    jpeg_start_decompress(&cinfo);

    data.width = cinfo.output_width;
    data.height = cinfo.output_height;
    data.channels = cinfo.output_components;

    size_t row_stride = data.width * data.channels;
    data.pixels = new unsigned char[data.height * row_stride];

    JSAMPROW row_pointer[1];
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &data.pixels[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return data;
}

// ============================================
// PNG Decoding
// ============================================

struct PngMemoryReader {
    const unsigned char* data;
    size_t size;
    size_t offset;
};

static void png_read_from_memory(png_structp png, png_bytep out, png_size_t count) {
    PngMemoryReader* reader = (PngMemoryReader*)png_get_io_ptr(png);
    if (reader->offset + count > reader->size) {
        png_error(png, "Read past end of data");
        return;
    }
    memcpy(out, reader->data + reader->offset, count);
    reader->offset += count;
}

ImageData decode_png(const std::string& path) {
    ImageData data;
    data.pixels = nullptr;
    data.width = 0;
    data.height = 0;
    data.channels = 0;

    MappedFile mapped;
    PngMemoryReader mem_reader = {nullptr, 0, 0};
    FILE* fp = nullptr;

    bool use_mmap = mapped.map(path);
    if (use_mmap) {
        mem_reader.data = (const unsigned char*)mapped.data;
        mem_reader.size = mapped.size;
        mem_reader.offset = 0;
    } else {
        fp = fopen(path.c_str(), "rb");
        if (!fp) return data;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        if (fp) fclose(fp);
        return data;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        if (fp) fclose(fp);
        return data;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        if (fp) fclose(fp);
        if (data.pixels) delete[] data.pixels;
        data.pixels = nullptr;
        return data;
    }

    if (use_mmap) {
        png_set_read_fn(png, &mem_reader, png_read_from_memory);
    } else {
        png_init_io(png, fp);
    }

    png_read_info(png, info);

    data.width = png_get_image_width(png, info);
    data.height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    png_read_update_info(png, info);

    switch (png_get_color_type(png, info)) {
        case PNG_COLOR_TYPE_GRAY:
            data.channels = 1;
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            data.channels = 2;
            break;
        case PNG_COLOR_TYPE_RGB:
            data.channels = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            data.channels = 4;
            break;
        default:
            data.channels = 3;
    }

    size_t row_bytes = png_get_rowbytes(png, info);
    data.pixels = new unsigned char[data.height * row_bytes];

    png_bytep* row_pointers = new png_bytep[data.height];
    for (int y = 0; y < data.height; y++) {
        row_pointers[y] = data.pixels + y * row_bytes;
    }

    png_read_image(png, row_pointers);
    png_read_end(png, nullptr);

    delete[] row_pointers;
    png_destroy_read_struct(&png, &info, nullptr);
    if (fp) fclose(fp);

    return data;
}

// ============================================
// WEBP Decoding
// ============================================

ImageData decode_webp(const std::string& path) {
    ImageData data;
    data.pixels = nullptr;
    data.width = 0;
    data.height = 0;
    data.channels = 0;

    MappedFile mapped;
    if (!mapped.map(path)) {
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) return data;

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (file_size <= 0) {
            fclose(fp);
            return data;
        }

        unsigned char* file_data = new unsigned char[file_size];
        size_t read_size = fread(file_data, 1, file_size, fp);
        fclose(fp);

        if (read_size != (size_t)file_size) {
            delete[] file_data;
            return data;
        }

        WebPBitstreamFeatures features;
        if (WebPGetFeatures(file_data, file_size, &features) != VP8_STATUS_OK) {
            delete[] file_data;
            return data;
        }

        data.width = features.width;
        data.height = features.height;
        data.channels = features.has_alpha ? 4 : 3;

        unsigned char* webp_pixels = nullptr;
        if (data.channels == 4) {
            webp_pixels = WebPDecodeRGBA(file_data, file_size, &data.width, &data.height);
        } else {
            webp_pixels = WebPDecodeRGB(file_data, file_size, &data.width, &data.height);
        }

        delete[] file_data;

        if (!webp_pixels) {
            data.width = 0;
            data.height = 0;
            data.channels = 0;
            return data;
        }

        size_t pixel_count = data.width * data.height * data.channels;
        data.pixels = new unsigned char[pixel_count];
        fast_copy_aligned(data.pixels, webp_pixels, pixel_count);
        WebPFree(webp_pixels);

        return data;
    }

    WebPBitstreamFeatures features;
    if (WebPGetFeatures((unsigned char*)mapped.data, mapped.size, &features) != VP8_STATUS_OK) {
        return data;
    }

    data.width = features.width;
    data.height = features.height;
    data.channels = features.has_alpha ? 4 : 3;

    unsigned char* webp_pixels = nullptr;
    if (data.channels == 4) {
        webp_pixels = WebPDecodeRGBA((unsigned char*)mapped.data, mapped.size,
                                      &data.width, &data.height);
    } else {
        webp_pixels = WebPDecodeRGB((unsigned char*)mapped.data, mapped.size,
                                     &data.width, &data.height);
    }

    if (!webp_pixels) {
        data.width = 0;
        data.height = 0;
        data.channels = 0;
        return data;
    }

    size_t pixel_count = data.width * data.height * data.channels;
    data.pixels = new unsigned char[pixel_count];
    fast_copy_aligned(data.pixels, webp_pixels, pixel_count);
    WebPFree(webp_pixels);

    return data;
}

// ============================================
// Image Decoding
// ============================================

ImageData decode_image(const std::string& path, ImageFormat format, int target_width, int target_height) {
    ImageData data;
    data.pixels = nullptr;
    data.width = 0;
    data.height = 0;
    data.channels = 0;

    switch (format) {
        case FORMAT_JPEG:
            return decode_jpeg(path, target_width, target_height);
        case FORMAT_PNG:
            return decode_png(path);
        case FORMAT_WEBP:
            return decode_webp(path);
        case FORMAT_BMP:
            data.pixels = stbi_load(path.c_str(),
                                   &data.width,
                                   &data.height,
                                   &data.channels,
                                   0);
            return data;
        default:
            data.pixels = stbi_load(path.c_str(),
                                   &data.width,
                                   &data.height,
                                   &data.channels,
                                   0);
            return data;
    }
}

void free_image_data(ImageData& data) {
    if (data.pixels) {
        delete[] data.pixels;
        data.pixels = nullptr;
    }
}

// ============================================
// Image Info
// ============================================

bool get_image_dimensions(const std::string& path, int& width, int& height, int& channels) {
    ImageFormat format = detect_format(path);

    if (format == FORMAT_WEBP) {
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) return false;

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (file_size <= 0) {
            fclose(fp);
            return false;
        }

        unsigned char* file_data = new unsigned char[file_size];
        size_t read_size = fread(file_data, 1, file_size, fp);
        fclose(fp);

        if (read_size != (size_t)file_size) {
            delete[] file_data;
            return false;
        }

        WebPBitstreamFeatures features;
        if (WebPGetFeatures(file_data, file_size, &features) == VP8_STATUS_OK) {
            width = features.width;
            height = features.height;
            channels = features.has_alpha ? 4 : 3;
            delete[] file_data;
            return true;
        }

        delete[] file_data;
        return false;
    } else {
        int w, h, c;
        int result = stbi_info(path.c_str(), &w, &h, &c);
        if (result) {
            width = w;
            height = h;
            channels = c;
            return true;
        }
        return false;
    }
}

}
}
