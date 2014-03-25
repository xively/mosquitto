# encoding: utf-8

require 'logger'
require 'mosquitto/logging'

class Mosquitto::Client
  include Mosquitto::Logging

  if RUBY_VERSION.split(".").first == '2'
    def wait_readable(timeout = 15)
      retries ||= 0
      IO.for_fd(socket).wait_readable(timeout)
    rescue Errno::EBADF
      retries += 1
      sleep 0.5
      raise if retries > 4
    end
  else
    def wait_readable(timeout = 15)
      retries ||= 0
      IO.for_fd(socket).wait(timeout)
    rescue Errno::EBADF
      retries += 1
      sleep 0.5
      raise if retries > 4
    end
  end
end