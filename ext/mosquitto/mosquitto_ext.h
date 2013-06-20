#ifndef MOSQUITTO_EXT_H
#define MOSQUITTO_EXT_H

#include <mosquitto.h>
#include "ruby.h"

#if defined(__GNUC__) && (__GNUC__ >= 3)
#define MOSQ_UNUSED __attribute__ ((unused))
#define MOSQ_NOINLINE __attribute__ ((noinline))
#else
#define MOSQ_UNUSED
#define MOSQ_NOINLINE
#endif

#include "mosquitto_prelude.h"

#include <ruby/encoding.h>
#include <ruby/io.h>
extern rb_encoding *binary_encoding;
#define MosquittoEncode(str) rb_enc_associate(str, binary_encoding)

#define MosquittoError(desc) rb_raise(rb_eMosquittoError, desc);

extern VALUE rb_mMosquitto;
extern VALUE rb_eMosquittoError;
extern VALUE rb_cMosquittoClient;
extern VALUE rb_cMosquittoMessage;

extern VALUE intern_call;

#include "client.h"
#include "message.h"

#endif
