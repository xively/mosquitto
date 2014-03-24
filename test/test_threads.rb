# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestThreads < MosquittoTestCase
  def test_init
    threads = []
    threads << Thread.new do
      publisher = Mosquitto::Client.new
      publisher.loop_start
      assert publisher.connect(TEST_HOST, TEST_PORT, TIMEOUT)
      publisher.wait_readable
      publisher.loop_stop(true)
    end

    threads << Thread.new do
      subscriber = Mosquitto::Client.new
      subscriber.loop_start
      assert subscriber.connect(TEST_HOST, TEST_PORT, TIMEOUT)
      subscriber.wait_readable
      subscriber.loop_stop(true)
    end
    threads.each(&:join)
  end

  def test_pub_sub
    threads = []
    published = 0
    messages = []
    publisher, subscriber = nil
    threads << Thread.new do
      subscriber = Mosquitto::Client.new
      subscriber.loop_start
      subscriber.logger = Logger.new(STDOUT)
      subscriber.on_message do |msg|
        messages << msg.to_s
      end
      subscriber.on_connect do |rc|
        subscriber.subscribe(nil, "test_pub_sub", Mosquitto::AT_MOST_ONCE)
      end
      assert subscriber.connect(TEST_HOST, TEST_PORT, TIMEOUT)
      subscriber.wait_readable && sleep(2)
      subscriber.loop_stop(true)
    end

    threads << Thread.new do
      publisher = Mosquitto::Client.new
      publisher.loop_start
      publisher.logger = Logger.new(STDOUT)
      publisher.on_publish do |mid|
        published += 1
      end
      publisher.on_connect do |rc|
        ('a'..'z').to_a.each do |message|
          publisher.publish(nil, "test_pub_sub", message, Mosquitto::AT_MOST_ONCE, true)
        end
      end
      assert publisher.connect(TEST_HOST, TEST_PORT, TIMEOUT)
      publisher.wait_readable && sleep(2)
      assert_equal published, 26
      publisher.loop_stop(true)
    end

    threads.each{|t| t.join(5) }
    messages.uniq!
    messages.sort!
    assert_equal ('a'..'z').to_a, messages.sort
  end
end