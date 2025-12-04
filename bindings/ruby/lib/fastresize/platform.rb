# frozen_string_literal: true

require 'rbconfig'

module FastResize
  # Platform detection and prebuilt binary management
  module Platform
    class << self
      # Detect the current platform
      # @return [String] Platform identifier (e.g., 'macos-arm64', 'linux-x86_64')
      def detect
        os = RbConfig::CONFIG['host_os']
        arch = RbConfig::CONFIG['host_cpu']

        platform = case os
        when /darwin/
          case arch
          when /arm64|aarch64/ then 'macos-arm64'
          when /x86_64|x64/ then 'macos-x86_64'
          else
            raise "Unsupported macOS architecture: #{arch}"
          end
        when /linux/
          case arch
          when /x86_64|x64/ then 'linux-x86_64'
          when /arm64|aarch64/ then 'linux-aarch64'
          else
            raise "Unsupported Linux architecture: #{arch}"
          end
        else
          raise "Unsupported operating system: #{os}"
        end

        platform
      end

      # Find the prebuilt library for the current platform
      # @return [String] Path to the prebuilt library
      # @raise [RuntimeError] if library not found
      def find_library
        platform = detect
        base_dir = File.expand_path('../../prebuilt', __dir__)
        lib_path = File.join(base_dir, platform, 'lib', 'libfastresize.a')

        # Try direct path first
        return lib_path if File.exist?(lib_path)

        # Try extracting from tarball
        tarball_path = File.join(base_dir, "#{platform}.tar.gz")
        if File.exist?(tarball_path)
          extract_library(tarball_path, File.join(base_dir, platform))
          return lib_path if File.exist?(lib_path)
        end

        raise "Pre-built library not found for #{platform}. " \
              "Please install from source or report this issue."
      end

      # Check if a prebuilt library exists for the current platform
      # @return [Boolean]
      def prebuilt_available?
        platform = detect
        base_dir = File.expand_path('../../prebuilt', __dir__)
        lib_path = File.join(base_dir, platform, 'lib', 'libfastresize.a')

        File.exist?(lib_path) || File.exist?(File.join(base_dir, "#{platform}.tar.gz"))
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

      private

      # Extract library from tarball
      # @param tarball_path [String] Path to tarball
      # @param dest_dir [String] Destination directory
      def extract_library(tarball_path, dest_dir)
        require 'rubygems/package'
        require 'zlib'

        File.open(tarball_path, 'rb') do |file|
          Gem::Package::TarReader.new(Zlib::GzipReader.new(file)) do |tar|
            tar.each do |entry|
              next unless entry.file?

              dest_path = File.join(dest_dir, entry.full_name.sub(%r{^[^/]+/}, ''))
              FileUtils.mkdir_p(File.dirname(dest_path))
              File.open(dest_path, 'wb') do |f|
                f.write(entry.read)
              end
            end
          end
        end
      end
    end
  end
end
