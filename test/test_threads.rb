# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestThreads < MosquittoTestCase
  def test_init
    threads = []
    threads << Thread.new do
      publisher = Mosquitto::Client.new
      publisher.loop_start
      assert publisher.connect(TEST_HOST, 1883, 10)
      sleep 1
    end

    threads << Thread.new do
      subscriber = Mosquitto::Client.new
      subscriber.loop_start
      assert subscriber.connect(TEST_HOST, 1883, 10)
      sleep 1
    end
    threads.each(&:join)
  end

  def test_pub_sub
    threads = []
    pub_queue = ('a'..'z').to_a
    published = 0
    messages = []
    threads << Thread.new do
      publisher = Mosquitto::Client.new
      publisher.loop_start
      assert publisher.connect(TEST_HOST, 1883, 10)
      publisher.on_connect do |rc|
        pub_queue.each do |message|
          publisher.publish(nil, "topic", message, Mosquitto::AT_MOST_ONCE, true)
        end
      end
      publisher.on_publish do |mid|
        published += 1
      end
      sleep 2
      assert_equal published, 26
    end

    threads << Thread.new do
      subscriber = Mosquitto::Client.new
      subscriber.loop_start
      assert subscriber.connect(TEST_HOST, 1883, 10)
      subscriber.on_connect do |rc|
        subscriber.subscribe(nil, "topic", Mosquitto::AT_MOST_ONCE)
      end
      subscriber.on_message do |msg|
        messages << msg.to_s
      end
      sleep 2
    end

    threads.each(&:join)
    assert_equal messages, ('a'..'z').to_a
  end
end