require 'mkmf'

# C++14 standard required
$CXXFLAGS << " -std=c++14"

# Add optimization flags
$CXXFLAGS << " -O3"

# Find project directories
ext_dir = File.dirname(__FILE__)
project_root = File.expand_path('../../../../', ext_dir)
include_dir = File.join(project_root, 'include')
src_dir = File.join(project_root, 'src')

unless File.directory?(include_dir)
  abort "Include directory not found: #{include_dir}"
end

unless File.directory?(src_dir)
  abort "Source directory not found: #{src_dir}"
end

puts "Building from source tree: #{project_root}"

# Add include directory
$INCFLAGS << " -I#{include_dir}"

# Find dependencies - add include paths
['/opt/homebrew/include', '/usr/local/include'].each do |path|
  $INCFLAGS << " -I#{path}" if File.directory?(path)
end

# Find libraries
find_library('jpeg', 'jpeg_CreateDecompress', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
find_library('png', 'png_create_read_struct', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
find_library('webp', 'WebPDecode', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
find_library('webpdemux', 'WebPDemuxGetFrame', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')
find_library('z', 'inflate', '/opt/homebrew/lib', '/usr/local/lib', '/usr/lib')

# Add C++ source files to be compiled with the extension
# Include both the extension file and the library sources
$srcs = ['fastresize_ext.cpp'] + Dir.glob(File.join(src_dir, '*.cpp')).map { |f| File.basename(f) }
$VPATH << src_dir

# Create Makefile
create_makefile('fastresize/fastresize_ext')
