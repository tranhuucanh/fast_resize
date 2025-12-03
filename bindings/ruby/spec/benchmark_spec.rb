require 'spec_helper'
require 'benchmark'

RSpec.describe 'FastResize Benchmarks' do
  let(:test_dir_path) { test_dir }

  # Helper to format time
  def format_time(seconds)
    if seconds < 0.001
      "#{(seconds * 1_000_000).round(2)} μs"
    elsif seconds < 1
      "#{(seconds * 1000).round(2)} ms"
    else
      "#{seconds.round(2)} s"
    end
  end

  # Helper to format throughput
  def format_throughput(megapixels_per_sec)
    "#{megapixels_per_sec.round(2)} MP/s"
  end

  describe 'Single Image Resize Performance' do
    it 'benchmarks small image resize (100x100 -> 50x50)' do
      input = File.join(test_dir_path, 'small.bmp')
      output = File.join(test_dir_path, 'small_out.bmp')
      create_simple_bmp(100, 100, input)

      iterations = 100
      elapsed = Benchmark.realtime do
        iterations.times do
          FastResize.resize(input, output, width: 50, height: 50)
        end
      end

      avg_time = elapsed / iterations
      megapixels = (100 * 100) / 1_000_000.0
      throughput = megapixels / avg_time

      puts "\n  Small (100x100 -> 50x50):"
      puts "    Average: #{format_time(avg_time)}"
      puts "    Throughput: #{format_throughput(throughput)}"

      expect(avg_time).to be < 0.1  # Should be under 100ms
    end

    it 'benchmarks medium image resize (800x600 -> 400x300)' do
      input = File.join(test_dir_path, 'medium.bmp')
      output = File.join(test_dir_path, 'medium_out.bmp')
      create_simple_bmp(800, 600, input)

      iterations = 50
      elapsed = Benchmark.realtime do
        iterations.times do
          FastResize.resize(input, output, width: 400, height: 300)
        end
      end

      avg_time = elapsed / iterations
      megapixels = (800 * 600) / 1_000_000.0
      throughput = megapixels / avg_time

      puts "\n  Medium (800x600 -> 400x300):"
      puts "    Average: #{format_time(avg_time)}"
      puts "    Throughput: #{format_throughput(throughput)}"

      expect(avg_time).to be < 0.5
    end

    it 'benchmarks large image resize (2000x2000 -> 800x600)' do
      input = File.join(test_dir_path, 'large.bmp')
      output = File.join(test_dir_path, 'large_out.bmp')
      create_simple_bmp(2000, 2000, input)

      iterations = 20
      elapsed = Benchmark.realtime do
        iterations.times do
          FastResize.resize(input, output, width: 800, height: 600)
        end
      end

      avg_time = elapsed / iterations
      megapixels = (2000 * 2000) / 1_000_000.0
      throughput = megapixels / avg_time

      puts "\n  Large (2000x2000 -> 800x600):"
      puts "    Average: #{format_time(avg_time)}"
      puts "    Throughput: #{format_throughput(throughput)}"

      expect(avg_time).to be < 1.0  # Should be under 1 second
    end
  end

  describe 'Filter Performance Comparison' do
    it 'compares all filters on 2000x2000 -> 800x800' do
      input = File.join(test_dir_path, 'filter_test.bmp')
      create_simple_bmp(2000, 2000, input)

      filters = [:mitchell, :catmull_rom, :box, :triangle]
      results = {}

      puts "\n  Filter Comparison (2000x2000 -> 800x800):"

      filters.each do |filter|
        output = File.join(test_dir_path, "filter_#{filter}.bmp")
        iterations = 20

        elapsed = Benchmark.realtime do
          iterations.times do
            FastResize.resize(input, output, width: 800, height: 800, filter: filter)
          end
        end

        avg_time = elapsed / iterations
        results[filter] = avg_time

        puts "    #{filter.to_s.ljust(15)}: #{format_time(avg_time)}"
      end

      # All filters should complete reasonably fast
      results.each do |filter, time|
        expect(time).to be < 1.0
      end
    end
  end

  describe 'Batch Processing Performance' do
    it 'benchmarks batch resize with different thread counts' do
      # Create 30 images
      inputs = (1..30).map do |i|
        file = File.join(test_dir_path, "batch_#{i}.bmp")
        create_simple_bmp(800, 600, file)
        file
      end

      output_dir = File.join(test_dir_path, 'batch_output')
      thread_counts = [1, 2, 4, 8]

      puts "\n  Batch Processing (30 images, 800x600 -> 400x300):"

      thread_counts.each do |threads|
        FileUtils.rm_rf(output_dir)
        FileUtils.mkdir_p(output_dir)

        elapsed = Benchmark.realtime do
          FastResize.batch_resize(
            inputs,
            output_dir,
            width: 400,
            height: 300,
            threads: threads
          )
        end

        per_image = elapsed / 30
        puts "    #{threads} thread(s): #{format_time(elapsed)} total, #{format_time(per_image)} per image"
      end
    end

    it 'benchmarks large batch (100 images)' do
      skip "Skipping large batch benchmark by default" unless ENV['RUN_LARGE_BENCHMARKS']

      inputs = (1..100).map do |i|
        file = File.join(test_dir_path, "large_batch_#{i}.bmp")
        create_simple_bmp(800, 600, file)
        file
      end

      output_dir = File.join(test_dir_path, 'large_batch_output')
      FileUtils.mkdir_p(output_dir)

      elapsed = Benchmark.realtime do
        result = FastResize.batch_resize(
          inputs,
          output_dir,
          width: 400,
          height: 300,
          threads: 8
        )
        expect(result[:success]).to eq(100)
      end

      per_image = elapsed / 100
      puts "\n  Large Batch (100 images):"
      puts "    Total: #{format_time(elapsed)}"
      puts "    Per image: #{format_time(per_image)}"

      expect(elapsed).to be < 30  # Should complete in under 30 seconds
    end

    it 'benchmarks target scenario (300 images)' do
      skip "Skipping 300 image benchmark by default" unless ENV['RUN_LARGE_BENCHMARKS']

      # This is the target scenario from ARCHITECTURE.md
      # Goal: 300 images (2000x2000 -> 800x600) in < 3 seconds
      inputs = (1..300).map do |i|
        file = File.join(test_dir_path, "target_#{i}.bmp")
        create_simple_bmp(2000, 2000, file)
        file
      end

      output_dir = File.join(test_dir_path, 'target_output')
      FileUtils.mkdir_p(output_dir)

      elapsed = Benchmark.realtime do
        result = FastResize.batch_resize(
          inputs,
          output_dir,
          width: 800,
          height: 600,
          threads: 8
        )
        expect(result[:success]).to eq(300)
      end

      puts "\n  Target Scenario (300 images, 2000x2000 -> 800x600):"
      puts "    Total: #{format_time(elapsed)}"
      puts "    Per image: #{format_time(elapsed / 300)}"
      puts "    Goal: < 3.0 seconds"
      puts "    Result: #{elapsed < 3.0 ? 'PASS' : 'NEEDS OPTIMIZATION'}"

      # This is the performance target
      # Note: May vary based on hardware
      puts "\n    Note: Performance target assumes modern multi-core CPU"
    end
  end

  describe 'Format Performance' do
    it 'compares JPEG quality levels performance' do
      input = File.join(test_dir_path, 'format_test.bmp')
      create_simple_bmp(1000, 1000, input)

      qualities = [10, 50, 75, 90, 95]
      puts "\n  JPEG Quality Performance (1000x1000 -> 500x500):"

      qualities.each do |quality|
        output = File.join(test_dir_path, "quality_#{quality}.jpg")
        iterations = 20

        elapsed = Benchmark.realtime do
          iterations.times do
            FastResize.resize_with_format(
              input, output, 'jpg',
              width: 500, height: 500,
              quality: quality
            )
          end
        end

        avg_time = elapsed / iterations
        file_size = File.size(output)

        puts "    Quality #{quality.to_s.rjust(2)}: #{format_time(avg_time)}, #{(file_size / 1024.0).round(1)} KB"
      end
    end

    it 'compares different output formats performance' do
      input = File.join(test_dir_path, 'format_compare.bmp')
      create_simple_bmp(1000, 1000, input)

      formats = [
        { ext: 'jpg', format: 'jpg' },
        { ext: 'png', format: 'png' },
        { ext: 'webp', format: 'webp' },
        { ext: 'bmp', format: 'bmp' }
      ]

      puts "\n  Format Comparison (1000x1000 -> 500x500):"

      formats.each do |fmt|
        output = File.join(test_dir_path, "output.#{fmt[:ext]}")
        iterations = 20

        elapsed = Benchmark.realtime do
          iterations.times do
            FastResize.resize_with_format(
              input, output, fmt[:format],
              width: 500, height: 500
            )
          end
        end

        avg_time = elapsed / iterations
        file_size = File.size(output)

        puts "    #{fmt[:format].upcase.ljust(6)}: #{format_time(avg_time)}, #{(file_size / 1024.0).round(1)} KB"
      end
    end
  end

  describe 'Upscale vs Downscale Performance' do
    it 'compares upscaling and downscaling performance' do
      puts "\n  Upscale vs Downscale:"

      # Downscale: 2000x2000 -> 800x800
      large_input = File.join(test_dir_path, 'downscale_input.bmp')
      create_simple_bmp(2000, 2000, large_input)

      iterations = 20
      elapsed = Benchmark.realtime do
        iterations.times do
          output = File.join(test_dir_path, 'downscale_out.bmp')
          FastResize.resize(large_input, output, width: 800, height: 800)
        end
      end

      downscale_time = elapsed / iterations
      downscale_throughput = (2000 * 2000 / 1_000_000.0) / downscale_time

      puts "    Downscale (2000x2000 -> 800x800): #{format_time(downscale_time)}, #{format_throughput(downscale_throughput)}"

      # Upscale: 400x400 -> 2000x2000
      small_input = File.join(test_dir_path, 'upscale_input.bmp')
      create_simple_bmp(400, 400, small_input)

      elapsed = Benchmark.realtime do
        iterations.times do
          output = File.join(test_dir_path, 'upscale_out.bmp')
          FastResize.resize(small_input, output, width: 2000, height: 2000)
        end
      end

      upscale_time = elapsed / iterations
      upscale_throughput = (400 * 400 / 1_000_000.0) / upscale_time

      puts "    Upscale (400x400 -> 2000x2000):   #{format_time(upscale_time)}, #{format_throughput(upscale_throughput)}"
    end
  end
end
