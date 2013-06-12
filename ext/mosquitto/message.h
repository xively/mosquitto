#ifndef MOSQUITTO_MESSAGE_H
#define MOSQUITTO_MESSAGE_H

typedef struct {
    struct mosquitto_message *msg;
} mosquitto_message_wrapper;

#define MosquittoGetMessage(obj) \
    mosquitto_message_wrapper *message = NULL; \
    Data_Get_Struct(obj, mosquitto_message_wrapper, message); \
    if (!message) rb_raise(rb_eTypeError, "uninitialized Mosquitto message!");

VALUE rb_mosquitto_message_alloc(const struct mosquitto_message *msg);
void _init_rb_mosquitto_message();

#endif