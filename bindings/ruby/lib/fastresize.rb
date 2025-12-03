require 'fastresize/fastresize_ext'

module FastResize
  VERSION = File.read(File.expand_path('../../../VERSION', __dir__)).strip

  # Error class for FastResize errors
  class Error < StandardError; end

  # Convenience methods with better Ruby idioms
  class << self
    # Get the version of the library
    def version
      VERSION
    end
  end
end
