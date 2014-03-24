# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

require 'stringio'

class TestCustomLogger < MosquittoTestCase
  def test_logger
    log_dev = StringIO.new
    logger = Logger.new(log_dev)
    client = Mosquitto::Client.new

    assert_raises ArgumentError do
      client.logger = Object.new
    end

    client.logger = logger
    client.loop_start
    assert client.connect(TEST_HOST, TEST_PORT, 60)
    assert client.subscribe(nil, "custom_logger", Mosquitto::AT_MOST_ONCE)
    client.wait_readable

    logs = log_dev.string
    assert_match(/DEBUG/, logs)
    assert_match(/sending CONNECT/, logs)
    assert_match(/sending SUBSCRIBE/, logs)
    assert_match(/custom_logger/, logs)
    assert_match(/Client mosq/, logs)

  ensure
    client.loop_stop(true)
  end
end