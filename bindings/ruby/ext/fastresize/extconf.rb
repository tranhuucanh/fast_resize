# FastResize - The Fastest Image Resizing Library On The Planet
# Copyright (C) 2025 Tran Huu Canh (0xTh3OKrypt) and FastResize Contributors
#
# Resize 1,000 images in 2 seconds. Up to 2.9x faster than libvips,
# 3.1x faster than imageflow. Uses 3-4x less RAM than alternatives.
#
# Author: Tran Huu Canh (0xTh3OKrypt)
# Email: tranhuucanh39@gmail.com
# Homepage: https://github.com/tranhuucanh/fast_resize
#
# BSD 3-Clause License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require 'mkmf'
require 'rbconfig'

def check_prebuilt_library
  puts "🔍 Checking for pre-built library..."

  os = RbConfig::CONFIG['host_os']
  arch = RbConfig::CONFIG['host_cpu']

  platform = case os
  when /darwin/
    case arch
    when /arm64|aarch64/ then 'macos-arm64'
    when /x86_64|x64/ then 'macos-x86_64'
    end
  when /linux/
    case arch
    when /x86_64|x64/ then 'linux-x86_64'
    when /arm64|aarch64/ then 'linux-aarch64'
    end
  end

  return false unless platform

  ext_dir = File.dirname(__FILE__)
  prebuilt_dir = File.expand_path("../../prebuilt/#{platform}", ext_dir)
  lib_path = File.join(prebuilt_dir, 'lib', 'libfastresize.a')

  if File.exist?(lib_path)
    puts "✅ Found pre-built library at #{lib_path}"
    puts "⚡ Skipping compilation (using pre-built binary)"

    File.open('Makefile', 'w') do |f|
      f.puts "all:"
      f.puts "\t@echo '✅ Using pre-built library for #{platform}'"
      f.puts ""
      f.puts "install:"
      f.puts "\t@echo '✅ Using pre-built library for #{platform}'"
      f.puts ""
      f.puts "clean:"
      f.puts "\t@echo 'Nothing to clean'"
    end

    return true
  end

  puts "⚠️  No pre-built library found for #{platform}"
  puts "📦 Will compile from source..."
  false
end

if check_prebuilt_library
  puts "🎉 Installation complete (pre-built binary)!"
  exit 0
end

puts "🔨 Compiling FastResize from source..."

$CXXFLAGS << " -std=c++14"

$CXXFLAGS << " -O3"

ext_dir = File.dirname(__FILE__)
project_root = File.expand_path('../../../../', ext_dir)
include_dir = File.join(project_root, 'include')
src_dir = File.join(project_root, 'src')

unless File.directory?(include_dir)
  abort "❌ Include directory not found: #{include_dir}"
end

unless File.directory?(src_dir)
  abort "❌ Source directory not found: #{src_dir}"
end

puts "📂 Building from source tree: #{project_root}"

$INCFLAGS << " -I#{include_dir}"

['/opt/homebrew/include', '/usr/local/include'].each do |path|
  $INCFLAGS << " -I#{path}" if File.directory?(path)
end

puts "🔍 Searching for required libraries..."

unless find_library('jpeg', 'jpeg_CreateDecompress', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
  abort "❌ ERROR: libjpeg not found. Please install: brew install jpeg (macOS) or apt-get install libjpeg-dev (Linux)"
end

unless find_library('png', 'png_create_read_struct', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
  abort "❌ ERROR: libpng not found. Please install: brew install libpng (macOS) or apt-get install libpng-dev (Linux)"
end

unless find_library('z', 'inflate', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
  abort "❌ ERROR: zlib not found. Please install: brew install zlib (macOS) or apt-get install zlib1g-dev (Linux)"
end

if find_library('webp', 'WebPDecode', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
  puts "✅ WebP support enabled"
  find_library('webpdemux', 'WebPDemuxGetFrame', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
  find_library('sharpyuv', 'SharpYuvGetCPUInfo', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
else
  puts "⚠️  WebP support disabled (library not found)"
end

$srcs = ['fastresize_ext.cpp'] + Dir.glob(File.join(src_dir, '*.cpp')).map { |f| File.basename(f) }
$VPATH << src_dir

puts "📝 Generating Makefile..."

create_makefile('fastresize/fastresize_ext')

puts "✅ Makefile generated successfully!"
puts "💡 Run 'make' to compile the extension"
