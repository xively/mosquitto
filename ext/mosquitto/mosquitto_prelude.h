#ifndef MOSQUITTO_PRELUDE_H
#define MOSQUITTO_PRELUDE_H

#ifndef RFLOAT_VALUE
#define RFLOAT_VALUE(v) (RFLOAT(v)->value)
#endif

#if LIBMOSQUITTO_VERSION_NUMBER != 1002003
#error libmosquitto version 1.2.3 required
#endif

#ifdef RUBINIUS
#include "rubinius.h"
#else
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
#include "ruby19.h"
#else
#include "ruby18.h"
#endif
#endif

#endif
