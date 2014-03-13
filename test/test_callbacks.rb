# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestCallbacks < MosquittoTestCase
  def test_connect_disconnect_callback
    connected, disconnected = false
    client = Mosquitto::Client.new
    assert client.loop_start
    client.on_connect do |rc|
      connected = true
    end
    client.on_disconnect do |rc|
      disconnected = true
    end
    client.on_log do |level, msg|
      p "log [#{level}]: #{msg}"
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    sleep 1.5
    assert client.disconnect
    sleep 1.5
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
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.disconnect
    sleep 0.5
    assert_equal 2, logs.size
    assert_match(/CONNECT/, logs[0])
    assert_match(/DISCONNECT/, logs[1])
  end

  def test_subscribe_unsubscribe_callback
    msg_id = 0
    subscribed = false
    unsubscribed = false
    client = Mosquitto::Client.new
    assert client.loop_start
    client.on_subscribe do |mid,granted_qos|
      subscribed = true
      p granted_qos
      msg_id = mid
    end
    client.on_unsubscribe do |mid|
      unsubscribed = true
    end
    client.on_log do |level, msg|
      p "log [#{level}]: #{msg}"
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    sleep 1.5
    assert client.subscribe(nil, "test_sub_unsub", Mosquitto::AT_MOST_ONCE)
    assert client.unsubscribe(nil, "test_sub_unsub")
    sleep 1.5
    assert subscribed
    assert unsubscribed
    assert client.disconnect
    assert msg_id != 0
  ensure
    client.loop_stop(true)
  end

  def test_message_callback
    message = nil
    publisher = Mosquitto::Client.new
    publisher.loop_start
    publisher.on_connect do |rc|
      publisher.publish(nil, "message_callback", "test", Mosquitto::AT_MOST_ONCE, true)
    end
    publisher.connect(TEST_HOST, TEST_PORT, 10)
    sleep 3
    publisher.loop_stop(true)

    subscriber = Mosquitto::Client.new
    subscriber.loop_start
    subscriber.on_connect do |rc|
      subscriber.subscribe(nil, "message_callback", Mosquitto::AT_MOST_ONCE)
    end
    subscriber.connect(TEST_HOST, TEST_PORT, 10)
    subscriber.on_message do |msg|
      message = msg
    end
    sleep 3
    subscriber.loop_stop(true)
    assert_equal "test", message.to_s
  end
end