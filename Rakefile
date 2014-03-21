# encoding: utf-8

require 'rubygems' unless defined?(Gem)
require 'rake' unless defined?(Rake)

require 'rake/extensiontask'
require 'rake/testtask'
require 'yard'

YARD::Rake::YardocTask.new do |t|
  t.files   = ['README.md', 'lib/**/*.rb', "ext/mosquitto/*.c"]
end

Rake::ExtensionTask.new('mosquitto') do |ext|
  ext.name = 'mosquitto_ext'
  ext.ext_dir = 'ext/mosquitto'
  CLEAN.include 'lib/**/mosquitto_ext.*'
end

desc 'Run mosquitto tests'
Rake::TestTask.new(:test) do |t|
  t.test_files = Dir.glob("test/**/test_*.rb")
  t.verbose = true
  t.warning = true
end

desc 'Run mosquitto integration tests'
Rake::TestTask.new(:integration) do |t|
  t.test_files = Dir.glob("test/test_integration.rb")
  t.verbose = true
  t.warning = true
end

desc 'Run mosquitto TLS tests'
Rake::TestTask.new(:tls) do |t|
  t.test_files = Dir.glob("test/test_tls.rb")
  t.verbose = true
  t.warning = true
end

namespace :debug do
  desc "Run the test suite under gdb"
  task :gdb do
    system "gdb --args ruby rake"
  end
end

task :test => :compile
task :default => :test