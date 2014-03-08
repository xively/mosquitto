#ifndef MOSQUITTO_RUBY19_H
#define MOSQUITTO_RUBY19_H

#include "ruby/intern.h"

#define rb_thread_call_without_gvl(func, data1, ubf, data2) \
  rb_thread_blocking_region((rb_blocking_function_t *)func, data1, ubf, data2)

#endif
