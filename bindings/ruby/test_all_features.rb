#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_BASE = File.join(BASE_DIR, 'images/feature_test')

# Clean and create output
FileUtils.rm_rf(OUTPUT_BASE)
FileUtils.mkdir_p(OUTPUT_BASE)

# Get sample PNG
input_file = Dir.glob(File.join(INPUT_DIR, 'img_*.png')).first
info = FastResize.image_info(input_file)

puts "=" * 80
puts "FastResize - Feature Verification Test"
puts "=" * 80
puts
puts "Input: #{File.basename(input_file)}"
puts "Size: #{info[:width]}x#{info[:height]} (#{info[:channels]} channels)"
puts
puts "=" * 80

tests = []

# ============================================
# Test 3.1: Resize by percentage
# ============================================
puts "\n✓ Test 3.1: Resize by PERCENTAGE"
output = File.join(OUTPUT_BASE, 'test_percent_50.jpg')
FastResize.resize(input_file, output, scale: 0.5, quality: 85)
result = FastResize.image_info(output)
expected_w = (info[:width] * 0.5).to_i
expected_h = (info[:height] * 0.5).to_i
success = (result[:width] == expected_w && result[:height] == expected_h)
tests << { name: "Resize 50%", success: success }
puts "  Input:    #{info[:width]}x#{info[:height]}"
puts "  Scale:    50%"
puts "  Output:   #{result[:width]}x#{result[:height]}"
puts "  Expected: #{expected_w}x#{expected_h}"
puts "  Status:   #{success ? '✅ PASS' : '❌ FAIL'}"

# ============================================
# Test 3.2: Resize by width (auto height)
# ============================================
puts "\n✓ Test 3.2: Resize by WIDTH (auto height, maintain aspect ratio)"
output = File.join(OUTPUT_BASE, 'test_width_800.jpg')
FastResize.resize(input_file, output, width: 800, quality: 85)
result = FastResize.image_info(output)
aspect = info[:height].to_f / info[:width]
expected_h = (800 * aspect).to_i
success = (result[:width] == 800 && (result[:height] - expected_h).abs <= 1)
tests << { name: "Resize width=800", success: success }
puts "  Input:    #{info[:width]}x#{info[:height]}"
puts "  Width:    800 (height auto)"
puts "  Output:   #{result[:width]}x#{result[:height]}"
puts "  Expected: 800x#{expected_h}"
puts "  Status:   #{success ? '✅ PASS' : '❌ FAIL'}"

# ============================================
# Test 3.2b: Resize by height (auto width)
# ============================================
puts "\n✓ Test 3.2b: Resize by HEIGHT (auto width, maintain aspect ratio)"
output = File.join(OUTPUT_BASE, 'test_height_600.jpg')
FastResize.resize(input_file, output, height: 600, quality: 85)
result = FastResize.image_info(output)
aspect = info[:width].to_f / info[:height]
expected_w = (600 * aspect).to_i
success = (result[:height] == 600 && (result[:width] - expected_w).abs <= 1)
tests << { name: "Resize height=600", success: success }
puts "  Input:    #{info[:width]}x#{info[:height]}"
puts "  Height:   600 (width auto)"
puts "  Output:   #{result[:width]}x#{result[:height]}"
puts "  Expected: #{expected_w}x600"
puts "  Status:   #{success ? '✅ PASS' : '❌ FAIL'}"

# ============================================
# Test 3.3: Batch with different dimensions per image
# ============================================
puts "\n✓ Test 3.3: BATCH resize with DIFFERENT dimensions per image"

# Get 5 images
input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.png')).first(5)
batch_dir = File.join(OUTPUT_BASE, 'batch_custom')
FileUtils.mkdir_p(batch_dir)

# Define different resize params for each image
custom_params = [
  { width: 800,  height: nil },   # width=800, auto height
  { width: 600,  height: nil },   # width=600, auto height
  { width: 1000, height: nil },   # width=1000, auto height
  { width: 400,  height: 400 },   # exact 400x400
  { scale: 0.3 }                  # 30% scale
]

puts "  Processing #{input_files.size} images with different dimensions..."

results_custom = []
input_files.each_with_index do |input, idx|
  params = custom_params[idx]
  filename = File.basename(input, '.*') + '_custom.jpg'
  output = File.join(batch_dir, filename)

  FastResize.resize(input, output, **params.merge(quality: 85))

  out_info = FastResize.image_info(output)
  results_custom << {
    params: params,
    output: "#{out_info[:width]}x#{out_info[:height]}"
  }
end

puts "\n  Results:"
results_custom.each_with_index do |r, idx|
  params_str = r[:params].map { |k,v| "#{k}=#{v || 'auto'}" }.join(', ')
  puts "    #{idx + 1}. #{params_str.ljust(25)} → #{r[:output]}"
end

# Verify all have different dimensions
unique_sizes = results_custom.map { |r| r[:output] }.uniq.size
success = (unique_sizes >= 4)  # At least 4 different sizes
tests << { name: "Batch custom dimensions", success: success }
puts "  Unique sizes: #{unique_sizes}/#{results_custom.size}"
puts "  Status:       #{success ? '✅ PASS' : '❌ FAIL'}"

# ============================================
# Test 3.3b: Batch with same dimensions (standard batch)
# ============================================
puts "\n✓ Test 3.3b: BATCH resize with SAME dimensions (standard batch)"

batch_dir2 = File.join(OUTPUT_BASE, 'batch_same')
FileUtils.mkdir_p(batch_dir2)

result = FastResize.batch_resize(
  input_files,
  batch_dir2,
  width: 800,
  quality: 85,
  threads: 0
)

puts "  Total:    #{result[:total]}"
puts "  Success:  #{result[:success]}"
puts "  Failed:   #{result[:failed]}"

# Check all outputs have same size
output_files = Dir.glob(File.join(batch_dir2, '*'))
sizes = output_files.map do |f|
  info = FastResize.image_info(f)
  "#{info[:width]}x#{info[:height]}"
end.uniq

success = (sizes.size == 1 && result[:success] == input_files.size)
tests << { name: "Batch same dimensions", success: success }
puts "  All same size: #{sizes.first}"
puts "  Status:        #{success ? '✅ PASS' : '❌ FAIL'}"

# ============================================
# Summary
# ============================================
puts
puts "=" * 80
puts "TEST SUMMARY"
puts "=" * 80

passed = tests.count { |t| t[:success] }
failed = tests.count { |t| !t[:success] }

tests.each_with_index do |t, idx|
  status = t[:success] ? "✅ PASS" : "❌ FAIL"
  puts sprintf("%2d. %-30s %s", idx + 1, t[:name], status)
end

puts
puts "Total:  #{tests.size} tests"
puts "Passed: #{passed} (#{(passed * 100.0 / tests.size).round}%)"
puts "Failed: #{failed}"

if failed == 0
  puts
  puts "🎉 ALL TESTS PASSED!"
  puts
  puts "✓ FastResize supports:"
  puts "  1. Single image resize"
  puts "  2. Batch resize (same dimensions for all)"
  puts "  3. Batch resize (custom dimensions per image)"
  puts "  4. Resize modes:"
  puts "     - By percentage (scale: 0.5 = 50%)"
  puts "     - By width (auto height)"
  puts "     - By height (auto width)"
  puts "     - Exact dimensions"
else
  puts
  puts "❌ SOME TESTS FAILED"
end

puts "=" * 80
