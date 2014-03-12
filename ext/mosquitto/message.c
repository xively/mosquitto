#include "mosquitto_ext.h"

/*
 * :nodoc:
 *  GC callback for releasing an out of scope Mosquitto::Message object
 *
 */
static void rb_mosquitto_free_message(void *ptr)
{
    mosquitto_message_wrapper *message = (mosquitto_message_wrapper *)ptr;
    if (message) {
        mosquitto_message_free(&message->msg);
        xfree(message);
    }
}

/*
 * :nodoc:
 *  Allocator function for Mosquitto::Message. This is only ever called from within an on_message callback
 *  within the binding scope, NEVER by the user.
 *
 */
VALUE rb_mosquitto_message_alloc(const struct mosquitto_message *msg)
{
    VALUE message;
    mosquitto_message_wrapper *wrapper = NULL;
    message = Data_Make_Struct(rb_cMosquittoMessage, mosquitto_message_wrapper, 0, rb_mosquitto_free_message, wrapper);
    wrapper->msg = msg;
    rb_obj_call_init(message, 0, NULL);
    return message;
}

/*
 *  call-seq:
 *    msg.mid -> Integer
 *
 *  Message identifier for this message. Note that although the MQTT protocol doesn't use message ids
 *  for messages with QoS=0, libmosquitto assigns them message ids so they can be tracked with this parameter.
 *
 * === Examples
 *
 *   msg.mid -> 2
 *
*/
static VALUE rb_mosquitto_message_mid(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->mid);
}

/*
 *  call-seq:
 *    msg.topic -> String
 *
 *  Topic this message was published on.
 *
 * === Examples
 *
 *   msg.topic -> "test"
 *
 */
static VALUE rb_mosquitto_message_topic(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return rb_str_new2(msg->topic);
}

/*
 *  call-seq:
 *    msg.to_s -> String
 *
 *  Coerces the Mosquitto::Message payload to a Ruby string.
 *
 * === Examples
 *
 *   msg.to_s -> "message"
 *
 */
static VALUE rb_mosquitto_message_to_s(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return rb_str_new(msg->payload, msg->payloadlen);
}

/*
 *  call-seq:
 *    msg.length -> Integer
 *
 *  The length of the message payload
 *
 * === Examples
 *
 *   msg.length -> 7
 *
 */
static VALUE rb_mosquitto_message_length(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->payloadlen);
}

/*
 *  call-seq:
 *    msg.qos -> Integer
 *
 *  Quality of Service used for the message
 *
 * === See
 *
 *  Mosquitto::AT_MOST_ONCE
 *  Mosquitto::AT_LEAST_ONCE
 *  Mosquitto::EXACTLY_ONCE
 *
 * === Examples
 *
 *   msg.qos -> Mosquitto::AT_MOST_ONCE
 *
 */
static VALUE rb_mosquitto_message_qos(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return INT2NUM(msg->qos);
}

/*
 *  call-seq:
 *    msg.retain? -> Boolean
 *
 *  Set to true if this message was flagged to retain.
 *
 * === Examples
 *
 *   msg.retain? -> true
 *
 */
static VALUE rb_mosquitto_message_retain_p(VALUE obj)
{
    struct mosquitto_message *msg;
    MosquittoGetMessage(obj);
    msg = message->msg;
    return (msg->retain == true) ? Qtrue : Qfalse;
}

/*
 *  Represents libmosquitto messages. They cannot be allocated or initialized from user code - they are
 *  spawned exclusively from within on_message callbacks and are thus read-only wrapper objects.
 *
 */

void _init_rb_mosquitto_message()
{
    rb_cMosquittoMessage = rb_define_class_under(rb_mMosquitto, "Message", rb_cObject);

    rb_define_method(rb_cMosquittoMessage, "mid", rb_mosquitto_message_mid, 0);
    rb_define_method(rb_cMosquittoMessage, "topic", rb_mosquitto_message_topic, 0);
    rb_define_method(rb_cMosquittoMessage, "to_s", rb_mosquitto_message_to_s, 0);
    rb_define_method(rb_cMosquittoMessage, "length", rb_mosquitto_message_length, 0);
    rb_define_method(rb_cMosquittoMessage, "qos", rb_mosquitto_message_qos, 0);
    rb_define_method(rb_cMosquittoMessage, "retain?", rb_mosquitto_message_retain_p, 0);
}
