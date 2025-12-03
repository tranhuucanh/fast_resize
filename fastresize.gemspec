lib = File.expand_path('../bindings/ruby/lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)

Gem::Specification.new do |spec|
  spec.name          = "fastresize"
  spec.version       = File.read(File.join(__dir__, 'VERSION')).strip
  spec.authors       = ["FastGems Team"]
  spec.email         = ["team@fastgems.org"]

  spec.summary       = "Fast, lightweight image resizing library"
  spec.description   = "High-performance image resizing library with C++ backend supporting PNG, JPG, JPEG, WEBP, and BMP formats. Features parallel batch processing and minimal memory footprint."
  spec.homepage      = "https://github.com/fastgems/fastresize"
  spec.license       = "MIT"

  spec.required_ruby_version = ">= 2.5.0"

  # Specify which files should be added to the gem
  spec.files         = Dir[
    'README.md',
    'LICENSE',
    'VERSION',
    'CHANGELOG.md',
    'bindings/ruby/lib/**/*',
    'bindings/ruby/ext/**/*.{rb,cpp,h}',
    'include/**/*.{h,hpp}',
    'src/**/*.{cpp,c}',
    'CMakeLists.txt'
  ]

  spec.require_paths = ['bindings/ruby/lib']
  spec.extensions    = ['bindings/ruby/ext/fastresize/extconf.rb']

  # Development dependencies
  spec.add_development_dependency 'rake', '~> 13.0'
  spec.add_development_dependency 'rake-compiler', '~> 1.2'
  spec.add_development_dependency 'rspec', '~> 3.0'
  spec.add_development_dependency 'benchmark-ips', '~> 2.0'

  # Metadata
  spec.metadata = {
    'bug_tracker_uri'   => 'https://github.com/fastgems/fastresize/issues',
    'changelog_uri'     => 'https://github.com/fastgems/fastresize/blob/main/CHANGELOG.md',
    'documentation_uri' => 'https://github.com/fastgems/fastresize',
    'source_code_uri'   => 'https://github.com/fastgems/fastresize'
  }
end
