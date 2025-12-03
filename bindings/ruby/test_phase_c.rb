#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

# Setup test images
TEST_DIR = '/tmp/ruby_phase_c_test'
FileUtils.rm_rf(TEST_DIR)
FileUtils.mkdir_p(TEST_DIR)

puts "=== Ruby Phase C Binding Test ==="
puts

# Create test images using system convert
puts "Creating 100 test images..."
100.times do |i|
  `convert -size 2000x2000 gradient:blue-red #{TEST_DIR}/input_#{i.to_s.rjust(3, '0')}.jpg`
end
puts "✓ Created 100 test images"
puts

# Prepare input files
input_files = Dir.glob("#{TEST_DIR}/input_*.jpg").sort

# Test 1: Normal mode (max_speed=false)
puts "=== Test 1: Normal Mode (Thread Pool) ==="
output_dir_1 = "#{TEST_DIR}/output_normal"
FileUtils.mkdir_p(output_dir_1)

time_normal = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    output_dir_1,
    width: 800,
    height: 600,
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: false  # Normal mode
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
end

puts "Time: #{(time_normal * 1000).round}ms"
puts "Speed: #{(100 / time_normal).round(2)} images/sec"
puts

# Cleanup output
FileUtils.rm_rf(output_dir_1)

# Test 2: Pipeline mode (max_speed=true)
puts "=== Test 2: Pipeline Mode (max_speed=true) ==="
output_dir_2 = "#{TEST_DIR}/output_pipeline"
FileUtils.mkdir_p(output_dir_2)

time_pipeline = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    output_dir_2,
    width: 800,
    height: 600,
    quality: 85,
    threads: 0,  # Auto-detect
    max_speed: true  # 🚀 Pipeline mode!
  )

  puts "Total: #{result[:total]}"
  puts "Success: #{result[:success]}"
  puts "Failed: #{result[:failed]}"
end

puts "Time: #{(time_pipeline * 1000).round}ms"
puts "Speed: #{(100 / time_pipeline).round(2)} images/sec"
puts

# Comparison
puts "=== Comparison ==="
puts "Normal mode: #{(time_normal * 1000).round}ms"
puts "Pipeline mode: #{(time_pipeline * 1000).round}ms"
speedup = time_normal / time_pipeline
puts "Speedup: #{speedup.round(2)}x 🚀"
puts

# Cleanup
FileUtils.rm_rf(TEST_DIR)
puts "✓ Test complete!"
