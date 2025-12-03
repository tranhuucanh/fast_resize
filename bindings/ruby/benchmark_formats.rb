#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_BASE = File.join(BASE_DIR, 'images/test_formats')

# Test với 100 images để nhanh hơn
NUM_IMAGES = 100

# Get input files (PNG)
input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.png')).sort.first(NUM_IMAGES)

if input_files.empty?
  puts "ERROR: No PNG files found in #{INPUT_DIR}"
  exit 1
end

# Get sample image info
sample = FastResize.image_info(input_files.first)

puts "="*80
puts "FastResize Format Benchmark - Compare JPEG vs PNG vs WEBP vs BMP"
puts "="*80
puts
puts "Input: #{NUM_IMAGES} PNG images"
puts "Original size: #{sample[:width]}x#{sample[:height]} (#{sample[:channels]} channels)"
puts "Resize to: 800x450 (maintain aspect ratio)"
puts
puts "="*80

formats = [
  { name: 'JPEG', ext: 'jpg', quality: 85 },
  { name: 'PNG', ext: 'png', quality: 85 },
  { name: 'WEBP', ext: 'webp', quality: 85 },
  { name: 'BMP', ext: 'bmp', quality: 85 }
]

results = []

formats.each do |fmt|
  output_dir = File.join(OUTPUT_BASE, fmt[:ext])
  FileUtils.rm_rf(output_dir)
  FileUtils.mkdir_p(output_dir)

  puts "\n📊 Testing #{fmt[:name]} format..."

  # Force garbage collection
  GC.start
  sleep 0.2

  # Measure
  mem_before = `ps -o rss= -p #{Process.pid}`.to_i
  cpu_start = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)

  time = Benchmark.realtime do
    # Process each file individually to control output format
    success = 0
    failed = 0

    input_files.each do |input_path|
      filename = File.basename(input_path, '.*') + '.' + fmt[:ext]
      output_path = File.join(output_dir, filename)

      begin
        FastResize.resize(
          input_path,
          output_path,
          width: 800,
          quality: fmt[:quality]
        )
        success += 1
      rescue => e
        failed += 1
      end
    end

    result = { total: input_files.size, success: success, failed: failed, errors: [] }

    if result[:failed] > 0
      puts "  ⚠️  Failed: #{result[:failed]} images"
      if result[:errors].any?
        puts "  Errors: #{result[:errors].first(3).join(', ')}"
      end
    end
  end

  cpu_end = Process.clock_gettime(Process::CLOCK_PROCESS_CPUTIME_ID)
  mem_after = `ps -o rss= -p #{Process.pid}`.to_i

  cpu_time = cpu_end - cpu_start
  mem_delta = mem_after - mem_before

  # Check output
  output_files = Dir.glob(File.join(output_dir, "*"))
  total_size = output_files.sum { |f| File.size(f) }
  avg_size = output_files.any? ? total_size / output_files.size : 0

  results << {
    format: fmt[:name],
    time_ms: (time * 1000).round,
    time_sec: time.round(3),
    throughput: (NUM_IMAGES / time).round(1),
    per_image_ms: (time / NUM_IMAGES * 1000).round(2),
    cpu_time_ms: (cpu_time * 1000).round,
    parallelism: (cpu_time / time).round(2),
    files: output_files.size,
    total_size_mb: (total_size / 1024.0 / 1024.0).round(2),
    avg_size_kb: (avg_size / 1024.0).round(1),
    mem_delta_mb: (mem_delta / 1024.0).round(2)
  }

  puts "  ⏱️  Time: #{results.last[:time_ms]}ms"
  puts "  🚀 Throughput: #{results.last[:throughput]} img/s"
  puts "  📊 Per image: #{results.last[:per_image_ms]}ms"
  puts "  💾 Avg file size: #{results.last[:avg_size_kb]}KB"
end

# Summary
puts
puts "="*80
puts "BENCHMARK RESULTS SUMMARY"
puts "="*80
puts

# Sort by speed (fastest first)
sorted = results.sort_by { |r| r[:time_ms] }
fastest = sorted.first[:time_ms]

puts sprintf("%-10s %12s %15s %15s %18s",
             "Format", "Time", "Throughput", "Per Image", "Avg File Size")
puts "-"*80

sorted.each do |r|
  speedup = fastest.to_f / r[:time_ms]
  speed_indicator = speedup >= 1.0 ? "✅" : "  "

  puts sprintf("%-10s %10dms %13.1f/s %13.2fms %15.1fKB   %s%.2fx",
               r[:format],
               r[:time_ms],
               r[:throughput],
               r[:per_image_ms],
               r[:avg_size_kb],
               speed_indicator,
               speedup)
end

puts
puts "="*80
puts "SPEED RANKING (Fastest to Slowest)"
puts "="*80
sorted.each_with_index do |r, idx|
  speedup = fastest.to_f / r[:time_ms]

  if idx == 0
    puts "🥇 1. #{r[:format]}: #{r[:time_ms]}ms (baseline)"
  elsif idx == 1
    slowdown = ((r[:time_ms].to_f / fastest - 1) * 100).round(1)
    puts "🥈 2. #{r[:format]}: #{r[:time_ms]}ms (#{slowdown}% slower)"
  elsif idx == 2
    slowdown = ((r[:time_ms].to_f / fastest - 1) * 100).round(1)
    puts "🥉 3. #{r[:format]}: #{r[:time_ms]}ms (#{slowdown}% slower)"
  else
    slowdown = ((r[:time_ms].to_f / fastest - 1) * 100).round(1)
    puts "   #{idx + 1}. #{r[:format]}: #{r[:time_ms]}ms (#{slowdown}% slower)"
  end
end

puts
puts "="*80
puts "FILE SIZE COMPARISON"
puts "="*80
size_sorted = results.sort_by { |r| r[:avg_size_kb] }

size_sorted.each_with_index do |r, idx|
  ratio = r[:avg_size_kb].to_f / size_sorted.last[:avg_size_kb] * 100

  puts sprintf("%d. %-10s %8.1fKB  (%5.1f%% of largest)",
               idx + 1,
               r[:format],
               r[:avg_size_kb],
               ratio)
end

puts
puts "="*80
puts "KEY INSIGHTS"
puts "="*80

fastest_fmt = sorted.first
slowest_fmt = sorted.last
smallest_fmt = size_sorted.first
largest_fmt = size_sorted.last

puts "🚀 Fastest format: #{fastest_fmt[:format]} (#{fastest_fmt[:throughput]} img/s)"
puts "🐌 Slowest format: #{slowest_fmt[:format]} (#{slowest_fmt[:throughput]} img/s)"
puts "   Speed difference: #{((slowest_fmt[:time_ms].to_f / fastest_fmt[:time_ms]) - 1).round(1) * 100}% slower"
puts
puts "💾 Smallest files: #{smallest_fmt[:format]} (#{smallest_fmt[:avg_size_kb]}KB avg)"
puts "📦 Largest files: #{largest_fmt[:format]} (#{largest_fmt[:avg_size_kb]}KB avg)"
puts "   Size difference: #{((largest_fmt[:avg_size_kb].to_f / smallest_fmt[:avg_size_kb])).round(1)}x larger"
puts
puts "="*80
