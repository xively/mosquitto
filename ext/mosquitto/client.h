#ifndef MOSQUITTO_CLIENT_H
#define MOSQUITTO_CLIENT_H

typedef struct {
    struct mosquitto *mosq;
    VALUE connect_cb;
    VALUE disconnect_cb;
    VALUE publish_cb;
    VALUE message_cb;
    VALUE subscribe_cb;
    VALUE unsubscribe_cb;
    VALUE log_cb;
} mosquitto_client_wrapper;

#define MosquittoGetClient(obj) \
    mosquitto_client_wrapper *client = NULL; \
    Data_Get_Struct(obj, mosquitto_client_wrapper, client); \
    if (!client) rb_raise(rb_eTypeError, "uninitialized Mosquitto client!");

#define MosquittoAssertCallback(cb, arity) \
    if (NIL_P(cb)){ \
        cb = proc; \
    } else { \
        if (rb_class_of(cb) != rb_cProc) \
            rb_raise(rb_eTypeError, "Expected a Proc callback"); \
        if (rb_proc_arity(cb) != arity) \
          rb_raise(rb_eArgError, "Callback expects %d argument(s), got %d", arity, NUM2INT(rb_proc_arity(cb))); \
    }

typedef struct mosquitto_callback_t mosquitto_callback_t;
struct mosquitto_callback_t {
    VALUE data[5];
    mosquitto_callback_t *next;
};

typedef struct mosquitto_callback_waiting_t mosquitto_callback_waiting_t;
struct mosquitto_callback_waiting_t {
    mosquitto_callback_t *callback;
    bool abort;
};

struct nogvl_connect_args {
    struct mosquitto *mosq;
    char *host;
    int port;
    int keepalive;
};

struct nogvl_loop_stop_args {
    struct mosquitto *mosq;
    bool force;
};

struct nogvl_loop_args {
    struct mosquitto *mosq;
    int timeout;
    int max_packets;
};

struct nogvl_reinitialise_args {
    struct mosquitto *mosq;
    char *client_id;
    bool clean_session;
    void *obj;
};

struct nogvl_publish_args {
    struct mosquitto *mosq;
    int *mid;
    char *topic;
    int payloadlen;
    const void *payload;
    int qos;
    bool retain;
};

struct nogvl_subscribe_args {
    struct mosquitto *mosq;
    int *mid;
    const char *subscription;
    int qos;
};

void _init_rb_mosquitto_client();

#endif
