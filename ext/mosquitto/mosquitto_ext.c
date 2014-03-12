#include "mosquitto_ext.h"

VALUE rb_mMosquitto;
VALUE rb_eMosquittoError;
VALUE rb_cMosquittoClient;
VALUE rb_cMosquittoMessage;

VALUE intern_call;

rb_encoding *binary_encoding;

/*
 *  call-seq:
 *    Mosquitto.version -> Integer
 *
 *  The libmosquitto version linked against. It returns for libmosquitto 1.2.3 an integer as :
 *
 *  1002003
 */

static VALUE rb_mosquitto_version(MOSQ_UNUSED VALUE obj)
{
    return INT2NUM(mosquitto_lib_version(NULL, NULL, NULL));
}

/*
 *  call-seq:
 *    Mosquitto.cleanup -> nil
 *
 *  Release resources associated with the libmosquitto library.
 */

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

    /*
     * Message specific constants
     */

    rb_define_const(rb_mMosquitto, "AT_MOST_ONCE", INT2NUM(0));
    rb_define_const(rb_mMosquitto, "AT_LEAST_ONCE", INT2NUM(1));
    rb_define_const(rb_mMosquitto, "EXACTLY_ONCE", INT2NUM(2));

   /*
    * Log specific constants *
    */

    rb_define_const(rb_mMosquitto, "LOG_NONE", INT2NUM(0x00));
    rb_define_const(rb_mMosquitto, "LOG_INFO", INT2NUM(0x01));
    rb_define_const(rb_mMosquitto, "LOG_NOTICE", INT2NUM(0x02));
    rb_define_const(rb_mMosquitto, "LOG_WARNING", INT2NUM(0x04));
    rb_define_const(rb_mMosquitto, "LOG_ERR", INT2NUM(0x08));
    rb_define_const(rb_mMosquitto, "LOG_DEBUG", INT2NUM(0x10));
    rb_define_const(rb_mMosquitto, "LOG_SUBSCRIBE", INT2NUM(0x20));
    rb_define_const(rb_mMosquitto, "LOG_UNSUBSCRIBE", INT2NUM(0x40));
    rb_define_const(rb_mMosquitto, "LOG_ALL", INT2NUM(0xFFFF));

    /*
     * TLS specific constants
     */

    rb_define_const(rb_mMosquitto, "SSL_VERIFY_NONE", INT2NUM(0));
    rb_define_const(rb_mMosquitto, "SSL_VERIFY_PEER", INT2NUM(1));

    rb_eMosquittoError = rb_define_class_under(rb_mMosquitto, "Error", rb_eStandardError);

    rb_define_module_function(rb_mMosquitto, "version", rb_mosquitto_version, 0);
    rb_define_module_function(rb_mMosquitto, "cleanup", rb_mosquitto_cleanup, 0);

    _init_rb_mosquitto_client();
    _init_rb_mosquitto_message();
}
