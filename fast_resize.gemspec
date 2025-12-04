# frozen_string_literal: true

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

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage
  spec.metadata["changelog_uri"] = "#{spec.homepage}/blob/master/CHANGELOG.md"
  spec.metadata["bug_tracker_uri"] = "#{spec.homepage}/issues"

  # Specify which files should be added to the gem
  spec.files = Dir.chdir(File.expand_path(__dir__)) do
    files = [
      'README.md',
      'LICENSE',
      'VERSION',
      'bindings/ruby/lib/**/*.rb',
      'bindings/ruby/ext/fastresize/extconf.rb'
    ].flat_map { |pattern| Dir.glob(pattern) }

    # Include pre-built binaries if they exist (for faster installation)
    # This is for CI builds where binaries are downloaded before gem build
    prebuilt_files = Dir.glob("bindings/ruby/prebuilt/**/*").select { |f| File.file?(f) }
    if prebuilt_files.any?
      puts "📦 Including #{prebuilt_files.length} pre-built binary files in gem"
      files += prebuilt_files
    end

    files
  end

  spec.extensions    = ["bindings/ruby/ext/fastresize/extconf.rb"]
  spec.require_paths = ['bindings/ruby/lib']

  # No runtime dependencies - uses pre-built CLI binary

  spec.add_development_dependency 'rake', '~> 13.0'
  spec.add_development_dependency 'rake-compiler', '~> 1.2'
  spec.add_development_dependency 'rspec', '~> 3.0'
  spec.add_development_dependency 'benchmark-ips', '~> 2.0'
end
