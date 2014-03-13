# mosquitto - a high perf MQTT 3.1 client

[![Build Status](https://travis-ci.org/bear-metal/mosquitto.png)](https://travis-ci.org/bear-metal/mosquitto)

## About MQTT and libmosquitto

[MQ Telemetry Transport](http://mqtt.org/) is :

```
MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks. The design principles are to minimise network bandwidth and device resource requirements whilst also attempting to ensure reliability and some degree of assurance of delivery. These principles also turn out to make the protocol ideal of the emerging “machine-to-machine” (M2M) or “Internet of Things” world of connected devices, and for mobile applications where bandwidth and battery power are at a premium.
```
Please see the [FAQ](http://mqtt.org/faq) and [list of supported software](http://mqtt.org/wiki/software).

### libmosquitto

```
Mosquitto is an open source (BSD licensed) message broker that implements the MQ Telemetry Transport protocol version 3.1. MQTT provides a lightweight method of carrying out messaging using a publish/subscribe model. This makes it suitable for "machine to machine" messaging such as with low power sensors or mobile devices such as phones, embedded computers or microcontrollers like the Arduino. 
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

## Contact, feedback and bugs

This extension is currently maintained by Lourens Naud (http://github.com/methodmissing) and contributors.

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