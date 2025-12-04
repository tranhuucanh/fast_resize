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

#include <ruby.h>
#include <ruby/thread.h>
#include <fastresize.h>
#include <string>
#include <vector>

static VALUE rb_mFastResize;

static std::string rb_string_to_cpp(VALUE rb_str) {
    Check_Type(rb_str, T_STRING);
    return std::string(RSTRING_PTR(rb_str), RSTRING_LEN(rb_str));
}

static fastresize::ResizeOptions parse_resize_options(VALUE options) {
    fastresize::ResizeOptions opts;

    if (NIL_P(options)) {
        return opts;
    }

    Check_Type(options, T_HASH);

    VALUE width = rb_hash_aref(options, ID2SYM(rb_intern("width")));
    if (!NIL_P(width)) {
        opts.target_width = NUM2INT(width);
    }

    VALUE height = rb_hash_aref(options, ID2SYM(rb_intern("height")));
    if (!NIL_P(height)) {
        opts.target_height = NUM2INT(height);
    }

    VALUE scale = rb_hash_aref(options, ID2SYM(rb_intern("scale")));
    if (!NIL_P(scale)) {
        opts.scale_percent = (float)NUM2DBL(scale) / 100.0f;
        if (opts.scale_percent <= 0) {
            rb_raise(rb_eArgError, "Scale must be positive");
        }
        opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    } else if (!NIL_P(width) && !NIL_P(height)) {
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    } else if (!NIL_P(width)) {
        opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    } else if (!NIL_P(height)) {
        opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
    }

    VALUE quality = rb_hash_aref(options, ID2SYM(rb_intern("quality")));
    if (!NIL_P(quality)) {
        opts.quality = NUM2INT(quality);
        if (opts.quality < 1 || opts.quality > 100) {
            rb_raise(rb_eArgError, "Quality must be between 1 and 100");
        }
    }

    VALUE keep_aspect = rb_hash_aref(options, ID2SYM(rb_intern("keep_aspect_ratio")));
    if (!NIL_P(keep_aspect)) {
        opts.keep_aspect_ratio = RTEST(keep_aspect);
    }

    VALUE overwrite = rb_hash_aref(options, ID2SYM(rb_intern("overwrite")));
    if (!NIL_P(overwrite)) {
        opts.overwrite_input = RTEST(overwrite);
    }

    if (opts.target_width < 0) {
        rb_raise(rb_eArgError, "Width must be non-negative");
    }
    if (opts.target_height < 0) {
        rb_raise(rb_eArgError, "Height must be non-negative");
    }

    VALUE filter = rb_hash_aref(options, ID2SYM(rb_intern("filter")));
    if (!NIL_P(filter)) {
        Check_Type(filter, T_SYMBOL);
        ID filter_id = SYM2ID(filter);

        if (filter_id == rb_intern("mitchell")) {
            opts.filter = fastresize::ResizeOptions::MITCHELL;
        } else if (filter_id == rb_intern("catmull_rom")) {
            opts.filter = fastresize::ResizeOptions::CATMULL_ROM;
        } else if (filter_id == rb_intern("box")) {
            opts.filter = fastresize::ResizeOptions::BOX;
        } else if (filter_id == rb_intern("triangle")) {
            opts.filter = fastresize::ResizeOptions::TRIANGLE;
        } else {
            rb_raise(rb_eArgError, "Invalid filter. Use :mitchell, :catmull_rom, :box, or :triangle");
        }
    }

    return opts;
}

struct ResizeParams {
    std::string input;
    std::string output;
    fastresize::ResizeOptions opts;
    bool success;
    std::string error;
};

static void* resize_without_gvl(void* data) {
    ResizeParams* params = static_cast<ResizeParams*>(data);
    try {
        params->success = fastresize::resize(params->input, params->output, params->opts);
        if (!params->success) {
            params->error = fastresize::get_last_error();
        }
    } catch (const std::exception& e) {
        params->success = false;
        params->error = std::string("Exception: ") + e.what();
    }
    return nullptr;
}

static VALUE rb_fastresize_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_path, output_path, options;
    rb_scan_args(argc, argv, "21", &input_path, &output_path, &options);

    try {
        ResizeParams params;
        params.input = rb_string_to_cpp(input_path);
        params.output = rb_string_to_cpp(output_path);
        params.opts = parse_resize_options(options);

        rb_thread_call_without_gvl(resize_without_gvl, &params, RUBY_UBF_IO, nullptr);

        if (!params.success) {
            rb_raise(rb_eRuntimeError, "Resize failed: %s", params.error.c_str());
        }

        return Qtrue;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Resize failed: %s", e.what());
    }
}

struct ResizeWithFormatParams {
    std::string input;
    std::string output;
    std::string format;
    fastresize::ResizeOptions opts;
    bool success;
    std::string error;
};

static void* resize_with_format_without_gvl(void* data) {
    ResizeWithFormatParams* params = static_cast<ResizeWithFormatParams*>(data);
    try {
        params->success = fastresize::resize_with_format(params->input, params->output, params->format, params->opts);
        if (!params->success) {
            params->error = fastresize::get_last_error();
        }
    } catch (const std::exception& e) {
        params->success = false;
        params->error = std::string("Exception: ") + e.what();
    }
    return nullptr;
}

static VALUE rb_fastresize_resize_with_format(int argc, VALUE* argv, VALUE self) {
    VALUE input_path, output_path, format, options;
    rb_scan_args(argc, argv, "31", &input_path, &output_path, &format, &options);

    try {
        ResizeWithFormatParams params;
        params.input = rb_string_to_cpp(input_path);
        params.output = rb_string_to_cpp(output_path);
        params.format = rb_string_to_cpp(format);
        params.opts = parse_resize_options(options);

        rb_thread_call_without_gvl(resize_with_format_without_gvl, &params, RUBY_UBF_IO, nullptr);

        if (!params.success) {
            rb_raise(rb_eRuntimeError, "Resize with format failed: %s", params.error.c_str());
        }

        return Qtrue;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Resize with format failed: %s", e.what());
    }
}

static VALUE rb_fastresize_image_info(VALUE self, VALUE path) {
    try {
        std::string path_str = rb_string_to_cpp(path);
        fastresize::ImageInfo info = fastresize::get_image_info(path_str);

        VALUE result = rb_hash_new();
        rb_hash_aset(result, ID2SYM(rb_intern("width")), INT2NUM(info.width));
        rb_hash_aset(result, ID2SYM(rb_intern("height")), INT2NUM(info.height));
        rb_hash_aset(result, ID2SYM(rb_intern("channels")), INT2NUM(info.channels));
        rb_hash_aset(result, ID2SYM(rb_intern("format")), rb_str_new_cstr(info.format.c_str()));

        return result;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Get image info failed: %s", e.what());
    }
}

static VALUE rb_fastresize_batch_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_paths, output_dir, options;
    rb_scan_args(argc, argv, "21", &input_paths, &output_dir, &options);

    Check_Type(input_paths, T_ARRAY);

    try {
        std::vector<std::string> inputs;
        long len = RARRAY_LEN(input_paths);
        for (long i = 0; i < len; ++i) {
            VALUE item = rb_ary_entry(input_paths, i);
            inputs.push_back(rb_string_to_cpp(item));
        }

        std::string out_dir = rb_string_to_cpp(output_dir);
        fastresize::ResizeOptions resize_opts = parse_resize_options(options);

        fastresize::BatchOptions batch_opts;
        if (!NIL_P(options)) {
            VALUE threads = rb_hash_aref(options, ID2SYM(rb_intern("threads")));
            if (!NIL_P(threads)) {
                batch_opts.num_threads = NUM2INT(threads);
            }

            VALUE stop_on_error = rb_hash_aref(options, ID2SYM(rb_intern("stop_on_error")));
            if (!NIL_P(stop_on_error)) {
                batch_opts.stop_on_error = RTEST(stop_on_error);
            }

            VALUE max_speed = rb_hash_aref(options, ID2SYM(rb_intern("max_speed")));
            if (!NIL_P(max_speed)) {
                batch_opts.max_speed = RTEST(max_speed);
            }
        }

        fastresize::BatchResult result = fastresize::batch_resize(inputs, out_dir, resize_opts, batch_opts);

        VALUE rb_result = rb_hash_new();
        rb_hash_aset(rb_result, ID2SYM(rb_intern("total")), INT2NUM(result.total));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("success")), INT2NUM(result.success));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("failed")), INT2NUM(result.failed));

        VALUE errors = rb_ary_new();
        for (const auto& error : result.errors) {
            rb_ary_push(errors, rb_str_new_cstr(error.c_str()));
        }
        rb_hash_aset(rb_result, ID2SYM(rb_intern("errors")), errors);

        return rb_result;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Batch resize failed: %s", e.what());
    }
}

static VALUE rb_fastresize_batch_resize_custom(int argc, VALUE* argv, VALUE self) {
    VALUE items, options;
    rb_scan_args(argc, argv, "11", &items, &options);

    Check_Type(items, T_ARRAY);

    try {
        std::vector<fastresize::BatchItem> batch_items;
        long len = RARRAY_LEN(items);
        for (long i = 0; i < len; ++i) {
            VALUE item = rb_ary_entry(items, i);
            Check_Type(item, T_HASH);

            fastresize::BatchItem batch_item;

            VALUE input = rb_hash_aref(item, ID2SYM(rb_intern("input")));
            if (NIL_P(input)) {
                rb_raise(rb_eArgError, "Item %ld missing 'input' key", i);
            }
            batch_item.input_path = rb_string_to_cpp(input);

            VALUE output = rb_hash_aref(item, ID2SYM(rb_intern("output")));
            if (NIL_P(output)) {
                rb_raise(rb_eArgError, "Item %ld missing 'output' key", i);
            }
            batch_item.output_path = rb_string_to_cpp(output);

            batch_item.options = parse_resize_options(item);

            batch_items.push_back(batch_item);
        }

        fastresize::BatchOptions batch_opts;
        if (!NIL_P(options)) {
            VALUE threads = rb_hash_aref(options, ID2SYM(rb_intern("threads")));
            if (!NIL_P(threads)) {
                batch_opts.num_threads = NUM2INT(threads);
            }

            VALUE stop_on_error = rb_hash_aref(options, ID2SYM(rb_intern("stop_on_error")));
            if (!NIL_P(stop_on_error)) {
                batch_opts.stop_on_error = RTEST(stop_on_error);
            }

            VALUE max_speed = rb_hash_aref(options, ID2SYM(rb_intern("max_speed")));
            if (!NIL_P(max_speed)) {
                batch_opts.max_speed = RTEST(max_speed);
            }
        }

        fastresize::BatchResult result = fastresize::batch_resize_custom(batch_items, batch_opts);

        VALUE rb_result = rb_hash_new();
        rb_hash_aset(rb_result, ID2SYM(rb_intern("total")), INT2NUM(result.total));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("success")), INT2NUM(result.success));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("failed")), INT2NUM(result.failed));

        VALUE errors = rb_ary_new();
        for (const auto& error : result.errors) {
            rb_ary_push(errors, rb_str_new_cstr(error.c_str()));
        }
        rb_hash_aset(rb_result, ID2SYM(rb_intern("errors")), errors);

        return rb_result;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Batch resize custom failed: %s", e.what());
    }
}

extern "C" void Init_fastresize_ext(void) {
    rb_mFastResize = rb_define_module("FastResize");

    rb_define_singleton_method(rb_mFastResize, "resize",
        RUBY_METHOD_FUNC(rb_fastresize_resize), -1);
    rb_define_singleton_method(rb_mFastResize, "resize_with_format",
        RUBY_METHOD_FUNC(rb_fastresize_resize_with_format), -1);
    rb_define_singleton_method(rb_mFastResize, "image_info",
        RUBY_METHOD_FUNC(rb_fastresize_image_info), 1);
    rb_define_singleton_method(rb_mFastResize, "batch_resize",
        RUBY_METHOD_FUNC(rb_fastresize_batch_resize), -1);
    rb_define_singleton_method(rb_mFastResize, "batch_resize_custom",
        RUBY_METHOD_FUNC(rb_fastresize_batch_resize_custom), -1);
}
