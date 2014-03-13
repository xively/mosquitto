# encoding: utf-8

require 'logger'
require 'mosquitto/logging'

class Mosquitto::Client
  include Mosquitto::Logging
end