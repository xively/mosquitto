# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestMosquitto < MosquittoTestCase
  def test_version
    assert_equal 1002003, Mosquitto.version
  end

  def test_constants
    assert_equal 0, Mosquitto::AT_MOST_ONCE
    assert_equal 1, Mosquitto::AT_LEAST_ONCE
    assert_equal 2, Mosquitto::EXACTLY_ONCE

    assert_equal 0, Mosquitto::SSL_VERIFY_NONE
    assert_equal 1, Mosquitto::SSL_VERIFY_PEER

    assert_equal 0, Mosquitto::LOG_NONE
    assert_equal 1, Mosquitto::LOG_INFO
    assert_equal 2, Mosquitto::LOG_NOTICE
    assert_equal 4, Mosquitto::LOG_WARNING
    assert_equal 8, Mosquitto::LOG_ERR
    assert_equal 16, Mosquitto::LOG_DEBUG
    assert_equal 32, Mosquitto::LOG_SUBSCRIBE
    assert_equal 64, Mosquitto::LOG_UNSUBSCRIBE
    assert_equal 65535, Mosquitto::LOG_ALL
  end
end