require 'mkmf'

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

dir_config('mosquitto')

# detect homebrew installs, via @brianmario
if !have_library 'mosquitto'
  base = if !`which brew`.empty?
    `brew --prefix`.strip
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