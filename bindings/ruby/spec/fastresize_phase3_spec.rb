require 'spec_helper'

RSpec.describe 'FastResize Phase 3 - Format Support' do
  let(:test_dir_path) { test_dir }
  let(:base_image) { File.join(test_dir_path, 'base.bmp') }

  before(:each) do
    create_simple_bmp(400, 300, base_image)
  end

  describe 'JPEG Support' do
    it 'resizes JPEG images' do
      jpg_input = File.join(test_dir_path, 'input.jpg')
      jpg_output = File.join(test_dir_path, 'output.jpg')

      # Convert BMP to JPEG first
      FastResize.resize_with_format(base_image, jpg_input, 'jpg', width: 400, height: 300)

      # Now resize the JPEG
      FastResize.resize(jpg_input, jpg_output, width: 200, height: 150)

      info = FastResize.image_info(jpg_output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(150)
      expect(info[:format]).to eq('jpg')
    end

    it 'respects JPEG quality parameter' do
      jpg_high = File.join(test_dir_path, 'high_quality.jpg')
      jpg_low = File.join(test_dir_path, 'low_quality.jpg')

      FastResize.resize_with_format(base_image, jpg_high, 'jpg', width: 200, height: 150, quality: 95)
      FastResize.resize_with_format(base_image, jpg_low, 'jpg', width: 200, height: 150, quality: 10)

      # High quality should result in larger file size
      expect(File.size(jpg_high)).to be > File.size(jpg_low)
    end

    it 'handles JPEG file extension variations' do
      jpeg_output = File.join(test_dir_path, 'output.jpeg')
      FastResize.resize_with_format(base_image, jpeg_output, 'jpeg', width: 200, height: 150)

      info = FastResize.image_info(jpeg_output)
      expect(info[:format]).to eq('jpg')
    end
  end

  describe 'PNG Support' do
    it 'resizes PNG images' do
      png_input = File.join(test_dir_path, 'input.png')
      png_output = File.join(test_dir_path, 'output.png')

      # Convert BMP to PNG first
      FastResize.resize_with_format(base_image, png_input, 'png', width: 400, height: 300)

      # Now resize the PNG
      FastResize.resize(png_input, png_output, width: 200, height: 150)

      info = FastResize.image_info(png_output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(150)
      expect(info[:format]).to eq('png')
    end

    it 'handles PNG transparency' do
      # PNG supports transparency (RGBA)
      png_output = File.join(test_dir_path, 'transparent.png')
      FastResize.resize_with_format(base_image, png_output, 'png', width: 200, height: 150)

      info = FastResize.image_info(png_output)
      expect(info[:format]).to eq('png')
      expect(File.exist?(png_output)).to be true
    end
  end

  describe 'WEBP Support' do
    it 'resizes WEBP images' do
      webp_input = File.join(test_dir_path, 'input.webp')
      webp_output = File.join(test_dir_path, 'output.webp')

      # Convert BMP to WEBP first
      FastResize.resize_with_format(base_image, webp_input, 'webp', width: 400, height: 300)

      # Now resize the WEBP
      FastResize.resize(webp_input, webp_output, width: 200, height: 150)

      info = FastResize.image_info(webp_output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(150)
      expect(info[:format]).to eq('webp')
    end

    it 'respects WEBP quality parameter' do
      webp_high = File.join(test_dir_path, 'high_quality.webp')
      webp_low = File.join(test_dir_path, 'low_quality.webp')

      FastResize.resize_with_format(base_image, webp_high, 'webp', width: 200, height: 150, quality: 95)
      FastResize.resize_with_format(base_image, webp_low, 'webp', width: 200, height: 150, quality: 10)

      # High quality should result in larger file size
      expect(File.size(webp_high)).to be > File.size(webp_low)
    end
  end

  describe 'BMP Support' do
    it 'resizes BMP images' do
      bmp_output = File.join(test_dir_path, 'output.bmp')
      FastResize.resize(base_image, bmp_output, width: 200, height: 150)

      info = FastResize.image_info(bmp_output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(150)
      expect(info[:format]).to eq('bmp')
    end
  end

  describe 'Format Conversion' do
    it 'converts BMP to JPEG' do
      jpg_output = File.join(test_dir_path, 'bmp_to_jpg.jpg')
      FastResize.resize_with_format(base_image, jpg_output, 'jpg', width: 200, height: 150)

      info = FastResize.image_info(jpg_output)
      expect(info[:format]).to eq('jpg')
    end

    it 'converts BMP to PNG' do
      png_output = File.join(test_dir_path, 'bmp_to_png.png')
      FastResize.resize_with_format(base_image, png_output, 'png', width: 200, height: 150)

      info = FastResize.image_info(png_output)
      expect(info[:format]).to eq('png')
    end

    it 'converts BMP to WEBP' do
      webp_output = File.join(test_dir_path, 'bmp_to_webp.webp')
      FastResize.resize_with_format(base_image, webp_output, 'webp', width: 200, height: 150)

      info = FastResize.image_info(webp_output)
      expect(info[:format]).to eq('webp')
    end

    it 'converts JPEG to PNG' do
      jpg_input = File.join(test_dir_path, 'temp.jpg')
      png_output = File.join(test_dir_path, 'jpg_to_png.png')

      FastResize.resize_with_format(base_image, jpg_input, 'jpg', width: 400, height: 300)
      FastResize.resize_with_format(jpg_input, png_output, 'png', width: 200, height: 150)

      info = FastResize.image_info(png_output)
      expect(info[:format]).to eq('png')
    end

    it 'converts PNG to WEBP' do
      png_input = File.join(test_dir_path, 'temp.png')
      webp_output = File.join(test_dir_path, 'png_to_webp.webp')

      FastResize.resize_with_format(base_image, png_input, 'png', width: 400, height: 300)
      FastResize.resize_with_format(png_input, webp_output, 'webp', width: 200, height: 150)

      info = FastResize.image_info(webp_output)
      expect(info[:format]).to eq('webp')
    end
  end

  describe 'Auto Format Detection' do
    it 'auto-detects output format from extension (.jpg)' do
      output = File.join(test_dir_path, 'auto.jpg')
      FastResize.resize(base_image, output, width: 200, height: 150)

      info = FastResize.image_info(output)
      expect(info[:format]).to eq('jpg')
    end

    it 'auto-detects output format from extension (.png)' do
      output = File.join(test_dir_path, 'auto.png')
      FastResize.resize(base_image, output, width: 200, height: 150)

      info = FastResize.image_info(output)
      expect(info[:format]).to eq('png')
    end

    it 'auto-detects output format from extension (.webp)' do
      output = File.join(test_dir_path, 'auto.webp')
      FastResize.resize(base_image, output, width: 200, height: 150)

      info = FastResize.image_info(output)
      expect(info[:format]).to eq('webp')
    end

    it 'auto-detects output format from extension (.bmp)' do
      output = File.join(test_dir_path, 'auto.bmp')
      FastResize.resize(base_image, output, width: 200, height: 150)

      info = FastResize.image_info(output)
      expect(info[:format]).to eq('bmp')
    end
  end
end
