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

  if base and mosquitto = Dir[File.join(base, 'Cellar/mosquitto/*')].sort.last
    $INCFLAGS << " -I#{mosquitto}/include "
    $LDFLAGS  << " -L#{mosquitto}/lib "
  end
end

have_func('rb_thread_call_without_gvl')
have_header("mosquitto.h")
have_header("pthread.h")
have_library('mosquitto')

$defs << "-pedantic"
$CFLAGS << ' -Wall -funroll-loops'
$CFLAGS << ' -Wextra -O0 -ggdb3' if ENV['DEBUG']

create_makefile('mosquitto_ext')