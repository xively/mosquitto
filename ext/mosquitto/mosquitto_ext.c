#include "mosquitto_ext.h"

VALUE rb_mMosquitto;
VALUE rb_eMosquittoError;
VALUE rb_cMosquittoClient;
VALUE rb_cMosquittoMessage;

VALUE intern_call;

rb_encoding *binary_encoding;

static VALUE rb_mosquitto_version(MOSQ_UNUSED VALUE obj)
{
    return INT2NUM(mosquitto_lib_version(NULL, NULL, NULL));
}

static VALUE rb_mosquitto_cleanup(MOSQ_UNUSED VALUE obj)
{
    mosquitto_lib_cleanup();
    return Qnil;
}

void Init_mosquitto_ext()
{
    mosquitto_lib_init();

    intern_call = rb_intern("call");

    binary_encoding = rb_enc_find("binary");

    rb_mMosquitto = rb_define_module("Mosquitto");

    rb_define_const(rb_mMosquitto, "AT_MOST_ONCE", INT2NUM(0));
    rb_define_const(rb_mMosquitto, "AT_LEAST_ONCE", INT2NUM(1));
    rb_define_const(rb_mMosquitto, "EXACTLY_ONCE", INT2NUM(2));

    rb_eMosquittoError = rb_define_class_under(rb_mMosquitto, "Error", rb_eStandardError);

    rb_define_module_function(rb_mMosquitto, "version", rb_mosquitto_version, 0);
    rb_define_module_function(rb_mMosquitto, "cleanup", rb_mosquitto_cleanup, 0);

    _init_rb_mosquitto_client();
    _init_rb_mosquitto_message();
}
