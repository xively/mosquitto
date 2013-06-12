require 'mkmf'

dir_config('mosquitto')

have_func('rb_thread_blocking_region')
find_header("mosquitto.h")
have_library('mosquitto')

$defs << "-pedantic"
$CFLAGS << ' -Wall -funroll-loops'
$CFLAGS << ' -Wextra -O0 -ggdb3' if ENV['DEBUG']

create_makefile('mosquitto_ext')