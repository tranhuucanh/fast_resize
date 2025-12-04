lib = File.expand_path('../bindings/ruby/lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)

Gem::Specification.new do |spec|
  spec.name          = "fast_resize"
  spec.version       = File.read(File.join(__dir__, 'VERSION')).strip
  spec.authors       = ["Tran Huu Canh (0xTh3OKrypt)"]
  spec.email         = ["tranhuucanh39@gmail.com"]

  spec.summary       = "The fastest image resizing library on the planet."
  spec.description   = "Resize 1,000 images in 2 seconds. Up to 2.9x faster than libvips, 3.1x faster than imageflow. Uses 3-4x less RAM than alternatives."
  spec.homepage      = "https://github.com/tranhuucanh/fast_resize"
  spec.license       = "BSD-3-Clause"

  spec.required_ruby_version = ">= 2.5.0"

  # Specify which files should be added to the gem
  files = Dir[
    'README.md',
    'LICENSE',
    'VERSION',
    'CHANGELOG.md',
    'bindings/ruby/lib/**/*',
    'bindings/ruby/ext/**/*.{rb,cpp,h}',
    'include/**/*.{h,hpp}',
    'src/**/*.{cpp,c,h}',
    'CMakeLists.txt'
  ]

  # Include prebuilt binaries if they exist (for faster installation)
  prebuilt_files = Dir.glob("bindings/ruby/prebuilt/**/*").select { |f| File.file?(f) }
  if prebuilt_files.any?
    puts "📦 Including #{prebuilt_files.length} pre-built binary files in gem"
    files += prebuilt_files
  else
    puts "⚠️  No pre-built binaries found, gem will compile from source"
  end

  spec.files         = files
  spec.require_paths = ['bindings/ruby/lib']
  spec.extensions    = ['bindings/ruby/ext/fastresize/extconf.rb']

  # Development dependencies
  spec.add_development_dependency 'rake', '~> 13.0'
  spec.add_development_dependency 'rake-compiler', '~> 1.2'
  spec.add_development_dependency 'rspec', '~> 3.0'
  spec.add_development_dependency 'benchmark-ips', '~> 2.0'

  # Metadata
  spec.metadata = {
    'bug_tracker_uri'   => 'https://github.com/tranhuucanh/fast_resize/issues',
    'changelog_uri'     => 'https://github.com/tranhuucanh/fast_resize/blob/master/CHANGELOG.md',
    'documentation_uri' => 'https://github.com/tranhuucanh/fast_resize',
    'source_code_uri'   => 'https://github.com/tranhuucanh/fast_resize',
    'rubygems_mfa_required' => 'true'
  }
end
