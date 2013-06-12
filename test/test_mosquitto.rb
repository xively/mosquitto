# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestMosquitto < MosquittoTestCase
  def test_version
    assert_equal 1001003, Mosquitto.version
  end

  def test_constants
    assert_equal 0, Mosquitto::AT_MOST_ONCE
    assert_equal 1, Mosquitto::AT_LEAST_ONCE
    assert_equal 2, Mosquitto::EXACTLY_ONCE
  end
end