#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_DIR = File.join(BASE_DIR, 'images/output')
BASE_IMAGE = File.join(INPUT_DIR, 'base.jpg')

# Get base image dimensions
info = FastResize.image_info(BASE_IMAGE)
puts "=== FastResize 500 Images Benchmark ==="
puts
puts "Base image: #{BASE_IMAGE}"
puts "Dimensions: #{info[:width]}x#{info[:height]}"
puts "Channels: #{info[:channels]}"
puts "Format: #{info[:format]}"
puts "File size: #{File.size(BASE_IMAGE) / 1024}KB"
puts

# Calculate target dimensions (60% size = 0.6 scale)
target_width = (info[:width] * 0.6).to_i
target_height = (info[:height] * 0.6).to_i
puts "Target dimensions: #{target_width}x#{target_height} (60% of original)"
puts

# Step 1: Create 500 copies
puts "Step 1: Creating 500 copies of base image..."
FileUtils.mkdir_p(INPUT_DIR)

time_copy = Benchmark.realtime do
  500.times do |i|
    filename = "img_#{(i+1).to_s.rjust(4, '0')}.jpg"
    FileUtils.cp(BASE_IMAGE, File.join(INPUT_DIR, filename))
  end
end

puts "✓ Created 500 images in #{(time_copy * 1000).round}ms"
puts

# Prepare input files
input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.jpg')).sort
puts "Total input files: #{input_files.size}"
puts

# Step 2: Benchmark Normal Mode
puts "=" * 60
puts "TEST 1: NORMAL MODE (max_speed=false)"
puts "=" * 60
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

GC.start # Clear garbage before benchmark

# Monitor memory before
mem_before = `ps -o rss= -p #{Process.pid}`.to_i
cpu_start = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)

time_normal = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: target_width,
    height: target_height,
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: false
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
  if result[:failed] > 0
    puts "Errors: #{result[:errors].join(', ')}"
  end
end

cpu_end = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)
mem_after = `ps -o rss= -p #{Process.pid}`.to_i
cpu_time_normal = cpu_end - cpu_start

puts
puts "Time: #{(time_normal * 1000).round}ms (#{time_normal.round(2)}s)"
puts "Speed: #{(500 / time_normal).round(2)} images/sec"
puts "Per image: #{(time_normal / 500 * 1000).round(2)}ms"
puts "CPU time: #{(cpu_time_normal * 1000).round}ms"
puts "Peak RAM: #{(mem_after / 1024.0).round(2)}MB (delta: +#{((mem_after - mem_before) / 1024.0).round(2)}MB)"
puts

# Check output
output_files = Dir.glob(File.join(OUTPUT_DIR, '*.jpg'))
sample_output = output_files.first
if sample_output
  sample_size = File.size(sample_output)
  total_output_size = output_files.sum { |f| File.size(f) }
  puts "Output: #{output_files.size} files"
  puts "Sample size: #{(sample_size / 1024.0).round(2)}KB"
  puts "Total output: #{(total_output_size / 1024.0 / 1024.0).round(2)}MB"
end

puts

# Step 3: Benchmark Pipeline Mode (max_speed=true)
puts "=" * 60
puts "TEST 2: PIPELINE MODE (max_speed=true) 🚀"
puts "=" * 60
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

GC.start # Clear garbage before benchmark

# Monitor memory before
mem_before = `ps -o rss= -p #{Process.pid}`.to_i
cpu_start = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)

time_pipeline = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: target_width,
    height: target_height,
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: true  # 🚀 PIPELINE MODE
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
  if result[:failed] > 0
    puts "Errors: #{result[:errors].join(', ')}"
  end
end

cpu_end = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)
mem_after = `ps -o rss= -p #{Process.pid}`.to_i
cpu_time_pipeline = cpu_end - cpu_start

puts
puts "Time: #{(time_pipeline * 1000).round}ms (#{time_pipeline.round(2)}s)"
puts "Speed: #{(500 / time_pipeline).round(2)} images/sec"
puts "Per image: #{(time_pipeline / 500 * 1000).round(2)}ms"
puts "CPU time: #{(cpu_time_pipeline * 1000).round}ms"
puts "Peak RAM: #{(mem_after / 1024.0).round(2)}MB (delta: +#{((mem_after - mem_before) / 1024.0).round(2)}MB)"
puts

# Check output
output_files = Dir.glob(File.join(OUTPUT_DIR, '*.jpg'))
sample_output = output_files.first
if sample_output
  sample_size = File.size(sample_output)
  total_output_size = output_files.sum { |f| File.size(f) }
  puts "Output: #{output_files.size} files"
  puts "Sample size: #{(sample_size / 1024.0).round(2)}KB"
  puts "Total output: #{(total_output_size / 1024.0 / 1024.0).round(2)}MB"
end

puts

# Comparison
puts "=" * 60
puts "COMPARISON"
puts "=" * 60
puts
puts "%-20s %15s %15s %15s" % ["Mode", "Time", "Speed", "CPU Time"]
puts "-" * 60
puts "%-20s %15s %15s %15s" % [
  "Normal",
  "#{(time_normal * 1000).round}ms",
  "#{(500 / time_normal).round(1)} img/s",
  "#{(cpu_time_normal * 1000).round}ms"
]
puts "%-20s %15s %15s %15s" % [
  "Pipeline (max_speed)",
  "#{(time_pipeline * 1000).round}ms",
  "#{(500 / time_pipeline).round(1)} img/s",
  "#{(cpu_time_pipeline * 1000).round}ms"
]
puts "-" * 60

speedup = time_normal / time_pipeline
improvement = ((speedup - 1) * 100).round(1)

puts
puts "🚀 Speedup: #{speedup.round(2)}x (#{improvement}% faster)"
puts

if speedup >= 4.0
  puts "✅ EXCELLENT! Pipeline mode is 4x+ faster"
elsif speedup >= 2.0
  puts "✅ GOOD! Pipeline mode is 2x+ faster"
elsif speedup >= 1.5
  puts "⚠️  MODERATE: Pipeline mode is 1.5x+ faster"
else
  puts "⚠️  LOW SPEEDUP: Check if images are too simple or batch too small"
end

puts
puts "=" * 60
puts "Benchmark complete!"
puts "Output directory: #{OUTPUT_DIR}"
puts "=" * 60
