# encoding: utf-8

$:.unshift('.')
$:.unshift(File.expand_path(File.dirname(__FILE__)) + '/../lib')

require 'mosquitto'

publisher = Mosquitto::Client.new
publisher.loop_start
publisher.on_log do |level,msg|
  p "PUB: #{msg}"
end
publisher.on_connect do |rc|
  p "Connect #{rc}"
  publisher.publish(nil, "topic", "test", Mosquitto::AT_MOST_ONCE, true)
end
publisher.connect("localhost", 1883, 10)
publisher.on_publish do |mid|
  p "Published #{mid}"
end

subscriber = Mosquitto::Client.new
subscriber.loop_start
subscriber.on_log do |level,msg|
  p "SUB: #{msg}"
end
subscriber.on_connect do |rc|
  sleep 0.5
  p "Connect #{rc}"
  subscriber.subscribe(nil, "topic", Mosquitto::AT_MOST_ONCE)
end
subscriber.on_subscribe do |mid,qos_count,granted_qos|
  p "Subscribed #{mid}"
end
subscriber.connect("localhost", 1883, 10)
subscriber.on_message do |msg|
  p "Message #{msg}"
end

sleep 2