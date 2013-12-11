# encoding: utf-8

require File.expand_path('../lib/mosquitto/version', __FILE__)

Gem::Specification.new do |s|
  s.name = "mosquitto"
  s.version = Mosquitto::VERSION
  s.summary = "mosquito - Ruby binding against libmosquitto (http://mosquitto.org/) - a high performance MQTT protocol (http://mqtt.org) client"
  s.description = "mosquito - Ruby binding against libmosquitto (http://mosquitto.org/) - a high performance MQTT protocol (http://mqtt.org) client"
  s.authors = ["Lourens Naudé", "Bear Metal OÜ"]
  s.email = ["lourens@methodmissing.com", "info@bearmetal.eu"]
  s.homepage = "http://github.com/bear-metal/mosquitto"
  s.date = Time.now.utc.strftime('%Y-%m-%d')
  s.platform = Gem::Platform::RUBY
  s.extensions = "ext/mostquitto/extconf.rb"
  s.has_rdoc = true
  s.files = `git ls-files`.split("\n")
  s.test_files = `git ls-files test`.split("\n")
  s.rdoc_options = ["--charset=UTF-8"]
  s.require_paths = ["lib"]
  s.add_development_dependency('rake-compiler', '~> 0.9.2')
end