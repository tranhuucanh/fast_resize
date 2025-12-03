#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_DIR = File.join(BASE_DIR, 'images/output')

# Clean output directory
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

# Prepare input files
input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.png')).sort

if input_files.empty?
  puts "ERROR: No input files found in #{INPUT_DIR}"
  puts "Please create test images first (img_0001.jpg to img_1000.jpg)"
  exit 1
end

# Get info from first image
sample_image = input_files.first
info = FastResize.image_info(sample_image)
total_input_size = input_files.sum { |f| File.size(f) }

puts "=" * 70
puts "FastResize Benchmark - Resize to width=800 (maintain aspect ratio)"
puts "=" * 70
puts
puts "Input directory: #{INPUT_DIR}"
puts "Sample image: #{File.basename(sample_image)}"
puts "Original size: #{info[:width]}x#{info[:height]}"
puts "Format: #{info[:format]}"
puts "Channels: #{info[:channels]}"
puts

# Calculate target dimensions
target_width = 800
aspect_ratio = info[:height].to_f / info[:width]
target_height = (target_width * aspect_ratio).to_i

puts "Target size: #{target_width}x#{target_height} (maintain aspect ratio)"
puts
puts "Total files: #{input_files.size}"
puts "Total input size: #{(total_input_size / 1024.0 / 1024.0).round(2)}MB"
puts

# Force garbage collection
GC.start
sleep 0.5

# Test 1: Normal Mode
puts "=" * 70
puts "TEST 1: NORMAL MODE (max_speed=false)"
puts "=" * 70
puts

FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

mem_before = `ps -o rss= -p #{Process.pid}`.to_i
cpu_start = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)

time_normal = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: target_width,  # Fixed width
    # height is auto-calculated to maintain aspect ratio
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: false
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
  if result[:failed] > 0
    puts "Errors:"
    result[:errors].first(5).each { |e| puts "  - #{e}" }
  end
end

cpu_end = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)
mem_after = `ps -o rss= -p #{Process.pid}`.to_i
cpu_time_normal = cpu_end - cpu_start
mem_delta_normal = mem_after - mem_before

puts
puts "⏱️  Wall time:    #{(time_normal * 1000).round}ms (#{time_normal.round(3)}s)"
puts "🚀 Throughput:   #{(input_files.size / time_normal).round(2)} images/sec"
puts "📊 Per image:    #{(time_normal / input_files.size * 1000).round(3)}ms"
puts "💻 CPU time:     #{(cpu_time_normal * 1000).round}ms"
puts "⚡ Parallelism:  #{(cpu_time_normal / time_normal).round(2)}x"
puts "💾 Peak RAM:     #{(mem_after / 1024.0).round(2)}MB"
puts "📈 RAM delta:    +#{(mem_delta_normal / 1024.0).round(2)}MB"
puts

# Check output
output_files = Dir.glob(File.join(OUTPUT_DIR, '*.png'))
if output_files.any?
  sample_output = output_files.first
  sample_info = FastResize.image_info(sample_output)
  sample_size = File.size(sample_output)
  total_output_size = output_files.sum { |f| File.size(f) }

  puts "📁 Output files: #{output_files.size}"
  puts "📐 Sample output size: #{sample_info[:width]}x#{sample_info[:height]}"
  puts "📦 Sample file size: #{(sample_size / 1024.0).round(2)}KB"
  puts "💿 Total output: #{(total_output_size / 1024.0 / 1024.0).round(2)}MB"
  puts "🗜️  Size ratio: #{((total_output_size.to_f / total_input_size) * 100).round(2)}%"
end

puts

# Force garbage collection
GC.start
sleep 1

# Test 2: Pipeline Mode
puts "=" * 70
puts "TEST 2: PIPELINE MODE (max_speed=true) 🚀"
puts "=" * 70
puts

FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

mem_before = `ps -o rss= -p #{Process.pid}`.to_i
cpu_start = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)

time_pipeline = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: target_width,  # Fixed width
    # height is auto-calculated to maintain aspect ratio
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: true  # 🚀 PIPELINE MODE
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
  if result[:failed] > 0
    puts "Errors:"
    result[:errors].first(5).each { |e| puts "  - #{e}" }
  end
end

cpu_end = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)
mem_after = `ps -o rss= -p #{Process.pid}`.to_i
cpu_time_pipeline = cpu_end - cpu_start
mem_delta_pipeline = mem_after - mem_before

puts
puts "⏱️  Wall time:    #{(time_pipeline * 1000).round}ms (#{time_pipeline.round(3)}s)"
puts "🚀 Throughput:   #{(input_files.size / time_pipeline).round(2)} images/sec"
puts "📊 Per image:    #{(time_pipeline / input_files.size * 1000).round(3)}ms"
puts "💻 CPU time:     #{(cpu_time_pipeline * 1000).round}ms"
puts "⚡ Parallelism:  #{(cpu_time_pipeline / time_pipeline).round(2)}x"
puts "💾 Peak RAM:     #{(mem_after / 1024.0).round(2)}MB"
puts "📈 RAM delta:    +#{(mem_delta_pipeline / 1024.0).round(2)}MB"
puts

# Check output
output_files = Dir.glob(File.join(OUTPUT_DIR, '*.png'))
if output_files.any?
  sample_output = output_files.first
  sample_info = FastResize.image_info(sample_output)
  sample_size = File.size(sample_output)
  total_output_size = output_files.sum { |f| File.size(f) }

  puts "📁 Output files: #{output_files.size}"
  puts "📐 Sample output size: #{sample_info[:width]}x#{sample_info[:height]}"
  puts "📦 Sample file size: #{(sample_size / 1024.0).round(2)}KB"
  puts "💿 Total output: #{(total_output_size / 1024.0 / 1024.0).round(2)}MB"
  puts "🗜️  Size ratio: #{((total_output_size.to_f / total_input_size) * 100).round(2)}%"
end

puts

# Comparison
puts "=" * 70
puts "PERFORMANCE COMPARISON"
puts "=" * 70
puts
printf "%-25s %18s %18s\n", "Metric", "Normal Mode", "Pipeline Mode"
puts "-" * 70
printf "%-25s %18s %18s\n", "Wall Time",
  "#{(time_normal * 1000).round}ms",
  "#{(time_pipeline * 1000).round}ms"
printf "%-25s %18s %18s\n", "Throughput",
  "#{(input_files.size / time_normal).round(1)} img/s",
  "#{(input_files.size / time_pipeline).round(1)} img/s"
printf "%-25s %18s %18s\n", "Per Image",
  "#{(time_normal / input_files.size * 1000).round(2)}ms",
  "#{(time_pipeline / input_files.size * 1000).round(2)}ms"
printf "%-25s %18s %18s\n", "CPU Time",
  "#{(cpu_time_normal * 1000).round}ms",
  "#{(cpu_time_pipeline * 1000).round}ms"
printf "%-25s %18s %18s\n", "Parallelism",
  "#{(cpu_time_normal / time_normal).round(2)}x",
  "#{(cpu_time_pipeline / time_pipeline).round(2)}x"
printf "%-25s %18s %18s\n", "RAM Delta",
  "+#{(mem_delta_normal / 1024.0).round(2)}MB",
  "+#{(mem_delta_pipeline / 1024.0).round(2)}MB"
puts "-" * 70

speedup = time_normal / time_pipeline
improvement = ((speedup - 1) * 100).round(1)
ram_diff = (mem_delta_pipeline - mem_delta_normal) / 1024.0

puts
if speedup > 1.0
  puts "🚀 SPEEDUP: #{speedup.round(2)}x (#{improvement}% faster)"
  puts "💾 RAM COST: #{ram_diff >= 0 ? '+' : ''}#{ram_diff.round(2)}MB"
  puts

  if speedup >= 4.0
    puts "✅ EXCELLENT! Pipeline delivers 4x+ speedup!"
  elsif speedup >= 2.0
    puts "✅ VERY GOOD! Pipeline delivers 2-4x speedup"
  elsif speedup >= 1.5
    puts "✅ GOOD! Pipeline delivers 1.5-2x speedup"
  elsif speedup >= 1.2
    puts "⚠️  MODERATE: #{improvement}% faster but +#{ram_diff.round(2)}MB RAM"
  else
    puts "⚠️  MINIMAL: Only #{improvement}% faster"
  end
else
  puts "❌ Pipeline is SLOWER: #{((1 - speedup) * 100).round(1)}% slower"
  puts "   Recommendation: Use normal mode (max_speed=false)"
end

puts
puts "=" * 70
puts "Benchmark Complete!"
puts "=" * 70
puts "Images: #{input_files.size} (#{info[:width]}x#{info[:height]} → #{target_width}x#{target_height})"
puts "Output: #{OUTPUT_DIR}"
puts "=" * 70
