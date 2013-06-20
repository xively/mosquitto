# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestClient < MosquittoTestCase
  def test_init
    client = Mosquitto::Client.new
    assert_instance_of Mosquitto::Client, client
    client = Mosquitto::Client.new("test")
  end

  def test_reinitialize
    client = Mosquitto::Client.new
    assert client.reinitialise("test")
  end

  def test_will_set
    client = Mosquitto::Client.new
    assert client.will_set("topic", "test", Mosquitto::AT_MOST_ONCE, true)
  end

  def test_will_clear
    client = Mosquitto::Client.new
    assert client.will_set("topic", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.will_clear
  end

  def test_auth
    client = Mosquitto::Client.new
    assert client.auth("username", "password")
  end

  def test_connect
    client = Mosquitto::Client.new
    assert client.connect("localhost", 1883, 10)
  end

  def test_connect_async
    client = Mosquitto::Client.new
    assert client.connect_async("localhost", 1883, 10)
  end

  def test_reconnect
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      assert client.reconnect
    end
    assert client.connect("localhost", 1883, 10)
    assert client.reconnect
  end

  def test_publish
    client = Mosquitto::Client.new
    assert client.connect("localhost", 1883, 10)
    assert client.publish(nil, "topic", "test", Mosquitto::AT_MOST_ONCE, true)
  end

  def test_subscribe_unsubscribe
    client = Mosquitto::Client.new
    assert client.connect("localhost", 1883, 10)
    assert client.subscribe(nil, "topic", Mosquitto::AT_MOST_ONCE)
    assert client.unsubscribe(nil, "topic")
  end

  def test_socket
    client = Mosquitto::Client.new
    assert_equal(-1, client.socket)
    assert client.connect("localhost", 1883, 10)
    assert_instance_of Fixnum, client.socket
  end

  def test_loop
    client = Mosquitto::Client.new
    assert client.connect("localhost", 1883, 10)
    assert client.publish(nil, "topic", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.loop(10,10)
  end

  def test_loop_stop_start
    client = Mosquitto::Client.new
    assert client.connect("localhost", 1883, 10)
    assert client.publish(nil, "topic", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.loop_start
    assert client.loop_stop(true)
  end

  def test_connect_disconnect_callback
    connected, disconnected = false
    client = Mosquitto::Client.new
    assert client.loop_start
    client.on_connect do |rc|
      p :connected
      connected = true
    end
    client.on_disconnect do |rc|
      p :disconnected
      disconnected = true
    end
    client.on_log do |level, msg|
      p msg
    end
    assert client.connect("localhost", 1883, 10)
    assert client.disconnect
    assert connected
    assert disconnected
  ensure
    client.loop_stop(true)
  end

  def test_log_callback
    logs = []
    client = Mosquitto::Client.new
    client.on_log do |level, msg|
      logs << msg
    end
    assert client.connect("localhost", 1883, 10)
    assert client.disconnect
    assert_equal 2, logs.size
    assert_match(/CONNECT/, logs[0])
    assert_match(/DISCONNECT/, logs[1])
  end

  def test_subscribe_unsubscribe_callback
    msg_id = 0
    client = Mosquitto::Client.new
    assert client.loop_start
    client.on_subscribe do |mid,qos_count,granted_qos|
      p :subscribed
      msg_id = mid
      p msg_id
    end
    client.on_unsubscribe do |mid|
      p :unsubscribed
    end
    client.on_log do |level, msg|
      p msg
    end
    assert client.connect("localhost", 1883, 10)
    assert client.subscribe(nil, "topic", Mosquitto::AT_MOST_ONCE)
    assert client.unsubscribe(nil, "topic")
    assert client.disconnect
    assert msg_id != 0
  ensure
    client.loop_stop(true)
  end
end