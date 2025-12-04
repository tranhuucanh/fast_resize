require 'fastresize/fastresize_ext'

module FastResize
  VERSION = File.read(File.expand_path('../../../VERSION', __dir__)).strip

  class Error < StandardError; end

  class << self
    def version
      VERSION
    end
  end
end
