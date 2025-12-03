require 'spec_helper'

RSpec.describe 'FastResize Phase 2 - Resize Operations' do
  let(:test_dir_path) { test_dir }

  describe 'Dimension Calculations' do
    let(:test_image) { File.join(test_dir_path, 'test_2000x1500.bmp') }

    before(:each) do
      create_simple_bmp(2000, 1500, test_image)
    end

    it 'scales by percentage correctly' do
      output = File.join(test_dir_path, 'scaled_50.bmp')
      FastResize.resize(test_image, output, scale: 0.5)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(1000)
      expect(info[:height]).to eq(750)
    end

    it 'fits to width with aspect ratio preservation' do
      output = File.join(test_dir_path, 'fit_width.bmp')
      FastResize.resize(test_image, output, width: 800)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(800)
      expect(info[:height]).to eq(600)  # 800 * (1500/2000) = 600
    end

    it 'fits to height with aspect ratio preservation' do
      output = File.join(test_dir_path, 'fit_height.bmp')
      FastResize.resize(test_image, output, height: 600)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(800)   # 600 * (2000/1500) = 800
      expect(info[:height]).to eq(600)
    end

    it 'creates exact size without aspect ratio' do
      output = File.join(test_dir_path, 'exact_no_aspect.bmp')
      FastResize.resize(test_image, output, width: 800, height: 800, keep_aspect_ratio: false)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(800)
      expect(info[:height]).to eq(800)
    end

    it 'creates exact size with aspect ratio (fits within bounds)' do
      output = File.join(test_dir_path, 'exact_with_aspect.bmp')
      FastResize.resize(test_image, output, width: 800, height: 800, keep_aspect_ratio: true)

      info = FastResize.image_info(output)
      # Should fit within 800x800 maintaining 4:3 ratio
      # Will be 800x600 (width limited)
      expect(info[:width]).to eq(800)
      expect(info[:height]).to eq(600)
    end
  end

  describe 'Edge Cases' do
    it 'handles very small images (1x1)' do
      tiny_image = File.join(test_dir_path, 'tiny.bmp')
      create_simple_bmp(1, 1, tiny_image)

      output = File.join(test_dir_path, 'tiny_upscaled.bmp')
      FastResize.resize(tiny_image, output, width: 10, height: 10)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(10)
      expect(info[:height]).to eq(10)
    end

    it 'handles downscaling to 1x1' do
      large_image = File.join(test_dir_path, 'large.bmp')
      create_simple_bmp(100, 100, large_image)

      output = File.join(test_dir_path, 'tiny_output.bmp')
      FastResize.resize(large_image, output, width: 1, height: 1)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(1)
      expect(info[:height]).to eq(1)
    end

    it 'handles large images (2000x2000)' do
      large_image = File.join(test_dir_path, 'large_2000.bmp')
      create_simple_bmp(2000, 2000, large_image)

      output = File.join(test_dir_path, 'large_resized.bmp')
      FastResize.resize(large_image, output, width: 800, height: 800)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(800)
      expect(info[:height]).to eq(800)
    end

    it 'handles extreme aspect ratio (wide)' do
      wide_image = File.join(test_dir_path, 'wide.bmp')
      create_simple_bmp(1000, 100, wide_image)

      output = File.join(test_dir_path, 'wide_resized.bmp')
      FastResize.resize(wide_image, output, scale: 0.5)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(500)
      expect(info[:height]).to eq(50)
    end

    it 'handles extreme aspect ratio (tall)' do
      tall_image = File.join(test_dir_path, 'tall.bmp')
      create_simple_bmp(100, 1000, tall_image)

      output = File.join(test_dir_path, 'tall_resized.bmp')
      FastResize.resize(tall_image, output, scale: 0.5)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(50)
      expect(info[:height]).to eq(500)
    end
  end

  describe 'Filter Operations' do
    let(:test_image) { File.join(test_dir_path, 'test_filter.bmp') }

    before(:each) do
      create_simple_bmp(400, 400, test_image)
    end

    it 'resizes with Mitchell filter' do
      output = File.join(test_dir_path, 'mitchell.bmp')
      FastResize.resize(test_image, output, width: 200, height: 200, filter: :mitchell)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(200)
    end

    it 'resizes with Catmull-Rom filter' do
      output = File.join(test_dir_path, 'catmull_rom.bmp')
      FastResize.resize(test_image, output, width: 200, height: 200, filter: :catmull_rom)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(200)
    end

    it 'resizes with Box filter' do
      output = File.join(test_dir_path, 'box.bmp')
      FastResize.resize(test_image, output, width: 200, height: 200, filter: :box)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(200)
    end

    it 'resizes with Triangle filter' do
      output = File.join(test_dir_path, 'triangle.bmp')
      FastResize.resize(test_image, output, width: 200, height: 200, filter: :triangle)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(200)
    end
  end

  describe 'Upscaling and Downscaling' do
    it 'upscales 2x correctly' do
      small_image = File.join(test_dir_path, 'small_200.bmp')
      create_simple_bmp(200, 200, small_image)

      output = File.join(test_dir_path, 'upscaled_2x.bmp')
      FastResize.resize(small_image, output, scale: 2.0)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(400)
      expect(info[:height]).to eq(400)
    end

    it 'downscales 4x correctly' do
      large_image = File.join(test_dir_path, 'large_800.bmp')
      create_simple_bmp(800, 800, large_image)

      output = File.join(test_dir_path, 'downscaled_4x.bmp')
      FastResize.resize(large_image, output, scale: 0.25)

      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(200)
    end
  end
end
