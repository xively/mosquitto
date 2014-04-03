require 'mkmf'

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

dir_config('mosquitto')

# detect homebrew installs, via @brianmario
if !have_library 'mosquitto'
  brew_exec_path = `which brew`
  base = if !brew_exec_path.empty?
    brew_exec_path.chomp!
    brew_exec_path = File.readlink(brew_exec_path) if File.symlink?(brew_exec_path)
    File.expand_path(File.join(brew_exec_path, "..", ".."))
  elsif File.exists?("/usr/local/Cellar/mosquitto")
    '/usr/local/Cellar'
  end
  p base
  if base and mosquitto = Dir[File.join(base, 'Cellar/mosquitto/*')].sort.last
    $INCFLAGS << " -I#{mosquitto}/include "
    $LDFLAGS  << " -L#{mosquitto}/lib "
  end
end

(have_header('ruby/thread.h') && have_func('rb_thread_call_without_gvl', 'ruby/thread.h')) || have_func('rb_thread_blocking_region')

(have_header("mosquitto.h") && have_library('mosquitto')) or abort("libmosquitto missing!")
have_header("pthread.h") or abort('pthread support required!')
have_macro("LIBMOSQUITTO_VERSION_NUMBER", "mosquitto.h")

$defs << "-pedantic"
$CFLAGS << ' -Wall -funroll-loops'
$CFLAGS << ' -Wextra -O0 -ggdb3' if ENV['DEBUG']

create_makefile('mosquitto_ext')
