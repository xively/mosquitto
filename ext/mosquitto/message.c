#include "mosquitto_ext.h"

static void rb_mosquitto_free_message(void *ptr)
{
    mosquitto_message_wrapper *message = (mosquitto_message_wrapper *)ptr;
    if (message) {
        mosquitto_message_free(&message->msg);
        xfree(message);
    }
}

VALUE rb_mosquitto_message_alloc(const struct mosquitto_message *msg)
{
    VALUE message;
    mosquitto_message_wrapper *wrapper = NULL;
    message = Data_Make_Struct(rb_cMosquittoMessage, mosquitto_message_wrapper, 0, rb_mosquitto_free_message, wrapper);
    wrapper->msg = (struct mosquitto_message *)msg;
    rb_obj_call_init(message, 0, NULL);
    return message;
}

VALUE rb_mosquitto_message_mid(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->mid);
}

VALUE rb_mosquitto_message_topic(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return rb_str_new2(msg->topic);
}

VALUE rb_mosquitto_message_to_s(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return rb_str_new(msg->payload, msg->payloadlen);
}

VALUE rb_mosquitto_message_length(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->payloadlen);
}

VALUE rb_mosquitto_message_copy(VALUE obj)
{
    struct mosquitto_message *msg = NULL;
    int ret;
    MosquittoGetMessage(obj);
    ret = mosquitto_message_copy(msg, (const struct mosquitto_message *)message->msg);
    switch (ret) {
        case MOSQ_ERR_INVAL:
            MosquittoError("invalid input params");
            break;
        case MOSQ_ERR_NOMEM:
            rb_memerror();
            break;
        default:
            return rb_mosquitto_message_alloc((const struct mosquitto_message *)msg);
    }
}

VALUE rb_mosquitto_message_qos(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->qos);
}

VALUE rb_mosquitto_message_retain_p(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return (msg->retain == true) ? Qtrue : Qfalse;
}

void _init_rb_mosquitto_message()
{
    rb_cMosquittoMessage = rb_define_class_under(rb_mMosquitto, "Message", rb_cObject);

    rb_define_method(rb_cMosquittoMessage, "mid", rb_mosquitto_message_mid, 0);
    rb_define_method(rb_cMosquittoMessage, "topic", rb_mosquitto_message_topic, 0);
    rb_define_method(rb_cMosquittoMessage, "to_s", rb_mosquitto_message_to_s, 0);
    rb_define_method(rb_cMosquittoMessage, "length", rb_mosquitto_message_length, 0);
    rb_define_method(rb_cMosquittoMessage, "copy", rb_mosquitto_message_copy, 0);
    rb_define_method(rb_cMosquittoMessage, "qos", rb_mosquitto_message_qos, 0);
    rb_define_method(rb_cMosquittoMessage, "retain?", rb_mosquitto_message_retain_p, 0);
}
