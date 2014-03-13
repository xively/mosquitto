# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestClient < MosquittoTestCase
  def test_init
    client = Mosquitto::Client.new
    assert_instance_of Mosquitto::Client, client
    client = Mosquitto::Client.new("test")
    assert_raises TypeError do
      Mosquitto::Client.new(:invalid)
    end
  end

  def test_reinitialize
    client = Mosquitto::Client.new
    assert client.reinitialise("test")
    assert_raises TypeError do
      client.reinitialise(:invalid)
    end
  end

  def test_will_set
    client = Mosquitto::Client.new
    assert client.will_set("will_set", "test", Mosquitto::AT_MOST_ONCE, true)
    assert_raises TypeError do
      client.will_set("will_set", :invalid, Mosquitto::AT_MOST_ONCE, true)
    end
    assert_raises Mosquitto::Error do
      client.will_set("will_set", ('a' * 268435456), Mosquitto::AT_MOST_ONCE, true)
    end
  end

  def test_will_clear
    client = Mosquitto::Client.new
    assert client.will_set("will_clear", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.will_clear
  end

  def test_auth
    client = Mosquitto::Client.new
    assert client.auth("username", "password")
    assert client.auth("username", nil)
    assert_raises TypeError do
      client.auth(:invalid, "password")
    end
  end

  def test_connect
    client = Mosquitto::Client.new
    assert client.loop_start
    assert_raises TypeError do
      client.connect(:invalid, TEST_PORT, 10)
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
  ensure
    client.loop_stop(true)
  end

  def test_connect_bind
    client = Mosquitto::Client.new
    assert client.loop_start
    assert_raises TypeError do
      client.connect_bind("localhost", TEST_PORT, 10, :invalid)
    end
    assert client.connect_bind(TEST_HOST, TEST_PORT, 10, "0.0.0.0")
  ensure
    client.loop_stop(true)
  end

  def test_connect_async
    client = Mosquitto::Client.new
    assert client.loop_start
    assert_raises TypeError do
      client.connect_async(:invalid, TEST_PORT, 10)
    end
    assert client.connect_async(TEST_HOST, TEST_PORT, 10)
    sleep 1
    assert client.socket != -1
  ensure
    client.loop_stop(true)
  end

  def test_connect_bind_async
    client = Mosquitto::Client.new
    assert client.loop_start
    assert_raises TypeError do
      client.connect_bind_async(TEST_HOST, TEST_PORT, 10, :invalid)
    end
    assert client.connect_bind_async(TEST_HOST, TEST_PORT, 10, '0.0.0.0')
    sleep 1
    assert client.socket != -1
  ensure
    client.loop_stop(true)
  end

  def test_disconnect
    client = Mosquitto::Client.new
    assert client.loop_start
    assert_raises Mosquitto::Error do
      client.disconnect
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.disconnect
  ensure
    client.loop_stop(true)
  end

  def test_reconnect
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.reconnect
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.reconnect
  end

  def test_reconnect_delay_set
    client = Mosquitto::Client.new
    assert_raises TypeError do
      client.reconnect_delay_set(:invalid, 10, true)
    end
    assert client.reconnect_delay_set(2, 10, true)
  end

  def test_max_inflight_messages
    client = Mosquitto::Client.new
    assert_raises TypeError do
      client.max_inflight_messages = :invalid
    end
    assert client.max_inflight_messages = 10
  end

  def test_message_retry
    client = Mosquitto::Client.new
    assert_raises TypeError do
      client.message_retry = :invalid
    end
    assert client.message_retry = 10
  end
end