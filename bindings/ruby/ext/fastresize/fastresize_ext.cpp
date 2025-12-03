#include <ruby.h>
#include <fastresize.h>
#include <string>
#include <vector>

// Module and class references
static VALUE rb_mFastResize;

// Helper function to convert Ruby string to C++ string
static std::string rb_string_to_cpp(VALUE rb_str) {
    Check_Type(rb_str, T_STRING);
    return std::string(RSTRING_PTR(rb_str), RSTRING_LEN(rb_str));
}

// Helper function to parse resize options from Ruby hash
static fastresize::ResizeOptions parse_resize_options(VALUE options) {
    fastresize::ResizeOptions opts;

    if (NIL_P(options)) {
        return opts;
    }

    Check_Type(options, T_HASH);

    // Parse width
    VALUE width = rb_hash_aref(options, ID2SYM(rb_intern("width")));
    if (!NIL_P(width)) {
        opts.target_width = NUM2INT(width);
    }

    // Parse height
    VALUE height = rb_hash_aref(options, ID2SYM(rb_intern("height")));
    if (!NIL_P(height)) {
        opts.target_height = NUM2INT(height);
    }

    // Parse scale
    VALUE scale = rb_hash_aref(options, ID2SYM(rb_intern("scale")));
    if (!NIL_P(scale)) {
        opts.scale_percent = (float)NUM2DBL(scale);
        opts.mode = fastresize::ResizeOptions::SCALE_PERCENT;
    } else if (!NIL_P(width) && !NIL_P(height)) {
        opts.mode = fastresize::ResizeOptions::EXACT_SIZE;
    } else if (!NIL_P(width)) {
        opts.mode = fastresize::ResizeOptions::FIT_WIDTH;
    } else if (!NIL_P(height)) {
        opts.mode = fastresize::ResizeOptions::FIT_HEIGHT;
    }

    // Parse quality
    VALUE quality = rb_hash_aref(options, ID2SYM(rb_intern("quality")));
    if (!NIL_P(quality)) {
        opts.quality = NUM2INT(quality);
    }

    // Parse keep_aspect_ratio
    VALUE keep_aspect = rb_hash_aref(options, ID2SYM(rb_intern("keep_aspect_ratio")));
    if (!NIL_P(keep_aspect)) {
        opts.keep_aspect_ratio = RTEST(keep_aspect);
    }

    // Parse overwrite
    VALUE overwrite = rb_hash_aref(options, ID2SYM(rb_intern("overwrite")));
    if (!NIL_P(overwrite)) {
        opts.overwrite_input = RTEST(overwrite);
    }

    // Parse filter
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

// FastResize.resize(input_path, output_path, options = {})
static VALUE rb_fastresize_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_path, output_path, options;
    rb_scan_args(argc, argv, "21", &input_path, &output_path, &options);

    try {
        std::string input = rb_string_to_cpp(input_path);
        std::string output = rb_string_to_cpp(output_path);
        fastresize::ResizeOptions opts = parse_resize_options(options);

        bool success = fastresize::resize(input, output, opts);

        if (!success) {
            std::string error = fastresize::get_last_error();
            rb_raise(rb_eRuntimeError, "Resize failed: %s", error.c_str());
        }

        return Qtrue;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Resize failed: %s", e.what());
    }
}

// FastResize.resize_with_format(input_path, output_path, format, options = {})
static VALUE rb_fastresize_resize_with_format(int argc, VALUE* argv, VALUE self) {
    VALUE input_path, output_path, format, options;
    rb_scan_args(argc, argv, "31", &input_path, &output_path, &format, &options);

    try {
        std::string input = rb_string_to_cpp(input_path);
        std::string output = rb_string_to_cpp(output_path);
        std::string fmt = rb_string_to_cpp(format);
        fastresize::ResizeOptions opts = parse_resize_options(options);

        bool success = fastresize::resize_with_format(input, output, fmt, opts);

        if (!success) {
            std::string error = fastresize::get_last_error();
            rb_raise(rb_eRuntimeError, "Resize with format failed: %s", error.c_str());
        }

        return Qtrue;
    } catch (const std::exception& e) {
        rb_raise(rb_eRuntimeError, "Resize with format failed: %s", e.what());
    }
}

// FastResize.image_info(path)
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

// FastResize.batch_resize(input_paths, output_dir, options = {})
static VALUE rb_fastresize_batch_resize(int argc, VALUE* argv, VALUE self) {
    VALUE input_paths, output_dir, options;
    rb_scan_args(argc, argv, "21", &input_paths, &output_dir, &options);

    Check_Type(input_paths, T_ARRAY);

    try {
        // Convert Ruby array to C++ vector
        std::vector<std::string> inputs;
        long len = RARRAY_LEN(input_paths);
        for (long i = 0; i < len; ++i) {
            VALUE item = rb_ary_entry(input_paths, i);
            inputs.push_back(rb_string_to_cpp(item));
        }

        std::string out_dir = rb_string_to_cpp(output_dir);
        fastresize::ResizeOptions resize_opts = parse_resize_options(options);

        // Parse batch options
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

            // Phase C: max_speed flag
            VALUE max_speed = rb_hash_aref(options, ID2SYM(rb_intern("max_speed")));
            if (!NIL_P(max_speed)) {
                batch_opts.max_speed = RTEST(max_speed);
            }
        }

        // Perform batch resize
        fastresize::BatchResult result = fastresize::batch_resize(inputs, out_dir, resize_opts, batch_opts);

        // Return result hash
        VALUE rb_result = rb_hash_new();
        rb_hash_aset(rb_result, ID2SYM(rb_intern("total")), INT2NUM(result.total));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("success")), INT2NUM(result.success));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("failed")), INT2NUM(result.failed));

        // Convert error messages
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

// FastResize.batch_resize_custom(items, options = {})
static VALUE rb_fastresize_batch_resize_custom(int argc, VALUE* argv, VALUE self) {
    VALUE items, options;
    rb_scan_args(argc, argv, "11", &items, &options);

    Check_Type(items, T_ARRAY);

    try {
        // Convert Ruby array of hashes to C++ vector of BatchItem
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

            // Parse item-specific options
            batch_item.options = parse_resize_options(item);

            batch_items.push_back(batch_item);
        }

        // Parse batch options
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

            // Phase C: max_speed flag
            VALUE max_speed = rb_hash_aref(options, ID2SYM(rb_intern("max_speed")));
            if (!NIL_P(max_speed)) {
                batch_opts.max_speed = RTEST(max_speed);
            }
        }

        // Perform batch resize
        fastresize::BatchResult result = fastresize::batch_resize_custom(batch_items, batch_opts);

        // Return result hash
        VALUE rb_result = rb_hash_new();
        rb_hash_aset(rb_result, ID2SYM(rb_intern("total")), INT2NUM(result.total));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("success")), INT2NUM(result.success));
        rb_hash_aset(rb_result, ID2SYM(rb_intern("failed")), INT2NUM(result.failed));

        // Convert error messages
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

// Initialize the extension
extern "C" void Init_fastresize_ext(void) {
    rb_mFastResize = rb_define_module("FastResize");

    // Define module methods
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
