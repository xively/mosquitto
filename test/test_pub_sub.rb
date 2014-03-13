# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestPubSub < MosquittoTestCase
  def test_publish
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.publish(nil, "publish", "test", Mosquitto::AT_MOST_ONCE, true)
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert_raises TypeError do
      client.publish(nil, :invalid, "test", Mosquitto::AT_MOST_ONCE, true)
    end
    assert client.publish(nil, "publish", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.publish(3, "publish", "test", Mosquitto::AT_MOST_ONCE, true)
  end

  def test_subscribe
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.subscribe(nil, "subscribe", Mosquitto::AT_MOST_ONCE)
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert_raises TypeError do
      client.subscribe(nil, :topic, Mosquitto::AT_MOST_ONCE)
    end
    assert client.subscribe(nil, "subscribe", Mosquitto::AT_MOST_ONCE)
    assert client.subscribe(3, "subscribe", Mosquitto::AT_MOST_ONCE)
  end

  def test_unsubscribe
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.unsubscribe(nil, "unsubscribe")
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert_raises TypeError do
      client.unsubscribe(nil, :topic)
    end
    assert client.unsubscribe(nil, "unsubscribe")
    assert client.unsubscribe(3, "unsubscribe")
  end

  def test_subscribe_unsubscribe
    client = Mosquitto::Client.new
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.subscribe(nil, "subscribe_unsubscribe", Mosquitto::AT_MOST_ONCE)
    assert client.unsubscribe(nil, "subscribe_unsubscribe")
  end
end