# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestIntegration < MosquittoTestCase
  TOPICS = ["1/2/3", "a/b/c", "1/2", "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8",
           "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/#", "#", "1/2/./3", "*/>/#",
           "a/+/#", "a/#", "+/+/+", "will/topic"]

  CLIENT_IDS = %w(test_integration test_lwt test_clean_session test_duplicate)

  def setup
    @result = nil
    @client = nil
    connected = false
    @client = Mosquitto::Client.new(nil, true)
    @client.loop_start
    @client.logger = Logger.new(STDOUT)
    @client.on_connect do |rc|
      connected = true
    end
    @client.on_message do |msg|
      @result = msg.to_s
    end
    assert @client.connect(TEST_HOST, TEST_PORT, 60)
    wait{ connected }
  end

  def teardown
    disconnected, connected = false, false
    @client.on_disconnect do |rc|
      disconnected = true
    end
    @client.disconnect
    wait{ disconnected }
    @client.loop_stop(true)

    CLIENT_IDS.each do |client_id|
      disconnected = false
      client = Mosquitto::Client.new(client_id)
      client.loop_start
      client.on_disconnect do |rc|
        disconnected = true
      end
      client.on_connect do |rc|
        assert client.disconnect
      end
      assert client.connect(TEST_HOST, TEST_PORT, 60)
      wait{ disconnected }
      client.loop_stop(true)
    end

    client = Mosquitto::Client.new("purge")
    client.loop_start
    client.on_connect do |rc|
      connected = true
    end
    client.on_message do |msg|
      if msg.retain?
        assert client.publish(nil, msg.topic, "", Mosquitto::AT_LEAST_ONCE, true) 
      end
    end
    assert client.connect(TEST_HOST, TEST_PORT, 60)
    wait{ connected }

    TOPICS.each do |topic|
      assert client.subscribe(nil, topic, Mosquitto::AT_MOST_ONCE)
    end

    sleep 5
    begin
      client.disconnect
      client.loop_stop(true)
    rescue Mosquitto::Error
    end
  end

  def test_basic
    # check basic pub/sub on QOS 0
    expected = "hello mqtt broker on QOS 0"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    assert @client.subscribe(nil, "1/2/3", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    assert @client.unsubscribe(nil, "1/2/3")

    # check basic pub/sub on QOS 1
    @result = nil
    expected = "hello mqtt broker on QOS 1"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_LEAST_ONCE, false)    
    end
    assert @client.subscribe(nil, "a/b/c", Mosquitto::AT_LEAST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "a/b/c")

    # check basic pub/sub on QOS 2
    @result = nil
    expected = "hello mqtt broker on QOS 2"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2", expected, Mosquitto::EXACTLY_ONCE, false)    
    end
    assert @client.subscribe(nil, "1/2", Mosquitto::EXACTLY_ONCE)
    wait{ @result }
    assert_equal expected, @result
    assert @client.unsubscribe(nil, "1/2")
  end

  def test_long_topic
    # check a simple # subscribe works
    @result = nil
    expected = "hello mqtt broker on long topic"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    assert @client.subscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    assert @client.unsubscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8")

    @result = nil
    expected = "hello mqtt broker on long topic with hash"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    assert @client.subscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on long topic with hash again"
    assert @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/9/10/0", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result
    assert @client.unsubscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/#")
  end

  def test_overlapping_topics
    # check a simple # subscribe works
    @result = nil
    expected = "hello mqtt broker on hash"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    assert @client.subscribe(nil, "#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on some other topic"
    assert @client.publish(nil, "1/2/3/4/5/6", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    # now subscribe on a topic that overlaps the root # wildcard - we should still get everything
    @result = nil
    assert @client.subscribe(nil, "1/2/3", Mosquitto::AT_MOST_ONCE)
    expected = "hello mqtt broker on explicit topic"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on some other topic"
    assert @client.publish(nil, "a/b/c/d/e", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

   # now unsub hash - we should only get called back on 1/2/3
    @client.unsubscribe(nil, "#");
    @result = nil
    expected = "this should not come back..."
    assert @client.publish(nil, "1/2/3/4", expected, Mosquitto::AT_MOST_ONCE, false)    
    sleep 1
    assert_nil @result

    @result = nil
    expected = "this should not come back either..."
    assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_MOST_ONCE, false)    
    sleep 1
    assert_nil @result

    # this should still come back since we are still subscribed on 1/2/3
    @result = nil
    expected = "we should still get this"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result
    assert @client.unsubscribe(nil, "1/2/3")

    # repeat the above full test but reverse the order of the subs
    @result = nil
    expected = "hello mqtt broker on hash"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    assert @client.subscribe(nil, "1/2/3", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on a different topic - we shouldn't get this"
    assert @client.publish(nil, "1/2/3/4/5/6", expected, Mosquitto::AT_MOST_ONCE, false)    
    sleep 1
    assert_nil @result

    @result = nil
    expected = "hello mqtt broker on some other topic topic"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "a/b/c/d", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    assert @client.subscribe(nil, "#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on some other topic"
    assert @client.publish(nil, "1/2/3/4/5/6", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @client.unsubscribe(nil, "1/2/3")

    @result = nil
    expected = "this should come back..."
    assert @client.publish(nil, "1/2/3/4/5/6", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "this should come back too..."
    assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "we should still get this as well."
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    assert @client.unsubscribe(nil, "#")
  end

  def test_dots
    # check that dots are not treated differently
    @result = nil
    expected = "hello mqtt broker with a dot"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/./3", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    assert @client.subscribe(nil, "1/2/./3", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/./3/4", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    assert @client.unsubscribe(nil, "1/2/./3")

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/./3", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/./3/4", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result
  end

  def test_active_mq_wildcards
    # check that ActiveMQ native wildcards are not treated differently
    @result = nil
    expected = "hello mqtt broker with fake wildcards"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "*/>/1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    assert @client.subscribe(nil, "*/>/#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "*/2", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should get this"
    assert @client.publish(nil, "*/>/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should get this"
    assert @client.publish(nil, "*/>", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result
  end

  def test_native_mqtt_wildcards
    # check that hash works right with plus
    @result = nil
    expected = "sub on everything below a but not a"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "a/b", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    assert @client.subscribe(nil, "a/+/#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "a", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    assert @client.unsubscribe(nil, "a/+/#")
    assert @client.subscribe(nil, "a/#", Mosquitto::AT_MOST_ONCE)

    sleep 1

    @result = nil
    expected = "sub on everything below a including a"
    assert @client.publish(nil, "a/b", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "sub on everything below a still including a"
    assert @client.publish(nil, "a", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "sub on everything below a still including a - should not get b"
    assert @client.publish(nil, "b", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    assert @client.unsubscribe(nil, "a/#")
  end

  def test_wildcard_plus
    # check that unsub of hash doesn't affect other subscriptions
    @result = nil
    expected = "should get this 1"
    @client.on_subscribe do |mid, granted_qos|
      assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    assert @client.subscribe(nil, "+/+/+", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should get this 2"
    assert @client.publish(nil, "a/2/3", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should get this 3"
    assert @client.publish(nil, "1/b/c", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should not get this"
    assert @client.publish(nil, "1/2", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get this either"
    assert @client.publish(nil, "1/2/3/4", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result
  end

  def test_subs
    assert @client.subscribe(nil, "1/2/3", Mosquitto::AT_MOST_ONCE)
    assert @client.subscribe(nil, "a/+/#", Mosquitto::AT_MOST_ONCE)
    assert @client.subscribe(nil, "#", Mosquitto::AT_MOST_ONCE)

    sleep 1

    @result = nil
    expected = "should get everything"
    assert @client.publish(nil, "1/2/3/4", expected, Mosquitto::AT_MOST_ONCE, false)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should get everything"
    assert @client.publish(nil, "a/1/2", expected, Mosquitto::AT_MOST_ONCE, false)
    wait{ @result }
    assert_equal expected, @result

    assert @client.unsubscribe(nil, "a/+/#")
    assert @client.unsubscribe(nil, "#")

    sleep 1

    @result = nil
    expected = "should still get 1/2/3"
    assert @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "should not get anything else"
    assert @client.publish(nil, "a/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result

    @result = nil
    expected = "should not get anything else"
    assert @client.publish(nil, "a", expected, Mosquitto::AT_MOST_ONCE, false)
    sleep 1
    assert_nil @result
  end

  def test_duplicate_client_id
    client1_connected = false, client1_disconnected = false
    client2 = nil
    client1 = Mosquitto::Client.new("test_duplicate")
    client1.loop_start
    client1.logger = Logger.new(STDOUT)
    client1.on_connect do |rc|
      client1_connected = true
    end
    client1.on_disconnect do |rc|
      client1_disconnected = true
      client1.loop_stop(true)
      client2.loop_stop(true)
    end
    client1.connect(TEST_HOST, TEST_PORT, 60)

    client1.wait_readable

    client2 = Mosquitto::Client.new("test_duplicate")
    client2.loop_start
    client2.logger = Logger.new(STDOUT)
    client2.connect(TEST_HOST, TEST_PORT, 60)

    client2.wait_readable

    assert client1_connected
    assert client1_disconnected
  end

  def test_clean_session
    client1 = Mosquitto::Client.new("test_clean_session")
    client1.logger = Logger.new(STDOUT)
    client1.loop_start
    client1.will_set("l/w/t", "This is an LWT", Mosquitto::AT_LEAST_ONCE, false)
    client1.connect(TEST_HOST, TEST_PORT, 60)

    assert client1.subscribe(nil, "a/b/c", Mosquitto::AT_LEAST_ONCE)

    sleep 1

    @result = nil
    expected = "should not get anything on publish only after the subscribe"

    client1.on_disconnect do |rc|
      assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_LEAST_ONCE, false)
      client1.connect(TEST_HOST, TEST_PORT, 60)
    end

    client1.disconnect

    sleep 1

    assert_nil @result
  end

  def test_retain
    # publish message with retain
    @result = nil
    expected = "should not get anything on publish only after the subscribe"
    assert @client.publish(nil, "a/b/c", expected, Mosquitto::AT_LEAST_ONCE, true)
    sleep 1
    assert_nil @result

    result = nil
    client1 = Mosquitto::Client.new(nil, true)
    client1.logger = Logger.new(STDOUT)
    client1.loop_start
    client1.on_message do |msg|
      result = msg.to_s
    end
    client1.connect(TEST_HOST, TEST_PORT, 60)
    client1.wait_readable

    assert client1.subscribe(nil, "a/b/c", Mosquitto::AT_LEAST_ONCE)

    wait{ result }
    assert_equal expected, result

    client1.disconnect

    sleep 1

    result = nil
    # clear retained message
    assert @client.publish(nil, "a/b/c", "", Mosquitto::AT_LEAST_ONCE, true)
  end

  def test_lwt
    assert @client.subscribe(nil, "will/topic", Mosquitto::AT_MOST_ONCE)

    will = "This is an LWT"
    client1 = Mosquitto::Client.new("test_lwt")
    client1.logger = Logger.new(STDOUT)
    client1.loop_start
    client1.will_set("will/topic", will, Mosquitto::AT_LEAST_ONCE, false)
    client1.on_connect do |rc|
      sleep 2
      client1.disconnect
    end
    client1.connect(TEST_HOST, TEST_PORT, 60)

    client1.wait_readable

    @result = nil
    wait{ @result }
    assert_equal will, @result
  end
end