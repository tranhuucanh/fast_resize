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

# Check if pre-built binary exists
def check_prebuilt_binary
  os = RbConfig::CONFIG['host_os']
  arch = RbConfig::CONFIG['host_cpu']

  platform = case os
  when /darwin/
    case arch
    when /arm64|aarch64/
      'macos-arm64'
    when /x86_64|x64/
      'macos-x86_64'
    else
      nil
    end
  when /linux/
    case arch
    when /x86_64|x64/
      'linux-x86_64'
    when /arm64|aarch64/
      'linux-aarch64'
    else
      nil
    end
  else
    nil
  end

  return false unless platform

  # When installed as gem, binaries are in bindings/ruby/prebuilt/
  ext_dir = File.dirname(__FILE__)
  prebuilt_dir = File.expand_path("../../prebuilt/#{platform}", ext_dir)
  binary_path = File.join(prebuilt_dir, 'bin', 'fast_resize')

  if File.exist?(binary_path)
    puts "✅ Found pre-built binary at #{binary_path}"
    puts "⏭️  Skipping compilation"

    # Create a dummy Makefile that does nothing
    File.open('Makefile', 'w') do |f|
      f.puts "all:\n\t@echo 'Using pre-built binary'\n"
      f.puts "install:\n\t@echo 'Using pre-built binary'\n"
      f.puts "clean:\n\t@echo 'Nothing to clean'\n"
    end

    return true
  end

  # Check for tarball
  tarball_path = File.join(File.expand_path("../../prebuilt", ext_dir), "#{platform}.tar.gz")
  if File.exist?(tarball_path)
    puts "📦 Found tarball at #{tarball_path}"
    puts "📦 Extracting pre-built binary..."

    require 'fileutils'
    FileUtils.mkdir_p(prebuilt_dir)
    system("tar -xzf '#{tarball_path}' -C '#{File.dirname(prebuilt_dir)}'")

    if File.exist?(binary_path)
      File.chmod(0755, binary_path)
      puts "✅ Extracted pre-built binary to #{binary_path}"
      puts "⏭️  Skipping compilation"

      # Create a dummy Makefile that does nothing
      File.open('Makefile', 'w') do |f|
        f.puts "all:\n\t@echo 'Using pre-built binary'\n"
        f.puts "install:\n\t@echo 'Using pre-built binary'\n"
        f.puts "clean:\n\t@echo 'Nothing to clean'\n"
      end

      return true
    end
  end

  false
end

# Try to use pre-built binary first
exit 0 if check_prebuilt_binary

# If no pre-built binary, show error message
# We don't support compiling from source in this version
puts "❌ No pre-built binary found for this platform"
puts ""
puts "FastResize requires a pre-built binary. Please check:"
puts "  1. Your platform is supported (macOS/Linux, x86_64/ARM64)"
puts "  2. The gem was properly downloaded with all files"
puts "  3. Report this issue at: https://github.com/tranhuucanh/fast_resize/issues"
puts ""

# Create a Makefile that will fail gracefully
File.open('Makefile', 'w') do |f|
  f.puts "all:"
  f.puts "\t@echo 'ERROR: No pre-built binary available for this platform'"
  f.puts "\t@exit 1"
  f.puts "install:"
  f.puts "\t@echo 'ERROR: No pre-built binary available for this platform'"
  f.puts "\t@exit 1"
  f.puts "clean:"
  f.puts "\t@echo 'Nothing to clean'"
end

exit 1
