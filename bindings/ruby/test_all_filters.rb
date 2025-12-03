#!/usr/bin/env ruby
require_relative 'lib/fastresize'
require 'fileutils'
require 'benchmark'

BASE_DIR = '/Users/canh.th/Desktop/fastgems/fastresize'
INPUT_DIR = File.join(BASE_DIR, 'images/input')
OUTPUT_DIR = File.join(BASE_DIR, 'images/test_output')

input_files = Dir.glob(File.join(INPUT_DIR, 'img_*.jpg')).sort.first(100)

puts "Testing all filters with 100 images (3440x1440 → 800x334)"
puts "="*70

[:box, :triangle, :mitchell, :catmull_rom].each do |filter|
  FileUtils.rm_rf(OUTPUT_DIR)
  FileUtils.mkdir_p(OUTPUT_DIR)
  
  time = Benchmark.realtime do
    FastResize.batch_resize(input_files, OUTPUT_DIR, width: 800, quality: 85, filter: filter, threads: 0)
  end
  
  puts "#{filter.to_s.upcase.ljust(15)}: #{(time * 1000).round}ms"
end
