# mosquitto - a high perf MQTT 3.1 client

[![Build Status](https://travis-ci.org/bear-metal/mosquitto.png)](https://travis-ci.org/bear-metal/mosquitto)

## About MQTT and libmosquitto

[MQ Telemetry Transport](http://mqtt.org/) is :

```
MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging
protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks. The design
principles are to minimise network bandwidth and device resource requirements whilst also attempting to ensure
reliability and some degree of assurance of delivery. These principles also turn out to make the protocol ideal of
the emerging “machine-to-machine” (M2M) or “Internet of Things” world of connected devices, and for mobile
applications where bandwidth and battery power are at a premium.
```
Please see the [FAQ](http://mqtt.org/faq) and [list of supported software](http://mqtt.org/wiki/software).

### libmosquitto

```
Mosquitto is an open source (BSD licensed) message broker that implements the MQ Telemetry Transport protocol
version 3.1. MQTT provides a lightweight method of carrying out messaging using a publish/subscribe model. This
makes it suitable for "machine to machine" messaging such as with low power sensors or mobile devices such as
phones, embedded computers or microcontrollers like the Arduino. 
```

See the [project website](http://mosquitto.org/) for more information.

## Requirements

This gem links against version 1.2.3 of `libmosquitto` . You will need to install additional packages for your system.

### OS X

The preferred installation method for libmosquitto on OS X is with the [Homebrew](https://github.com/Homebrew/homebrew) package manager :

``` sh
brew install mosquitto
```

### Linux Ubuntu

``` sh
sudo apt-get update
sudo apt-get install pkg-config cmake openssl
```

The current Ubuntu packages aren't on 1.2.3 yet - it's recommended to build libmosquitto from source (see below) until further notice. OpenSSL is an optional dependency - libmosquitto builds without it, however TLS specific features would not be available.

### Building libmosquitto from source

``` sh
wget http://mosquitto.org/files/source/mosquitto-1.2.3.tar.gz
tar xzf mosquitto-1.2.3.tar.gz
cd mosquitto-1.2.3
cmake .
sudo make install
```

## Compatibility

This gem is regularly tested against the following Ruby versions on Linux and Mac OS X:

 * Ruby MRI 1.9.3
 * Ruby MRI 2.0.0 (ongoing patch releases).
 * Ruby MRI 2.1.0 (ongoing patch releases).
 * Ruby MRI 2.1.1 (ongoing patch releases).

Ruby 1.8, Rubinius and JRuby are not supported.

## Installing

### OSX / Linux

When are requirements or dependencies are met, the following should install mosquitto without any problems :

``` sh
gem install mosquitto
```

The extension checks for libmosquitto presence as well as a 1.2.3 version.

## Usage

### Threaded loop

The simplest mode of operation - starts a new thread to process network traffic.

``` ruby
require 'mosquitto'

publisher = Mosquitto::Client.new("blocking")

# Spawn a network thread with a main loop
publisher.loop_start

# On publish callback
publisher.on_publish do |mid|
  p "Published #{mid}"
end

# On connect callback
publisher.on_connect do |rc|
  p "Connected with return code #{rc}"
  # publish a test message once connected
  publisher.publish(nil, "topic", "test message", Mosquitto::AT_MOST_ONCE, true)
end

# Connect to MQTT broker
publisher.connect("test.mosquitto.org", 1883, 10)

# Allow some time for processing
sleep 2

publisher.disconnect

# Optional, stop the threaded loop - the network thread would be reaped on Ruby exit as well
publisher.loop_stop(true)
```

### Blocking loop (simple clients)

The client supports a blocking main loop as well which is useful for building simple MQTT clients. Reconnects
etc. and other misc operations are handled for you.

``` ruby
require 'mosquitto'

publisher = Mosquitto::Client.new("blocking")

# On publish callback
publisher.on_publish do |mid|
  p "Published #{mid}"
end

# On connect callback
publisher.on_connect do |rc|
  p "Connected with return code #{rc}"
  # publish a test message once connected
  publisher.publish(nil, "topic", "test message", Mosquitto::AT_MOST_ONCE, true)
end

# Connect to MQTT broker
publisher.connect("test.mosquitto.org", 1883, 10)

# Blocking main loop
publisher.loop_forever
```

### Custom main loop

EventMachine support is forthcoming.

### Logging

The client supports any of the Ruby Logger libraries with the standard #add method interface

``` ruby
require 'mosquitto'

publisher = Mosquitto::Client.new("blocking")

# Sets a custom log callback for us that pipes to the given logger instance
publisher.logger = Logger.new(STDOUT)

# Connect to MQTT broker
publisher.connect("test.mosquitto.org", 1883, 10)
```

### Callbacks

The following callbacks are supported (please follow links for further documentation) :

* [connect](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_connect) - called when the broker sends a CONNACK message in response to a connection.
* [disconnect](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_disconnect) - called when the broker has received the DISCONNECT command and has disconnected the client.
* [log](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_log) - should be used if you want event logging information from the client library.
* [subscribe](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_subscribe) - called when the broker responds to a subscription request.
* [unsubscribe](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_unsubscribe) - called when the broker responds to a unsubscription request.
* [publish](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_publish) - called when a message initiated with Mosquitto::Client#publish has been sent to the broker successfully.
* [message](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:on_message) - called when a message is received from the broker.

### TLS / SSL

libmosquitto builds with TLS support by default, however [pre-shared key (PSK)](http://rubydoc.info/github/bear-metal/mosquitto/master/Mosquitto/Client:tls_psk_set) support is not available when linked against older OpenSSL versions.

``` ruby
tls_client = Mosquitto::Client.new

tls_client.logger = Logger.new(STDOUT)

tls_client.loop_start

tls_client.on_connect do |rc|
  p :tls_connection
end
tls_client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil)
tls_client.tls_set('/path/to/mosquitto.org.crt'), nil, nil, nil)
tls_client.connect('test.mosquitto.org', 8883, 10)
```

See [documentation](http://rubydoc.info/github/bear-metal/mosquitto) for the full API specification.

## Contact, feedback and bugs

This extension is currently maintained by Lourens Naudé (http://github.com/methodmissing) and contributors.

Please file bugs / issues and feature requests on the [issue tracker](https://github.com/bear-metal/mosquitto/issues)

## Development

To run the tests, you can use RVM and Bundler to create a pristine environment for mosquitto development/hacking.
Use 'bundle install' to install the necessary development and testing gems:

``` sh
bundle install
rake
```
Tests by default run against the public `test.mosquitto.org` MQTT server, which supports TLS as well. More information is available at http://test.mosquitto.org/. Alternatively, should you wish you run tests against a local MQTT broker, change the following constants in the test helper to `localhost`:

``` ruby
class MosquittoTestCase < Test::Unit::TestCase
  TEST_HOST = "localhost"
  TEST_PORT = 1883

  TLS_TEST_HOST = "localhost"
  TLS_TEST_PORT = 8883
```

## Resources

Documentation available at http://rubydoc.info/github/bear-metal/mosquitto

## Special Thanks

* Roger Light - for libmosquitto