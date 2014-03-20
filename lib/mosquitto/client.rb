# encoding: utf-8

require 'logger'
require 'mosquitto/logging'

class Mosquitto::Client
  include Mosquitto::Logging

  if RUBY_VERSION.split(".").first == '2'
    def wait_readable(timeout = 15)
      IO.for_fd(socket).wait_readable(timeout)
    end
  else
    def wait_readable(timeout = 15)
      IO.for_fd(socket).wait(timeout)
    end
  end
end