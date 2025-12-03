#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_DIR = File.join(BASE_DIR, 'images/test_output')

# Clean output directory
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

# Get first 100 images
input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.jpg')).sort.first(100)

puts "Testing filter speed with 100 images (3440x1440 → 800x334)"
puts "="*70

# Test with BOX filter
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

time_box = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: 800,
    quality: 85,
    filter: :box,  # Explicit BOX filter
    threads: 0
  )
  puts "BOX filter: #{result[:success]} success, #{result[:failed]} failed"
end

puts "⏱️  BOX filter time: #{(time_box * 1000).round}ms"
puts

# Test with MITCHELL filter
FileUtils.rm_rf(OUTPUT_DIR)
FileUtils.mkdir_p(OUTPUT_DIR)

time_mitchell = Benchmark.realtime do
  result = FastResize.batch_resize(
    input_files,
    OUTPUT_DIR,
    width: 800,
    quality: 85,
    filter: :mitchell,  # Explicit MITCHELL filter
    threads: 0
  )
  puts "MITCHELL filter: #{result[:success]} success, #{result[:failed]} failed"
end

puts "⏱️  MITCHELL filter time: #{(time_mitchell * 1000).round}ms"
puts

speedup = time_mitchell / time_box
puts "="*70
puts "🚀 BOX is #{speedup.round(2)}x faster than MITCHELL"
puts "="*70
