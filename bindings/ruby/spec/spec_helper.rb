require 'bundler/setup'
require 'fastresize'
require 'fileutils'
require 'tmpdir'

RSpec.configure do |config|
  # Enable flags like --only-failures and --next-failure
  config.example_status_persistence_file_path = '.rspec_status'

  # Disable RSpec exposing methods globally on `Module` and `main`
  config.disable_monkey_patching!

  config.expect_with :rspec do |c|
    c.syntax = :expect
  end

  # Create a temporary directory for test outputs
  config.around(:each) do |example|
    Dir.mktmpdir do |dir|
      @test_dir = dir
      example.run
    end
  end

  # Helper method to get test directory
  def test_dir
    @test_dir
  end

  # Helper method to create test image using ImageMagick/GraphicsMagick if available
  def create_test_image(width, height, path)
    # Create a simple test image using stb (via our library)
    # For now, we'll use existing test images from the examples directory
    project_root = File.expand_path('../../../../', __FILE__)
    examples_dir = File.join(project_root, 'examples')

    # If we have an input.jpg in examples, use it as base
    source = File.join(examples_dir, 'input.jpg')
    if File.exist?(source)
      FileUtils.cp(source, path)
      return true
    end

    # Otherwise, try to create using system tools
    system("convert -size #{width}x#{height} xc:blue #{path} 2>/dev/null") ||
    system("magick -size #{width}x#{height} xc:blue #{path} 2>/dev/null")
  end

  # Helper method to verify image dimensions
  def verify_image_dimensions(path, expected_width, expected_height, tolerance: 0)
    info = FastResize.image_info(path)
    expect(info[:width]).to be_within(tolerance).of(expected_width)
    expect(info[:height]).to be_within(tolerance).of(expected_height)
  end

  # Helper method to create a simple BMP image programmatically
  def create_simple_bmp(width, height, path)
    # BMP header for 24-bit RGB image
    file_size = 54 + (width * height * 3)
    pixel_data_offset = 54
    dib_header_size = 40

    File.open(path, 'wb') do |f|
      # BMP File Header (14 bytes)
      f.write(['BM'].pack('A2'))                    # Signature
      f.write([file_size].pack('V'))                # File size
      f.write([0].pack('V'))                        # Reserved
      f.write([pixel_data_offset].pack('V'))        # Pixel data offset

      # DIB Header (40 bytes - BITMAPINFOHEADER)
      f.write([dib_header_size].pack('V'))          # DIB header size
      f.write([width].pack('V'))                    # Width
      f.write([height].pack('V'))                   # Height
      f.write([1].pack('v'))                        # Planes
      f.write([24].pack('v'))                       # Bits per pixel
      f.write([0].pack('V'))                        # Compression
      f.write([0].pack('V'))                        # Image size (can be 0 for uncompressed)
      f.write([0].pack('V'))                        # X pixels per meter
      f.write([0].pack('V'))                        # Y pixels per meter
      f.write([0].pack('V'))                        # Colors used
      f.write([0].pack('V'))                        # Important colors

      # Pixel data (BGR format, bottom-up)
      row_size = ((width * 3 + 3) / 4) * 4  # Rows padded to 4-byte boundary
      padding = row_size - (width * 3)

      height.times do |y|
        width.times do |x|
          # Create a gradient pattern
          r = (x * 255 / width) & 0xFF
          g = (y * 255 / height) & 0xFF
          b = 128
          f.write([b, g, r].pack('CCC'))  # BGR order
        end
        f.write([0].pack('C') * padding) if padding > 0
      end
    end
  end
end
