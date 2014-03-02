require 'mkmf'

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

dir_config('mosquitto')

# XXX temp, oust
$INCFLAGS << " -I/usr/local/Cellar/mosquitto/1.2.3/include"
$LDFLAGS << " -L/usr/local/Cellar/mosquitto/1.2.3/lib"

have_func('rb_thread_call_without_gvl')
have_header("mosquitto.h")
have_library('mosquitto')

$defs << "-pedantic"
$CFLAGS << ' -Wall -funroll-loops'
$CFLAGS << ' -Wextra -O0 -ggdb3' if ENV['DEBUG']

create_makefile('mosquitto_ext')