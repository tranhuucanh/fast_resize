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

require 'rbconfig'
require 'fileutils'

module FastResize
  # Platform detection and pre-built binary management
  module Platform
    class << self
      # Detects the current platform
      # @return [String] Platform identifier (e.g., 'macos-arm64', 'linux-x86_64')
      def detect
        os = RbConfig::CONFIG['host_os']
        arch = RbConfig::CONFIG['host_cpu']

        case os
        when /darwin/
          case arch
          when /arm64|aarch64/
            'macos-arm64'
          when /x86_64|x64/
            'macos-x86_64'
          else
            raise "Unsupported macOS architecture: #{arch}"
          end
        when /linux/
          case arch
          when /x86_64|x64/
            'linux-x86_64'
          when /arm64|aarch64/
            'linux-aarch64'
          else
            raise "Unsupported Linux architecture: #{arch}"
          end
        else
          raise "Unsupported platform: #{os}"
        end
      end

      # Check if pre-built binary is available
      # @return [Boolean] true if binary exists
      def prebuilt_available?
        binary_path = find_binary
        binary_path && File.exist?(binary_path)
      rescue StandardError
        false
      end

      # Get binary name
      # @return [String] 'fast_resize'
      def binary_name
        'fast_resize'
      end

      # Extracts pre-built binary from tarball
      # @param tarball_path [String] Path to the tarball
      # @param dest_dir [String] Destination directory
      def extract_binary(tarball_path, dest_dir)
        FileUtils.mkdir_p(dest_dir)
        system("tar -xzf '#{tarball_path}' -C '#{dest_dir}'") or raise "Failed to extract #{tarball_path}"
      end

      # Finds the fast_resize binary
      # @return [String] Path to fast_resize binary
      def find_binary
        platform = detect
        base_dir = File.expand_path('../../prebuilt', __dir__)
        binary_path = File.join(base_dir, platform, 'bin', 'fast_resize')

        if platform.start_with?('linux')
          # Linux: Check if binary exists and is executable
          if File.exist?(binary_path) && File.executable?(binary_path)
            # Test if binary can run (version check)
            begin
              output = `#{binary_path} --version 2>&1`.strip
              if $?.success? && output.include?('FastResize')
                return binary_path
              end
            rescue => e
              # Binary failed, continue to fallback
            end
          end
        else
          # macOS: Use regular binary
          return binary_path if File.exist?(binary_path)
        end

        # Try to extract from tarball
        tarball_path = File.join(base_dir, "#{platform}.tar.gz")
        if File.exist?(tarball_path)
          puts "Extracting pre-built binary from #{tarball_path}..."
          extract_binary(tarball_path, File.join(base_dir, platform))

          if File.exist?(binary_path)
            File.chmod(0755, binary_path)
            return binary_path
          end
        end

        raise "Pre-built binary not found for #{platform}"
      end

      # Get human-readable platform information
      # @return [Hash] Platform details
      def info
        {
          platform: detect,
          os: RbConfig::CONFIG['host_os'],
          arch: RbConfig::CONFIG['host_cpu'],
          ruby_version: RUBY_VERSION,
          prebuilt_available: prebuilt_available?
        }
      end
    end
  end
end
