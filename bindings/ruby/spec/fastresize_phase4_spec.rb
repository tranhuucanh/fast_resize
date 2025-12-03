require 'spec_helper'

RSpec.describe 'FastResize Phase 4 - Batch Processing' do
  let(:test_dir_path) { test_dir }

  describe '.batch_resize' do
    let(:input_files) do
      (1..10).map do |i|
        file = File.join(test_dir_path, "input_#{i}.bmp")
        create_simple_bmp(400, 300, file)
        file
      end
    end

    let(:output_dir) { File.join(test_dir_path, 'output') }

    before(:each) do
      FileUtils.mkdir_p(output_dir)
    end

    it 'resizes multiple images with same options' do
      result = FastResize.batch_resize(
        input_files,
        output_dir,
        width: 200,
        height: 150
      )

      expect(result[:total]).to eq(10)
      expect(result[:success]).to eq(10)
      expect(result[:failed]).to eq(0)
      expect(result[:errors]).to be_empty

      # Verify outputs
      input_files.each do |input|
        basename = File.basename(input)
        output = File.join(output_dir, basename)
        expect(File.exist?(output)).to be true

        info = FastResize.image_info(output)
        expect(info[:width]).to eq(200)
        expect(info[:height]).to eq(150)
      end
    end

    it 'resizes images with percentage scaling' do
      result = FastResize.batch_resize(
        input_files,
        output_dir,
        scale: 0.5
      )

      expect(result[:total]).to eq(10)
      expect(result[:success]).to eq(10)
      expect(result[:failed]).to eq(0)

      # Verify first output
      output = File.join(output_dir, File.basename(input_files[0]))
      info = FastResize.image_info(output)
      expect(info[:width]).to eq(200)
      expect(info[:height]).to eq(150)
    end

    it 'respects thread count parameter' do
      result = FastResize.batch_resize(
        input_files,
        output_dir,
        width: 200,
        height: 150,
        threads: 4
      )

      expect(result[:total]).to eq(10)
      expect(result[:success]).to eq(10)
      expect(result[:failed]).to eq(0)
    end

    it 'handles errors gracefully (continue on error)' do
      # Add a non-existent file
      bad_files = input_files + ['nonexistent.jpg']

      result = FastResize.batch_resize(
        bad_files,
        output_dir,
        width: 200,
        height: 150,
        stop_on_error: false
      )

      expect(result[:total]).to eq(11)
      expect(result[:success]).to eq(10)
      expect(result[:failed]).to eq(1)
      expect(result[:errors].size).to eq(1)
    end

    it 'stops on first error when requested' do
      # Create files where one in the middle will fail
      mixed_files = input_files[0..2] + ['nonexistent.jpg'] + input_files[3..5]

      result = FastResize.batch_resize(
        mixed_files,
        output_dir,
        width: 200,
        height: 150,
        stop_on_error: true
      )

      expect(result[:failed]).to be > 0
      expect(result[:errors]).not_to be_empty
    end

    it 'handles large batches efficiently' do
      large_batch = (1..50).map do |i|
        file = File.join(test_dir_path, "large_batch_#{i}.bmp")
        create_simple_bmp(200, 200, file)
        file
      end

      start_time = Time.now
      result = FastResize.batch_resize(
        large_batch,
        output_dir,
        width: 100,
        height: 100,
        threads: 8
      )
      elapsed = Time.now - start_time

      expect(result[:total]).to eq(50)
      expect(result[:success]).to eq(50)
      expect(result[:failed]).to eq(0)

      # Should complete reasonably fast (adjust based on hardware)
      # This is mainly to ensure it doesn't hang
      expect(elapsed).to be < 30
    end

    it 'works with different output formats' do
      # Create output dir for different formats
      jpg_dir = File.join(test_dir_path, 'jpg_output')
      FileUtils.mkdir_p(jpg_dir)

      # Note: batch_resize uses input extensions by default
      # So we need to convert manually or use batch_resize_custom
      inputs_jpg = input_files.map.with_index do |file, i|
        jpg_file = File.join(test_dir_path, "input_#{i + 1}.jpg")
        FastResize.resize_with_format(file, jpg_file, 'jpg', width: 400, height: 300)
        jpg_file
      end

      result = FastResize.batch_resize(
        inputs_jpg,
        jpg_dir,
        width: 200,
        height: 150,
        quality: 90
      )

      expect(result[:success]).to eq(10)

      # Verify JPEG output
      output = File.join(jpg_dir, File.basename(inputs_jpg[0]))
      info = FastResize.image_info(output)
      expect(info[:format]).to eq('jpg')
    end
  end

  describe '.batch_resize_custom' do
    it 'resizes images with individual options' do
      items = [
        {
          input: File.join(test_dir_path, 'img1.bmp'),
          output: File.join(test_dir_path, 'out1.bmp'),
          width: 100,
          height: 100
        },
        {
          input: File.join(test_dir_path, 'img2.bmp'),
          output: File.join(test_dir_path, 'out2.bmp'),
          width: 200,
          height: 200
        },
        {
          input: File.join(test_dir_path, 'img3.bmp'),
          output: File.join(test_dir_path, 'out3.bmp'),
          scale: 0.5
        }
      ]

      # Create input images
      create_simple_bmp(400, 400, items[0][:input])
      create_simple_bmp(400, 400, items[1][:input])
      create_simple_bmp(400, 400, items[2][:input])

      result = FastResize.batch_resize_custom(items)

      expect(result[:total]).to eq(3)
      expect(result[:success]).to eq(3)
      expect(result[:failed]).to eq(0)

      # Verify individual outputs
      info1 = FastResize.image_info(items[0][:output])
      expect(info1[:width]).to eq(100)
      expect(info1[:height]).to eq(100)

      info2 = FastResize.image_info(items[1][:output])
      expect(info2[:width]).to eq(200)
      expect(info2[:height]).to eq(200)

      info3 = FastResize.image_info(items[2][:output])
      expect(info3[:width]).to eq(200)
      expect(info3[:height]).to eq(200)
    end

    it 'handles different formats per image' do
      items = [
        {
          input: File.join(test_dir_path, 'base.bmp'),
          output: File.join(test_dir_path, 'out.jpg'),
          width: 100,
          height: 100
        },
        {
          input: File.join(test_dir_path, 'base.bmp'),
          output: File.join(test_dir_path, 'out.png'),
          width: 100,
          height: 100
        },
        {
          input: File.join(test_dir_path, 'base.bmp'),
          output: File.join(test_dir_path, 'out.webp'),
          width: 100,
          height: 100
        }
      ]

      # Create input image
      create_simple_bmp(400, 400, items[0][:input])

      result = FastResize.batch_resize_custom(items)

      expect(result[:total]).to eq(3)
      expect(result[:success]).to eq(3)

      expect(FastResize.image_info(items[0][:output])[:format]).to eq('jpg')
      expect(FastResize.image_info(items[1][:output])[:format]).to eq('png')
      expect(FastResize.image_info(items[2][:output])[:format]).to eq('webp')
    end

    it 'applies different filters per image' do
      items = [:mitchell, :catmull_rom, :box, :triangle].map.with_index do |filter, i|
        input_file = File.join(test_dir_path, "filter_input_#{i}.bmp")
        create_simple_bmp(400, 400, input_file)

        {
          input: input_file,
          output: File.join(test_dir_path, "filter_out_#{filter}.bmp"),
          width: 200,
          height: 200,
          filter: filter
        }
      end

      result = FastResize.batch_resize_custom(items)

      expect(result[:total]).to eq(4)
      expect(result[:success]).to eq(4)

      # All outputs should exist
      items.each do |item|
        expect(File.exist?(item[:output])).to be true
      end
    end

    it 'handles custom thread count' do
      items = (1..20).map do |i|
        input_file = File.join(test_dir_path, "custom_#{i}.bmp")
        create_simple_bmp(200, 200, input_file)

        {
          input: input_file,
          output: File.join(test_dir_path, "custom_out_#{i}.bmp"),
          width: 100,
          height: 100
        }
      end

      result = FastResize.batch_resize_custom(items, threads: 4)

      expect(result[:total]).to eq(20)
      expect(result[:success]).to eq(20)
    end

    it 'raises error for missing required keys' do
      items = [
        { input: 'test.bmp' }  # Missing output
      ]

      expect {
        FastResize.batch_resize_custom(items)
      }.to raise_error(ArgumentError, /missing 'output' key/)
    end
  end

  describe 'Performance' do
    it 'parallel processing is faster than sequential (with many images)' do
      # Create 30 test images
      batch_files = (1..30).map do |i|
        file = File.join(test_dir_path, "perf_#{i}.bmp")
        create_simple_bmp(800, 600, file)
        file
      end

      output_dir = File.join(test_dir_path, 'perf_output')
      FileUtils.mkdir_p(output_dir)

      # Time with multiple threads
      start_time = Time.now
      result = FastResize.batch_resize(
        batch_files,
        output_dir,
        width: 400,
        height: 300,
        threads: 8
      )
      parallel_time = Time.now - start_time

      expect(result[:success]).to eq(30)

      # Clean output for second run
      FileUtils.rm_rf(output_dir)
      FileUtils.mkdir_p(output_dir)

      # Time with single thread
      start_time = Time.now
      result = FastResize.batch_resize(
        batch_files,
        output_dir,
        width: 400,
        height: 300,
        threads: 1
      )
      sequential_time = Time.now - start_time

      expect(result[:success]).to eq(30)

      # Parallel should be significantly faster (at least 1.5x on multi-core)
      # This is a rough check - actual speedup depends on CPU cores
      puts "Sequential: #{sequential_time.round(2)}s, Parallel: #{parallel_time.round(2)}s, Speedup: #{(sequential_time / parallel_time).round(2)}x"
      expect(parallel_time).to be < sequential_time
    end
  end
end
