# encoding: utf-8

require 'test/unit'
require 'mosquitto'
require 'stringio'
require 'thread'
require 'io/wait'

Thread.abort_on_exception = true

class Mosquitto::Client
if RUBY_VERSION.split(".").first == '2'
  def wait_readable(timeout = 10)
    IO.for_fd(socket).wait_readable(timeout)
  end
else
  def wait_readable(timeout = 10)
    IO.for_fd(socket).wait(timeout)
  end
end
end

class MosquittoTestCase < Test::Unit::TestCase
  TEST_HOST = "test.mosquitto.org"
  TEST_PORT = 1883

  TLS_TEST_HOST = "test.mosquitto.org"
  TLS_TEST_PORT = 8883

  undef_method :default_test if method_defined? :default_test

  def ssl_path
    File.expand_path("../ssl", __FILE__)
  end

  def ssl_object(file)
    File.expand_path("../ssl/#{file}", __FILE__)
  end

  if ENV['STRESS_GC'] == '1'
    def setup
      GC.stress = true
    end

    def teardown
      GC.stress = false
    end
  end
end