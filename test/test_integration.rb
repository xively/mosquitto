# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestIntegration < MosquittoTestCase
  def setup
    @result = nil
    return if @client
    @client = nil
    connected = false
    @client = Mosquitto::Client.new
    @client.loop_start
    @client.logger = Logger.new(STDOUT)
    @client.on_connect do |rc|
      connected = true
    end
    @client.on_message do |msg|
      @result = msg.to_s
    end
    @client.connect(TEST_HOST, TEST_PORT, 10)
    wait{ connected }
  end

  def test_basic
    # check basic pub/sub on QOS 0
    expected = "hello mqtt broker on QOS 0"
    @client.on_subscribe do |mid, granted_qos|
      @client.publish(nil, "1/2/3", expected, Mosquitto::AT_MOST_ONCE, false)
    end
    @client.subscribe(nil, "1/2/3", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "1/2/3")

    # check basic pub/sub on QOS 1
    @result = nil
    expected = "hello mqtt broker on QOS 1"
    @client.on_subscribe do |mid, granted_qos|
      @client.publish(nil, "a/b/c", expected, Mosquitto::AT_LEAST_ONCE, false)    
    end
    @client.subscribe(nil, "a/b/c", Mosquitto::AT_LEAST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "a/b/c")

    # check basic pub/sub on QOS 2
    @result = nil
    expected = "hello mqtt broker on QOS 2"
    @client.on_subscribe do |mid, granted_qos|
      @client.publish(nil, "1/2", expected, Mosquitto::EXACTLY_ONCE, false)    
    end
    @client.subscribe(nil, "1/2", Mosquitto::EXACTLY_ONCE)
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "1/2")
  end

  def test_long_topic
    # check a simple # subscribe works
    @result = nil
    expected = "hello mqtt broker on long topic"
    @client.on_subscribe do |mid, granted_qos|
      @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    @client.subscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8")

    @result = nil
    expected = "hello mqtt broker on long topic with hash"
    @client.on_subscribe do |mid, granted_qos|
      @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8", expected, Mosquitto::AT_MOST_ONCE, false)    
    end
    @client.subscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/#", Mosquitto::AT_MOST_ONCE)
    wait{ @result }
    assert_equal expected, @result

    @result = nil
    expected = "hello mqtt broker on long topic with hash again"
    @client.publish(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/9/10/0", expected, Mosquitto::AT_MOST_ONCE, false)    
    wait{ @result }
    assert_equal expected, @result
    @client.unsubscribe(nil, "1/2/3/this_is_a_long_topic_that_wasnt_working/before/4/5/6/7/8/#")
  end
end