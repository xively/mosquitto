# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestLoops < MosquittoTestCase
  def test_socket
    client = Mosquitto::Client.new
    assert_equal(-1, client.socket)
    assert client.socket == -1
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert_instance_of Fixnum, client.socket
    sleep 1
    assert client.socket != -1
  end

  def test_loop
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.loop(10,10)
    end
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.publish(nil, "loop", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.loop(10,10)
  end

=begin
  def test_loop_forever
    connected = false
    Thread.new do
      client = Mosquitto::Client.new
      client.on_connect do |rc|
        connected = true
        Thread.current.kill
      end
      assert_raises TypeError do
        client.loop_forever(:invalid,1)
      end
      assert_raises Mosquitto::Error do
        client.loop_forever(10,10)
      end
      assert client.connect(TEST_HOST, 1883, 10)
      assert client.loop_forever(-1,1)
    end.join(1)
    sleep 1
    assert connected
  end
=end

  def test_loop_stop_start
    client = Mosquitto::Client.new
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert client.publish(nil, "loop_stop_start", "test", Mosquitto::AT_MOST_ONCE, true)
    assert client.loop_start
    assert client.loop_stop(true)
  end

  def test_want_write_p
    client = Mosquitto::Client.new
    assert client.connect(TEST_HOST, TEST_PORT, 10)
    assert !client.want_write?
  end
end