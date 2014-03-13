# mosquitto - a high performance MQTT 3.1 client

[![Build Status](https://travis-ci.org/bear-metal/mosquitto.png)](https://travis-ci.org/bear-metal/mosquitto)

## About MQTT

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

The current Ubuntu packages aren't on 1.2.3 yet - it's recommended to build libmosquitto from source (see below) until further notice.

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

## Installing

### OSX / Linux

``` sh
gem install mosquitto
```
## Usage


## Contact, feedback and bugs


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