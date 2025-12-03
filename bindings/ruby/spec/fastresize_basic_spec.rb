require 'spec_helper'

RSpec.describe FastResize do
  let(:test_image) { File.join(test_dir, 'test.bmp') }
  let(:output_image) { File.join(test_dir, 'output.bmp') }

  before(:each) do
    create_simple_bmp(100, 100, test_image)
  end

  describe '.version' do
    it 'returns the version string' do
      expect(FastResize.version).to match(/\d+\.\d+\.\d+/)
    end
  end

  describe '.resize' do
    it 'resizes image to exact dimensions' do
      FastResize.resize(test_image, output_image, width: 50, height: 50)
      expect(File.exist?(output_image)).to be true

      info = FastResize.image_info(output_image)
      expect(info[:width]).to eq(50)
      expect(info[:height]).to eq(50)
    end

    it 'resizes image by percentage' do
      FastResize.resize(test_image, output_image, scale: 0.5)
      expect(File.exist?(output_image)).to be true

      info = FastResize.image_info(output_image)
      expect(info[:width]).to eq(50)
      expect(info[:height]).to eq(50)
    end

    it 'resizes image to fit width' do
      FastResize.resize(test_image, output_image, width: 80)
      expect(File.exist?(output_image)).to be true

      info = FastResize.image_info(output_image)
      expect(info[:width]).to eq(80)
      expect(info[:height]).to eq(80)  # Aspect ratio preserved
    end

    it 'resizes image to fit height' do
      FastResize.resize(test_image, output_image, height: 60)
      expect(File.exist?(output_image)).to be true

      info = FastResize.image_info(output_image)
      expect(info[:width]).to eq(60)  # Aspect ratio preserved
      expect(info[:height]).to eq(60)
    end

    it 'respects quality parameter for JPEG' do
      jpeg_output = File.join(test_dir, 'output.jpg')
      FastResize.resize(test_image, jpeg_output, width: 50, height: 50, quality: 90)
      expect(File.exist?(jpeg_output)).to be true
    end

    it 'works with different filters' do
      [:mitchell, :catmull_rom, :box, :triangle].each do |filter|
        output = File.join(test_dir, "output_#{filter}.bmp")
        FastResize.resize(test_image, output, width: 50, height: 50, filter: filter)
        expect(File.exist?(output)).to be true
      end
    end

    it 'raises error for invalid filter' do
      expect {
        FastResize.resize(test_image, output_image, width: 50, height: 50, filter: :invalid)
      }.to raise_error(ArgumentError, /Invalid filter/)
    end

    it 'raises error for missing input file' do
      expect {
        FastResize.resize('nonexistent.jpg', output_image, width: 50, height: 50)
      }.to raise_error(RuntimeError)
    end
  end

  describe '.image_info' do
    it 'returns correct image information' do
      info = FastResize.image_info(test_image)

      expect(info).to be_a(Hash)
      expect(info[:width]).to eq(100)
      expect(info[:height]).to eq(100)
      expect(info[:channels]).to be > 0
      expect(info[:format]).to be_a(String)
    end

    it 'returns zero dimensions for missing file' do
      # Note: The C++ implementation returns zeros instead of raising an error
      info = FastResize.image_info('nonexistent.jpg')
      expect(info[:width]).to eq(0)
      expect(info[:height]).to eq(0)
    end
  end

  describe '.resize_with_format' do
    it 'converts BMP to JPEG' do
      jpeg_output = File.join(test_dir, 'output.jpg')
      FastResize.resize_with_format(test_image, jpeg_output, 'jpg', width: 50, height: 50)
      expect(File.exist?(jpeg_output)).to be true

      info = FastResize.image_info(jpeg_output)
      expect(info[:format]).to eq('jpg')
    end

    it 'converts BMP to PNG' do
      png_output = File.join(test_dir, 'output.png')
      FastResize.resize_with_format(test_image, png_output, 'png', width: 50, height: 50)
      expect(File.exist?(png_output)).to be true

      info = FastResize.image_info(png_output)
      expect(info[:format]).to eq('png')
    end

    it 'converts BMP to WEBP' do
      webp_output = File.join(test_dir, 'output.webp')
      FastResize.resize_with_format(test_image, webp_output, 'webp', width: 50, height: 50)
      expect(File.exist?(webp_output)).to be true

      info = FastResize.image_info(webp_output)
      expect(info[:format]).to eq('webp')
    end
  end
end
