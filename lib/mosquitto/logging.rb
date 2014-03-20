# encoding: utf-8

module Mosquitto::Logging
  LOG_LEVELS = {
    Mosquitto::LOG_ERR => Logger::FATAL,
    Mosquitto::LOG_ERR => Logger::ERROR,
    Mosquitto::LOG_WARNING => Logger::WARN,
    Mosquitto::LOG_INFO => Logger::INFO,
    Mosquitto::LOG_DEBUG => Logger::DEBUG
  }

  attr_reader :logger

  # Pipes libmosquitto log messages to a Ruby logger instance.
  #
  # @param logger [Logger] a Ruby logger instance. Compatible with SyslogLogger and other
  #                        implementations as well.
  # @raise [Argument] on invalid input params
  # @example
  #   client.logger = Logger.new(STDOUT)
  #
  def logger=(obj)
    unless obj.respond_to?(:add) and obj.method(:add).arity != 3
      raise ArgumentError, "invalid Logger instance #{obj.inspect}"
    end

    @logger = obj

    on_log do |level, message|
      severity = LOG_LEVELS[level] || Logger::UNKNOWN
      logger.add(severity, message.to_s, "MQTT")
    end
  end

  # Pipe debug messages through an already assigned logger instance.
  #
  # @param message [string] a message to log
  # @param severity [Mosquitto::LOG_ERR, Mosquitto::LOG_WARNING, Mosquitto::LOG_INFO, Mosquitto::LOG_DEBUG] log severity
  # @example
  #   client.log("message")
  #
  def log(message, severity = Logger::DEBUG)
    logger.add(severity, message.to_s, "MQTT") if logger
  end
end