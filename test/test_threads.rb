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
    published = 0
    messages = []
    threads << Thread.new do
      publisher = Mosquitto::Client.new
      publisher.loop_start
      publisher.on_connect do |rc|
        ('a'..'z').to_a.each do |message|
          publisher.publish(nil, "test_pub_sub", message, Mosquitto::AT_MOST_ONCE, true)
        end
      end
      publisher.on_publish do |mid|
        published += 1
      end
      assert publisher.connect(TEST_HOST, 1883, 10)
      sleep 2
      assert_equal published, 26
    end

    threads << Thread.new do
      subscriber = Mosquitto::Client.new
      subscriber.loop_start
      subscriber.on_connect do |rc|
        subscriber.subscribe(nil, "test_pub_sub", Mosquitto::AT_MOST_ONCE)
      end
      subscriber.on_message do |msg|
        messages << msg.to_s
      end
      assert subscriber.connect(TEST_HOST, 1883, 10)
      sleep 2
    end

    threads.each(&:join)
    assert_equal published, 26
    assert_equal messages.sort, ('a'..'z').to_a
  end
end