# FastResize - The Fastest Image Resizing Library On The Planet
# Copyright (C) 2025 Tran Huu Canh (0xTh3OKrypt) and FastResize Contributors
#
# Resize 1,000 images in 2 seconds. Up to 2.9x faster than libvips,
# 3.1x faster than imageflow. Uses 3-4x less RAM than alternatives.
#
# Author: Tran Huu Canh (0xTh3OKrypt)
# Email: tranhuucanh39@gmail.com
# Homepage: https://github.com/tranhuucanh/fast_resize
#
# BSD 3-Clause License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require_relative "fastresize/version"
require_relative "fastresize/platform"

module FastResize
  class Error < StandardError; end

  # Get library version
  #
  # @return [String] Version string
  def self.version
    cli_path = Platform.find_binary
    output = `#{cli_path} --version 2>&1`.strip
    output.sub('FastResize v', '')
  rescue => e
    VERSION
  end

  # Get image information
  #
  # @param path [String] Path to the image file
  # @return [Hash] Image info with :width, :height, :channels, :format
  #
  # @example
  #   FastResize.image_info("photo.jpg")
  #   # => { width: 1920, height: 1080, channels: 3, format: "JPEG" }
  def self.image_info(path)
    raise Error, "Image path cannot be empty" if path.nil? || path.empty?
    raise Error, "Image file not found: #{path}" unless File.exist?(path)

    cli_path = Platform.find_binary
    output = `#{cli_path} info '#{path}' 2>&1`
    raise Error, "Failed to get image info: #{output}" unless $?.success?

    info = {}
    output.each_line do |line|
      case line
      when /Format:\s*(\S+)/
        info[:format] = $1
      when /Size:\s*(\d+)x(\d+)/
        info[:width] = $1.to_i
        info[:height] = $2.to_i
      when /Channels:\s*(\d+)/
        info[:channels] = $1.to_i
      end
    end

    info
  end

  # Resize a single image
  #
  # @param input_path [String] Path to input image
  # @param output_path [String] Path to save resized image
  # @param options [Hash] Resize options
  # @option options [Integer] :width Target width in pixels
  # @option options [Integer] :height Target height in pixels
  # @option options [Float] :scale Scale factor (e.g., 0.5 = 50%, 2.0 = 200%)
  # @option options [Integer] :quality JPEG/WebP quality 1-100 (default: 85)
  # @option options [Symbol] :filter Resize filter: :mitchell, :catmull_rom, :box, :triangle
  # @option options [Boolean] :keep_aspect_ratio Maintain aspect ratio (default: true)
  # @option options [Boolean] :overwrite Overwrite input file (default: false)
  # @return [Boolean] true if successful
  #
  # @example Basic resize by width
  #   FastResize.resize("input.jpg", "output.jpg", width: 800)
  #
  # @example Resize to exact dimensions
  #   FastResize.resize("input.jpg", "output.jpg", width: 800, height: 600)
  #
  # @example Scale to 50%
  #   FastResize.resize("input.jpg", "output.jpg", scale: 0.5)
  #
  # @example With quality and filter
  #   FastResize.resize("input.jpg", "output.jpg",
  #     width: 800,
  #     quality: 95,
  #     filter: :catmull_rom
  #   )
  def self.resize(input_path, output_path, options = {})
    raise Error, "Input path cannot be empty" if input_path.nil? || input_path.empty?
    raise Error, "Output path cannot be empty" if output_path.nil? || output_path.empty?
    raise Error, "Input file not found: #{input_path}" unless File.exist?(input_path)

    cli_path = Platform.find_binary
    args = build_resize_args(input_path, output_path, options)

    result = system(cli_path, *args, out: File::NULL, err: File::NULL)
    raise Error, "Failed to resize image" unless result

    true
  end

  # Resize with format conversion
  #
  # @param input_path [String] Path to input image
  # @param output_path [String] Path to save resized image
  # @param format [String] Output format: 'jpg', 'png', 'webp'
  # @param options [Hash] Resize options (same as resize)
  # @return [Boolean] true if successful
  #
  # @example Convert to WebP
  #   FastResize.resize_with_format("input.jpg", "output.webp", "webp", width: 800)
  def self.resize_with_format(input_path, output_path, format, options = {})
    raise Error, "Input path cannot be empty" if input_path.nil? || input_path.empty?
    raise Error, "Output path cannot be empty" if output_path.nil? || output_path.empty?
    raise Error, "Format cannot be empty" if format.nil? || format.empty?
    raise Error, "Input file not found: #{input_path}" unless File.exist?(input_path)

    # The CLI handles format based on output extension
    # Ensure output path has the correct extension
    ext = File.extname(output_path).downcase
    expected_ext = ".#{format.downcase}"

    if ext != expected_ext
      output_path = output_path.sub(/\.[^.]+$/, expected_ext)
    end

    resize(input_path, output_path, options)
  end

  # Batch resize images to a directory with same options
  #
  # @param input_paths [Array<String>] Array of input image paths
  # @param output_dir [String] Output directory
  # @param options [Hash] Resize options plus batch options
  # @option options [Integer] :threads Number of threads (default: auto)
  # @option options [Boolean] :stop_on_error Stop on first error (default: false)
  # @option options [Boolean] :max_speed Enable pipeline mode (default: false)
  # @return [Hash] Result with :total, :success, :failed, :errors
  #
  # @example Batch resize
  #   files = Dir["photos/*.jpg"]
  #   result = FastResize.batch_resize(files, "thumbnails/", width: 300)
  #   # => { total: 100, success: 100, failed: 0, errors: [] }
  def self.batch_resize(input_paths, output_dir, options = {})
    raise Error, "Input paths cannot be empty" if input_paths.nil? || input_paths.empty?
    raise Error, "Output directory cannot be empty" if output_dir.nil? || output_dir.empty?

    # Create output directory if it doesn't exist
    require 'fileutils'
    FileUtils.mkdir_p(output_dir)

    cli_path = Platform.find_binary

    # Create temp file with input paths for batch mode
    require 'tempfile'
    temp_dir = Tempfile.new(['fastresize_batch', ''])
    temp_dir_path = temp_dir.path
    temp_dir.close
    temp_dir.unlink

    # Copy input files to temp directory (simulate batch input)
    # Actually, use the batch command directly
    args = ['batch']
    args += build_batch_args(options)

    # Get the input directory from the first file
    input_dir = File.dirname(input_paths.first)
    args << input_dir
    args << output_dir

    output = `#{cli_path} #{args.map { |a| "'#{a}'" }.join(' ')} 2>&1`
    success = $?.success?

    # Parse output
    result = {
      total: input_paths.length,
      success: 0,
      failed: 0,
      errors: []
    }

    if success
      # Parse success/failed from output
      if output =~ /(\d+) success/
        result[:success] = $1.to_i
      end
      if output =~ /(\d+) failed/
        result[:failed] = $1.to_i
      end
    else
      result[:failed] = input_paths.length
      result[:errors] << output.strip
    end

    result
  end

  # Batch resize with custom options per image
  #
  # @param items [Array<Hash>] Array of items, each with :input, :output, and resize options
  # @param options [Hash] Batch options (:threads, :stop_on_error, :max_speed)
  # @return [Hash] Result with :total, :success, :failed, :errors
  #
  # @example Custom batch resize
  #   items = [
  #     { input: "photo1.jpg", output: "thumb1.jpg", width: 300 },
  #     { input: "photo2.jpg", output: "thumb2.jpg", width: 400, quality: 90 }
  #   ]
  #   result = FastResize.batch_resize_custom(items)
  def self.batch_resize_custom(items, options = {})
    raise Error, "Items cannot be empty" if items.nil? || items.empty?

    result = {
      total: items.length,
      success: 0,
      failed: 0,
      errors: []
    }

    items.each do |item|
      input = item[:input]
      output = item[:output]
      item_options = item.reject { |k, _| [:input, :output].include?(k) }

      begin
        resize(input, output, item_options)
        result[:success] += 1
      rescue Error => e
        result[:failed] += 1
        result[:errors] << "#{input}: #{e.message}"
        break if options[:stop_on_error]
      end
    end

    result
  end

  private

  # Build CLI arguments for single resize
  def self.build_resize_args(input_path, output_path, options)
    args = [input_path, output_path]

    if options[:scale]
      # Scale factor: 0.5 = 50%, 2.0 = 200%
      args += ['-s', options[:scale].to_s]
    elsif options[:width] && options[:height]
      args += ['-w', options[:width].to_s]
      args += ['-h', options[:height].to_s]
    elsif options[:width]
      args += ['-w', options[:width].to_s]
    elsif options[:height]
      args += ['-h', options[:height].to_s]
    end

    args += ['-q', options[:quality].to_s] if options[:quality]

    if options[:filter]
      filter_name = options[:filter].to_s.gsub('_', '-')
      args += ['-f', filter_name]
    end

    args << '--no-aspect-ratio' if options[:keep_aspect_ratio] == false
    args << '-o' if options[:overwrite]

    args
  end

  # Build CLI arguments for batch operations
  def self.build_batch_args(options)
    args = []

    if options[:scale]
      # Scale factor: 0.5 = 50%, 2.0 = 200%
      args += ['-s', options[:scale].to_s]
    elsif options[:width] && options[:height]
      args += ['-w', options[:width].to_s]
      args += ['-h', options[:height].to_s]
    elsif options[:width]
      args += ['-w', options[:width].to_s]
    elsif options[:height]
      args += ['-h', options[:height].to_s]
    end

    args += ['-q', options[:quality].to_s] if options[:quality]

    if options[:filter]
      filter_name = options[:filter].to_s.gsub('_', '-')
      args += ['-f', filter_name]
    end

    args << '--no-aspect-ratio' if options[:keep_aspect_ratio] == false
    args += ['-t', options[:threads].to_s] if options[:threads]
    args << '--stop-on-error' if options[:stop_on_error]
    args << '--max-speed' if options[:max_speed]

    args
  end
end
