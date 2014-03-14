# encoding: utf-8

begin
require "mosquitto/mosquitto_ext"
rescue LoadError
  require "mosquitto_ext"
end

require 'mosquitto/version' unless defined? Mosquitto::VERSION

require 'mosquitto/client'