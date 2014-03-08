# encoding: utf-8

# Prefer compiled Rubinius bytecode in .rbx/
ENV["RBXOPT"] = "-Xrbc.db"

begin
require "mosquitto/mosquitto_ext"
rescue LoadError
  require "mosquitto_ext"
end

require 'mosquitto/version' unless defined? Mosquitto::VERSION

require 'mosquitto/client'

at_exit { Mosquitto.cleanup }