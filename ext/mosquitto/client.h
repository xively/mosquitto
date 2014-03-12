#ifndef MOSQUITTO_CLIENT_H
#define MOSQUITTO_CLIENT_H

typedef struct mosquitto_callback_t mosquitto_callback_t;

typedef struct {
    struct mosquitto *mosq;
    VALUE connect_cb;
    VALUE disconnect_cb;
    VALUE publish_cb;
    VALUE message_cb;
    VALUE subscribe_cb;
    VALUE unsubscribe_cb;
    VALUE log_cb;
    VALUE callback_thread;
    pthread_mutex_t callback_mutex;
    pthread_cond_t callback_cond;
    mosquitto_callback_t *callback_queue;
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

// TODO: xmalloc, the Ruby VM's preferred allocation method for managing memory pressure fails under GC stress when callbacks
// fire on a non-Ruby thread ( mosquitto_loop_start )
#define MOSQ_ALLOC(type) ((type*)malloc(sizeof(type)))

#define ON_CONNECT_CALLBACK 0x00
#define ON_DISCONNECT_CALLBACK 0x01
#define ON_PUBLISH_CALLBACK 0x02
#define ON_MESSAGE_CALLBACK 0x04
#define ON_SUBSCRIBE_CALLBACK 0x08
#define ON_UNSUBSCRIBE_CALLBACK 0x10
#define ON_LOG_CALLBACK 0x20

typedef struct on_connect_callback_args_t on_connect_callback_args_t;
struct on_connect_callback_args_t {
    int rc;
};

typedef struct on_disconnect_callback_args_t on_disconnect_callback_args_t;
struct on_disconnect_callback_args_t {
    int rc;
};

typedef struct on_publish_callback_args_t on_publish_callback_args_t;
struct on_publish_callback_args_t {
    int mid;
};

typedef struct on_message_callback_args_t on_message_callback_args_t;
struct on_message_callback_args_t {
    struct mosquitto_message *msg;
};

typedef struct on_subscribe_callback_args_t on_subscribe_callback_args_t;
struct on_subscribe_callback_args_t {
    int mid;
    int qos_count;
    const int *granted_qos;
};

typedef struct on_unsubscribe_callback_args_t on_unsubscribe_callback_args_t;
struct on_unsubscribe_callback_args_t {
    int mid;
};

typedef struct on_log_callback_args_t on_log_callback_args_t;
struct on_log_callback_args_t {
    int level;
    char *str;
};

struct mosquitto_callback_t {
    int type;
    mosquitto_client_wrapper *client;
    void *data;
    mosquitto_callback_t *next;
};

typedef struct mosquitto_callback_waiting_t mosquitto_callback_waiting_t;
struct mosquitto_callback_waiting_t {
    mosquitto_callback_t *callback;
    mosquitto_client_wrapper *client;
    bool abort;
};

struct nogvl_connect_args {
    struct mosquitto *mosq;
    char *host;
    int port;
    int keepalive;
    char *bind_address;
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
