#include "mosquitto_ext.h"

static VALUE rb_mosquitto_run_callback(mosquitto_callback_t *callback);

/*
 * :nodoc:
 *  Pushes a callback onto the client's callback queue. The callback runs within the context of an event thread.
 *
 */
static void mosquitto_callback_queue_push(mosquitto_callback_t *cb)
{
    mosquitto_client_wrapper *client = cb->client;
    cb->next = client->callback_queue;
    client->callback_queue = cb;
}

/*
 * :nodoc:
 *  Pops a callback off the client's callback queue. The callback runs within the context of an event thread.
 *
 */
static mosquitto_callback_t *mosquitto_callback_queue_pop(mosquitto_client_wrapper *client)
{
    mosquitto_callback_t *cb = client->callback_queue;
    if(cb)
    {
        client->callback_queue = cb->next;
    }

    return cb;
}

/*
 * :nodoc:
 *  Runs without the GIL (Global Interpreter Lock) and polls the client's callback queue for any callbacks
 *  to handle.
 *
 *  Only applicable to clients that run with the threaded Mosquitto::Client#loop_start event loop
 *
 */
static void *mosquitto_wait_for_callbacks(void *w)
{
    mosquitto_callback_waiting_t *waiter = (mosquitto_callback_waiting_t *)w;
    mosquitto_client_wrapper *client = waiter->client;

    pthread_mutex_lock(&client->callback_mutex);
    while (!waiter->abort && (waiter->callback = mosquitto_callback_queue_pop(client)) == NULL)
    {
        pthread_cond_wait(&client->callback_cond, &client->callback_mutex);
    }
    pthread_mutex_unlock(&client->callback_mutex);

    return (void *)Qnil;
}

/*
 * :nodoc:
 *  Unblocking function for the callback poller - invoked when the event thread should exit.
 *
 *  Only applicable to clients that run with the threaded Mosquitto::Client#loop_start event loop
 *
 */
static void mosquitto_stop_waiting_for_callbacks(void *w)
{
    mosquitto_callback_waiting_t *waiter = (mosquitto_callback_waiting_t *)w;
    mosquitto_client_wrapper *client = waiter->client;

    pthread_mutex_lock(&client->callback_mutex);
    waiter->abort = 1;
    pthread_mutex_unlock(&client->callback_mutex);
    pthread_cond_signal(&client->callback_cond);
}

/*
 * :nodoc:
 *  Enqueues a callback to be handled by the event thread for a given client.
 *
 *  Callbacks for clients that don't use the threaded Mosquitto::Client#loop_start event loop are invoked
 *  directly within context of the current Ruby thread. Thus there's no locking overhead and associated
 *  cruft.
 *
 */
static void rb_mosquitto_queue_callback(mosquitto_callback_t *callback)
{
    mosquitto_client_wrapper *client = callback->client;
    if (!NIL_P(client->callback_thread)) {
        pthread_mutex_lock(&client->callback_mutex);
        mosquitto_callback_queue_push(callback);
        pthread_mutex_unlock(&client->callback_mutex);
        pthread_cond_signal(&client->callback_cond);
    } else {
        rb_mosquitto_run_callback(callback);
    }
}

/*
 * :nodoc:
 *  Main callback dispatch method. Invokes callback procedures with variable argument counts. It's expected
 *  to raise exceptions - they're handled by a wrapper function.
 *
 */
static VALUE rb_mosquitto_funcall_protected0(VALUE *args)
{
    int argc = args[1];
    VALUE proc = args[0];
    if (NIL_P(proc)) MosquittoError("invalid callback");
    if (argc == 1) {
        rb_funcall(proc, intern_call, 1, args[2]);
    } else if (argc == 2) {
        rb_funcall(proc, intern_call, 2, args[2], args[3]);
    } else if (argc == 3) {
        rb_funcall(proc, intern_call, 3, args[2], args[3], args[4]);
    }
    return Qnil;
}

/*
 * :nodoc:
 *  Invokes the main callback dispatch method, but tracks error state and allows us to take appropriate action
 *  on exception.
 *
 */
static VALUE rb_mosquitto_funcall_protected(int *error_tag, VALUE *args)
{
    rb_protect((VALUE(*)(VALUE))rb_mosquitto_funcall_protected0, (VALUE)args, error_tag);
    return Qnil;
}

/*
 * :nodoc:
 *  Releases resources allocated for a given callback. Mostly callback specific arguments copied from libmosquitto
 *  callback arguments for later processing.
 *
 */
static void rb_mosquitto_free_callback(mosquitto_callback_t *callback)
{
    if (callback->type == ON_LOG_CALLBACK) {
        on_log_callback_args_t *args = (on_log_callback_args_t *)callback->data;
        xfree(args->str);
    }

    xfree(callback->data);
    xfree(callback);
}

/*
 * :nodoc:
 *  Our main callback handler within the Ruby VM. I sets up arguments, it's safe to raise exceptions here and coerces
 *  C specific callback arguments to Ruby counterparts prior to method dispatch.
 *
 */
static void rb_mosquitto_handle_callback(int *error_tag, mosquitto_callback_t *callback)
{
    VALUE args[5];
    mosquitto_client_wrapper *client = callback->client;
    switch (callback->type) {
        case ON_CONNECT_CALLBACK: {
                                    on_connect_callback_args_t *cb = (on_connect_callback_args_t *)callback->data;
                                    args[0] = client->connect_cb;
                                    args[1] = (VALUE)1;
                                    args[2] = INT2NUM(cb->rc);
                                    switch (cb->rc) {
                                        case 1:
                                                MosquittoError("connection refused (unacceptable protocol version)");
                                                break;
                                        case 2:
                                                MosquittoError("connection refused (identifier rejected)");
                                                break;
                                        case 3:
                                                MosquittoError("connection refused (broker unavailable)");
                                                break;
                                        default:
                                                 rb_mosquitto_funcall_protected(error_tag, args);           
                                    }
                                  }
                                  break;

        case ON_DISCONNECT_CALLBACK: {
                                       on_disconnect_callback_args_t *cb = (on_disconnect_callback_args_t *)callback->data;
                                       args[0] = client->disconnect_cb;
                                       args[1] = (VALUE)1;
                                       args[2] = INT2NUM(cb->rc);
                                       rb_mosquitto_funcall_protected(error_tag, args); 
                                     }
                                     break;

        case ON_PUBLISH_CALLBACK: {
                                    on_publish_callback_args_t *cb = (on_publish_callback_args_t *)callback->data;
                                    args[0] = client->publish_cb;
                                    args[1] = (VALUE)1;
                                    args[2] = INT2NUM(cb->mid);
                                    rb_mosquitto_funcall_protected(error_tag, args);
                                  }
                                  break;

        case ON_MESSAGE_CALLBACK: {
                                    on_message_callback_args_t *cb = (on_message_callback_args_t *)callback->data;
                                    args[0] = client->message_cb;
                                    args[1] = (VALUE)1;
                                    args[2] = rb_mosquitto_message_alloc(cb->msg);
                                    rb_mosquitto_funcall_protected(error_tag, args);
                                  }
                                  break;

        case ON_SUBSCRIBE_CALLBACK: {
                                      int i;
                                      on_subscribe_callback_args_t *cb = (on_subscribe_callback_args_t *)callback->data;
                                      VALUE granted_qos = rb_ary_new2(cb->qos_count);
                                      for (i = 0; i < cb->qos_count; i++) {
                                          rb_ary_push(granted_qos, INT2NUM(cb->granted_qos[i]));
                                      }
                                      args[0] = client->subscribe_cb;
                                      args[1] = (VALUE)2;
                                      args[2] = INT2NUM(cb->mid);
                                      args[3] = granted_qos;
                                      rb_mosquitto_funcall_protected(error_tag, args);
                                    }
                                    break;

        case ON_UNSUBSCRIBE_CALLBACK: {
                                        on_unsubscribe_callback_args_t *cb = (on_unsubscribe_callback_args_t *)callback->data;
                                        args[0] = client->unsubscribe_cb;
                                        args[1] = (VALUE)1;
                                        args[2] = INT2NUM(cb->mid);
                                        rb_mosquitto_funcall_protected(error_tag, args);
                                      }
                                      break;

        case ON_LOG_CALLBACK: {
                                on_log_callback_args_t *cb = (on_log_callback_args_t *)callback->data;
                                args[0] = client->log_cb;
                                args[1] = (VALUE)2;
                                args[2] = INT2NUM(cb->level);
                                args[3] = MosquittoEncode(rb_str_new2(cb->str));
                                rb_mosquitto_funcall_protected(error_tag, args);
                              }
                              break;
        }
}

/*
 * :nodoc:
 *  Wrapper function for running callbacks. It respects error / exception status as well as frees any resources
 *  allocated specific to callback processing.
 *
 */
static VALUE rb_mosquitto_run_callback(mosquitto_callback_t *callback)
{
    int error_tag;
    rb_mosquitto_handle_callback(&error_tag, callback);
    rb_mosquitto_free_callback(callback);
    if (error_tag) rb_jump_tag(error_tag);
}

/*
 * :nodoc:
 *  The callback thread - the main workhorse for the threaded Mosquitto::Client#loop_start event loop.
 *
 */
static VALUE rb_mosquitto_callback_thread(void *obj)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)obj;
    mosquitto_callback_waiting_t waiter;
    waiter.callback = NULL;
    waiter.abort = 0;
    waiter.client = client;
    while (!waiter.abort)
    {
        rb_thread_call_without_gvl(mosquitto_wait_for_callbacks, (void *)&waiter, mosquitto_stop_waiting_for_callbacks, (void *)&waiter);
        if (waiter.callback)
        {
            rb_mosquitto_run_callback(waiter.callback);
        }
    }

    return Qnil;
}

/*
 * :nodoc:
 *  On connect callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_connect_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int rc)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_CONNECT_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_connect_callback_args_t *args = MOSQ_ALLOC(on_connect_callback_args_t);
    args->rc = rc;

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On disconnect callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_disconnect_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int rc)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_DISCONNECT_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_disconnect_callback_args_t *args = MOSQ_ALLOC(on_disconnect_callback_args_t);
    args->rc = rc;

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On publish callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_publish_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_PUBLISH_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_publish_callback_args_t *args = MOSQ_ALLOC(on_publish_callback_args_t);
    args->mid = mid;

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On message callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_message_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_MESSAGE_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_message_callback_args_t *args = MOSQ_ALLOC(on_message_callback_args_t);
    args->msg = MOSQ_ALLOC(struct mosquitto_message);
    mosquitto_message_copy(args->msg, msg);

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On subscribe callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_subscribe_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_SUBSCRIBE_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_subscribe_callback_args_t *args = MOSQ_ALLOC(on_subscribe_callback_args_t);
    args->mid = mid;
    args->qos_count = qos_count;
    args->granted_qos = granted_qos;

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On unsubscribe callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_unsubscribe_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_UNSUBSCRIBE_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_unsubscribe_callback_args_t *args = MOSQ_ALLOC(on_unsubscribe_callback_args_t);
    args->mid = mid;

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback);
}

/*
 * :nodoc:
 *  On log callback - invoked by libmosquitto.
 *
 */
static void rb_mosquitto_client_on_log_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int level, const char *str)
{
    mosquitto_callback_t *callback = MOSQ_ALLOC(mosquitto_callback_t);
    callback->type = ON_LOG_CALLBACK;
    callback->client = (mosquitto_client_wrapper *)obj;

    on_log_callback_args_t *args = MOSQ_ALLOC(on_log_callback_args_t);
    args->level = level;
    args->str = strdup(str);

    callback->data = (void *)args;
    rb_mosquitto_queue_callback(callback); 
}

/*
 * :nodoc:
 *  GC callback for Mosquitto::Client objects - invoked during the GC mark phase.
 *
 */
static void rb_mosquitto_mark_client(void *ptr)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)ptr;
    if (client) {
        rb_gc_mark(client->connect_cb);
        rb_gc_mark(client->disconnect_cb);
        rb_gc_mark(client->publish_cb);
        rb_gc_mark(client->message_cb);
        rb_gc_mark(client->subscribe_cb);
        rb_gc_mark(client->unsubscribe_cb);
        rb_gc_mark(client->log_cb);
        rb_gc_mark(client->callback_thread);
    }
}

/*
 * :nodoc:
 *  GC callback for releasing an out of scope Mosquitto::Client object
 *
 */
static void rb_mosquitto_free_client(void *ptr)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)ptr;
    if (client) {
        mosquitto_destroy(client->mosq);
        xfree(client);
    }
}

/*
 * call-seq:
 *   Mosquitto::Client.new("some-id") -> Mosquitto::Client
 *
 * Create a new mosquitto client instance.
 *
 * @param identifier [String] the client identifier. Set to nil to have a random one generated.
 *                            clean_session must be true if the identifier is nil.
 * @param clean_session [true, false] set to true to instruct the broker to clean all messages
 *                                    and subscriptions on disconnect, false to instruct it to
 *                                    keep them
 * @return [Mosquitto::Client] mosquitto client instance
 * @raise [Mosquitto::Error] on invalid input params
 * @note As per the MQTT spec, client identifiers cannot exceed 23 characters
 * @example
 *   Mosquitto::Client.new("session_id") -> Mosquitto::Client
 *   Mosquitto::Client.new(nil, true) -> Mosquitto::Client
 *
 */
static VALUE rb_mosquitto_client_s_new(int argc, VALUE *argv, VALUE client)
{
    VALUE client_id;
    char *cl_id = NULL;
    mosquitto_client_wrapper *cl = NULL;
    bool clean_session;
    rb_scan_args(argc, argv, "01", &client_id);
    if (NIL_P(client_id)) {
        clean_session = true;
    } else {
        clean_session = false;
        Check_Type(client_id, T_STRING);
        MosquittoEncode(client_id);
        cl_id = StringValueCStr(client_id);
    }
    client = Data_Make_Struct(rb_cMosquittoClient, mosquitto_client_wrapper, rb_mosquitto_mark_client, rb_mosquitto_free_client, cl);
    cl->mosq = mosquitto_new(cl_id, clean_session, (void *)cl);
    if (cl->mosq == NULL) {
        xfree(cl);
        switch (errno) {
            case EINVAL:
                MosquittoError("invalid input params");
                break;
            case ENOMEM:
                rb_memerror();
                break;
            default:
                return Qfalse;
        }
    }
    cl->connect_cb = Qnil;
    cl->disconnect_cb = Qnil;
    cl->publish_cb = Qnil;
    cl->message_cb = Qnil;
    cl->subscribe_cb = Qnil;
    cl->unsubscribe_cb = Qnil;
    cl->log_cb = Qnil;
    cl->callback_thread = Qnil;
    cl->callback_queue = NULL;
    rb_obj_call_init(client, 0, NULL);
    return client;
}

static void *rb_mosquitto_client_reinitialise_nogvl(void *ptr)
{
    struct nogvl_reinitialise_args *args = ptr;
    return (void *)mosquitto_reinitialise(args->mosq, args->client_id, args->clean_session, args->obj);
}

/*
 * call-seq:
 *   client.reinitialise("some-id") -> Mosquitto::Client
 *
 * Allows an existing mosquitto client to be reused. Call on a mosquitto instance to close any
 * open network connections, free memory and reinitialise the client with the new parameters.
 *
 * @param identifier [String] the client identifier. Set to nil to have a random one generated.
 *                            clean_session must be true if the identifier is nil.
 * @param clean_session [true, false] set to true to instruct the broker to clean all messages
 *                                    and subscriptions on disconnect, false to instruct it to
 *                                    keep them
 * @return [Mosquitto::Client] mosquitto client instance
 * @raise [Mosquitto::Error] on invalid input params
 * @note As per the MQTT spec, client identifiers cannot exceed 23 characters
 * @example
 *   client.reinitialise("session_id") -> Mosquitto::Client
 *   client.reinitialise(nil, true) -> Mosquitto::Client
 *
 */
static VALUE rb_mosquitto_client_reinitialise(int argc, VALUE *argv, VALUE obj)
{
    struct nogvl_reinitialise_args args;
    VALUE client_id;
    int ret;
    bool clean_session;
    char *cl_id = NULL;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01", &client_id);
    if (NIL_P(client_id)) {
        clean_session = true;
    } else {
        clean_session = false;
        Check_Type(client_id, T_STRING);
        MosquittoEncode(client_id);
        cl_id = StringValueCStr(client_id);
    }
    args.mosq = client->mosq;
    args.client_id = cl_id;
    args.clean_session = clean_session;
    args.obj = (void *)obj;
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_reinitialise_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.will_set("topic", "died", Mosquitto::AT_MOST_ONCE, false) -> Mosquitto::Client
 *
 * Configure will information for a mosquitto instance. By default, clients do not have a will.
 *
 * @param topic [String] the topic on which to publish the will
 * @param payload [String] the message payload. Max 256MB
 * @param qos [Mosquitto::AT_MOST_ONCE, Mosquitto::AT_LEAST_ONCE, Mosquitto::EXACTLY_ONCE] quality
 *            of service used for the will
 * @param retain [true, false] set to true to make the will a retained message
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or a too large payload size
 * @note This must be called before calling Mosquitto::Client#connect
 * @example
 *   client.will_set("will_set", "test", Mosquitto::AT_MOST_ONCE, true)
 *
 */
static VALUE rb_mosquitto_client_will_set(VALUE obj, VALUE topic, VALUE payload, VALUE qos, VALUE retain)
{
    int ret;
    MosquittoGetClient(obj);
    Check_Type(topic, T_STRING);
    MosquittoEncode(topic);
    Check_Type(payload, T_STRING);
    MosquittoEncode(payload);
    Check_Type(qos, T_FIXNUM);
    ret = mosquitto_will_set(client->mosq, StringValueCStr(topic), (int)RSTRING_LEN(payload), StringValueCStr(payload), NUM2INT(qos), ((retain == Qtrue) ? true : false));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_PAYLOAD_SIZE:
           MosquittoError("payload too large");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.will_clear -> Boolean
 *
 * Remove a previously configured will.
 *
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params
 * @note This must be called before calling Mosquitto::Client#connect
 * @example
 *   client.will_clear
 *
 */
static VALUE rb_mosquitto_client_will_clear(VALUE obj)
{
    int ret;
    MosquittoGetClient(obj);
    ret = mosquitto_will_clear(client->mosq);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.auth("username", "password") -> Boolean
 *
 * Configure username and password for a mosquitto instance. This is only supported by brokers that
 * implement the MQTT spec v3.1. By default, no username or password will be sent.
 *
 * @param username [String] the username to send, or nil to disable authentication.
 * @param password [String] the password to send. Set to nil when username is valid in order to send
 *                          just a username.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params
 * @note This must be called before calling Mosquitto::Client#connect
 * @example
 *   client.auth("username", "password")
 *
 */
static VALUE rb_mosquitto_client_auth(VALUE obj, VALUE username, VALUE password)
{
    int ret;
    MosquittoGetClient(obj);
    if (!NIL_P(username)) {
        Check_Type(username, T_STRING);
        MosquittoEncode(username);
    }
    if (!NIL_P(password)) {
        Check_Type(password, T_STRING);
        MosquittoEncode(password);
    }
    ret = mosquitto_username_pw_set(client->mosq, (NIL_P(username) ? NULL : StringValueCStr(username)), (NIL_P(password) ? NULL : StringValueCStr(password)));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.tls_set('/certs/all-ca.crt'), '/certs', '/certs/client.crt'), '/certs/client.key') -> Boolean
 *
 * Configure the client for certificate based SSL/TLS support.
 * 
 * Cannot be used in conjunction with Mosquitto::Client#tls_psk_set.
 *
 * Define the Certificate Authority certificates to be trusted (ie. the server
 * certificate must be signed with one of these certificates) using cafile.
 *
 * If the server you are connecting to requires clients to provide a
 * certificate, define certfile and keyfile with your client certificate and
 * private key
 *
 * @param cafile [String] path to a file containing the PEM encoded trusted CA certificate files.
 *                        Either cafile or capath must not be nil.
 * @param capath [String] path to a directory containing the PEM encoded trusted CA certificate files.
 *                        Either cafile or capath must not be nil.
 * @param certfile [String] path to a file containing the PEM encoded certificate file for this client.
 *                          If nil, keyfile must also be nil and no client certificate will be used.
 * @param keyfile [String] path to a file containing the PEM encoded private key for this client. If nil,
 *                         certfile must also be NULL and no client certificate will be used.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or when TLS is not supported
 * @note This must be called before calling Mosquitto::Client#connect
 * @example
 *   client.tls_set('/certs/all-ca.crt'), '/certs', '/certs/client.crt'), '/certs/client.key')
 *
 */
static VALUE rb_mosquitto_client_tls_set(VALUE obj, VALUE cafile, VALUE capath, VALUE certfile, VALUE keyfile)
{
    int ret;
    MosquittoGetClient(obj);
    if (!NIL_P(cafile)) {
        Check_Type(cafile, T_STRING);
        MosquittoEncode(cafile);
    }
    if (!NIL_P(capath)) {
        Check_Type(capath, T_STRING);
        MosquittoEncode(capath);
    }
    if (!NIL_P(certfile)) {
        Check_Type(certfile, T_STRING);
        MosquittoEncode(certfile);
    }
    if (!NIL_P(keyfile)) {
        Check_Type(keyfile, T_STRING);
        MosquittoEncode(keyfile);
    }

    if (NIL_P(cafile) && NIL_P(capath)) MosquittoError("Either CA path or CA file is required!");
    if (NIL_P(certfile) && !NIL_P(keyfile)) MosquittoError("Key file can only be used with a certificate file!");
    if (NIL_P(keyfile) && !NIL_P(certfile)) MosquittoError("Certificate file also requires a key file!");

    ret = mosquitto_tls_set(client->mosq, (NIL_P(cafile) ? NULL : StringValueCStr(cafile)), (NIL_P(capath) ? NULL : StringValueCStr(capath)), (NIL_P(certfile) ? NULL : StringValueCStr(certfile)), (NIL_P(keyfile) ? NULL : StringValueCStr(keyfile)), NULL);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NOT_SUPPORTED:
           MosquittoError("TLS support is not available");
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.insecure = true -> Boolean
 *
 * Configure verification of the server hostname in the server certificate. If
 * value is set to true, it is impossible to guarantee that the host you are
 * connecting to is not impersonating your server. This can be useful in
 * initial server testing, but makes it possible for a malicious third party to
 * impersonate your server through DNS spoofing, for example.
 * Do not use this function in a real system. Setting value to true makes the
 * connection encryption pointless.
 *
 * @param insecure [true, false] if set to false, the default, certificate hostname checking is
 *                               performed. If set to true, no hostname checking is performed and
 *                               the connection is insecure.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or when TLS is not supported
 * @note This must be called before calling Mosquitto::Client#connect
 * @example
 *   client.insecure = true
 *
 */
static VALUE rb_mosquitto_client_tls_insecure_set(VALUE obj, VALUE insecure)
{
    int ret;
    MosquittoGetClient(obj);
    if (insecure != Qtrue && insecure != Qfalse) {
         rb_raise(rb_eTypeError, "changing TLS verification semantics requires a boolean value");
    }

    ret = mosquitto_tls_insecure_set(client->mosq, ((insecure == Qtrue) ? true : false));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED:
           MosquittoError("TLS support is not available");
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil) -> Boolean
 *
 * Set advanced SSL/TLS options.
 *
 * @param cert_reqs [Mosquitto::SSL_VERIFY_NONE, Mosquitto::SSL_VERIFY_NONE] an integer defining the verification
 *                  requirements the client will impose on the server. The default and recommended value is
 *                  Mosquitto::SSL_VERIFY_PEER. Using Mosquitto::SSL_VERIFY_NONE provides no security.
 * @param tls_version ["tlsv1.2", "tlsv1.1", "tlsv1"] the version of the SSL/TLS protocol to use as a string. If
                                                      nil, the default value is used.
 * @param ciphers [String] a string describing the ciphers available for use. See the `openssl ciphers` tool for
                  more information. If nil, the default ciphers will be used.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or when TLS is not supported
 * @note This must be called before calling Mosquitto::Client#connect
 * @see `openssl ciphers`
 * @example
 *   client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil)
 *
 */
static VALUE rb_mosquitto_client_tls_opts_set(VALUE obj, VALUE cert_reqs, VALUE tls_version, VALUE ciphers)
{
    int ret;
    MosquittoGetClient(obj);
    Check_Type(cert_reqs, T_FIXNUM);
    if (!NIL_P(tls_version)) {
        Check_Type(tls_version, T_STRING);
        MosquittoEncode(tls_version);
    }
    if (!NIL_P(ciphers)) {
        Check_Type(ciphers, T_STRING);
        MosquittoEncode(ciphers);
    }

    if (NUM2INT(cert_reqs) != 0 && NUM2INT(cert_reqs) != 1) {
        MosquittoError("TLS verification requirement should be one of Mosquitto::SSL_VERIFY_NONE or Mosquitto::SSL_VERIFY_PEER");
    }

    ret = mosquitto_tls_opts_set(client->mosq, NUM2INT(cert_reqs), (NIL_P(tls_version) ? NULL : StringValueCStr(tls_version)), (NIL_P(ciphers) ? NULL : StringValueCStr(ciphers)));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NOT_SUPPORTED:
           MosquittoError("TLS support is not available");
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.tls_psk_set("deadbeef", "psk-id", nil) -> Boolean
 *
 * Configure the client for pre-shared-key based TLS support.
 *
 * @param psk [String] the pre-shared-key in hex format with no leading "0x".
 * @param identity [String] the identity of this client. May be used as the username depending on the server
 *                          settings.
 * @param ciphers [String] a string describing the ciphers available for use. See the `openssl ciphers` tool for
                  more information. If nil, the default ciphers will be used.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or when TLS is not supported
 * @note This must be called before calling Mosquitto::Client#connect
 * @see `openssl ciphers`
 * @example
 *   client.tls_psk_set("deadbeef", "psk-id", nil)
 *
 */
static VALUE rb_mosquitto_client_tls_psk_set(VALUE obj, VALUE psk, VALUE identity, VALUE ciphers)
{
    int ret;
    MosquittoGetClient(obj);
    Check_Type(psk, T_STRING);
    Check_Type(identity, T_STRING);
    if (!NIL_P(ciphers)) {
        Check_Type(ciphers, T_STRING);
        MosquittoEncode(ciphers);
    }

    ret = mosquitto_tls_psk_set(client->mosq, StringValueCStr(psk), StringValueCStr(identity), (NIL_P(ciphers) ? NULL : StringValueCStr(ciphers)));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NOT_SUPPORTED:
           MosquittoError("TLS support is not available");
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_connect_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return (void *)mosquitto_connect(args->mosq, args->host, args->port, args->keepalive);
}

/*
 * call-seq:
 *   client.connect("localhost", 1883, 10) -> Boolean
 *
 * Connect to an MQTT broker.
 *
 * @param host [String] the hostname or ip address of the broker to connect to.
 * @param port [Integer] the network port to connect to. Usually 1883 (or 8883 for TLS)
 * @param keepalive [Integer] the number of seconds after which the broker should send a PING message
 *                            to the client if no other messages have been exchanged in that time.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.connect("localhost", 1883, 10)
 *
 */
static VALUE rb_mosquitto_client_connect(VALUE obj, VALUE host, VALUE port, VALUE keepalive)
{
    struct nogvl_connect_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(host, T_STRING);
    MosquittoEncode(host);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_connect_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_connect_bind_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return (void *)mosquitto_connect_bind(args->mosq, args->host, args->port, args->keepalive, args->bind_address);
}

/*
 * call-seq:
 *   client.connect_bind("localhost", 1883, 10, "10.0.0.3") -> Boolean
 *
 * Connect to an MQTT broker. This extends the functionality of Mosquitto::Client#connect by adding the bind_address
 * parameter. Use this function if you need to restrict network communication over a particular interface.
 *
 * @param host [String] the hostname or ip address of the broker to connect to.
 * @param port [Integer] the network port to connect to. Usually 1883 (or 8883 for TLS)
 * @param keepalive [Integer] the number of seconds after which the broker should send a PING message
 *                            to the client if no other messages have been exchanged in that time.
 * @param bind_address [String] the hostname or ip address of the local network interface to bind to
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.connect_bind("localhost", 1883, 10, "10.0.0.3")
 *
 */
static VALUE rb_mosquitto_client_connect_bind(VALUE obj, VALUE host, VALUE port, VALUE keepalive, VALUE bind_address)
{
    struct nogvl_connect_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(host, T_STRING);
    MosquittoEncode(host);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    Check_Type(bind_address, T_STRING);
    MosquittoEncode(bind_address);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    args.bind_address = StringValueCStr(bind_address);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_connect_bind_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect_bind");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_connect_async_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return mosquitto_connect_async(args->mosq, args->host, args->port, args->keepalive);
}

/*
 * call-seq:
 *   client.connect_async("localhost", 1883, 10) -> Boolean
 *
 * Connect to an MQTT broker. This is a non-blocking call. If you use
 * Mosquitto::Client#connect_async your client must use the threaded interface
 * Mosquitto::Client#loop_start. If you need to use Mosquitto::Client#loop, you must use
 * Mosquitto::Client#connect to connect the client.
 *
 * @param host [String] the hostname or ip address of the broker to connect to.
 * @param port [Integer] the network port to connect to. Usually 1883 (or 8883 for TLS)
 * @param keepalive [Integer] the number of seconds after which the broker should send a PING message
 *                            to the client if no other messages have been exchanged in that time.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.connect_async("localhost", 1883, 10)
 *
 */
static VALUE rb_mosquitto_client_connect_async(VALUE obj, VALUE host, VALUE port, VALUE keepalive)
{
    struct nogvl_connect_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(host, T_STRING);
    MosquittoEncode(host);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_connect_async_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect_async");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_connect_bind_async_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return mosquitto_connect_bind_async(args->mosq, args->host, args->port, args->keepalive, args->bind_address);
}

/*
 * call-seq:
 *   client.connect_bind_async("localhost", 1883, 10, "10.0.0.3") -> Boolean
 *
 * Connect to an MQTT broker. This is a non-blocking call. If you use
 * Mosquitto::Client#connect_async your client must use the threaded interface
 * Mosquitto::Client#loop_start. If you need to use Mosquitto::Client#loop, you must use
 * Mosquitto::Client#connect to connect the client.
 *
 * This extends the functionality of Mosquitto::Client#connect_async by adding the
 * bind_address parameter. Use this function if you need to restrict network
 * communication over a particular interface.
 *
 * @param host [String] the hostname or ip address of the broker to connect to.
 * @param port [Integer] the network port to connect to. Usually 1883 (or 8883 for TLS)
 * @param keepalive [Integer] the number of seconds after which the broker should send a PING message
 *                            to the client if no other messages have been exchanged in that time.
 * @param bind_address [String] the hostname or ip address of the local network interface to bind to
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.connect_bind_async("localhost", 1883, 10, "10.0.0.3")
 *
 */
static VALUE rb_mosquitto_client_connect_bind_async(VALUE obj, VALUE host, VALUE port, VALUE keepalive, VALUE bind_address)
{
    struct nogvl_connect_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(host, T_STRING);
    MosquittoEncode(host);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    Check_Type(bind_address, T_STRING);
    MosquittoEncode(bind_address);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    args.bind_address = StringValueCStr(bind_address);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_connect_bind_async_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect_bind_async");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_reconnect_nogvl(void *ptr)
{
    return mosquitto_reconnect((struct mosquitto *)ptr);
}

/*
 * call-seq:
 *   client.reconnect -> Boolean
 *
 * Reconnect to a broker.
 *
 * This function provides an easy way of reconnecting to a broker after a connection has been lost.
 * It uses the values that were provided in the Mosquitto::Client#connect call.
 *
 * @return [true] on success
 * @note It must not be called before Mosquitto::Client#connect
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.reconnect
 *
 */
static VALUE rb_mosquitto_client_reconnect(VALUE obj)
{
    int ret;
    MosquittoGetClient(obj);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_reconnect_nogvl, (void *)client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_reconnect");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_disconnect_nogvl(void *ptr)
{
    return (VALUE)mosquitto_disconnect((struct mosquitto *)ptr);
}

/*
 * call-seq:
 *   client.disconnect-> Boolean
 *
 * Disconnect from the broker.
 *
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or if the client is not connected
 * @example
 *   client.disconnect
 *
 */
static VALUE rb_mosquitto_client_disconnect(VALUE obj)
{
    int ret;
    MosquittoGetClient(obj);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_disconnect_nogvl, (void *)client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_publish_nogvl(void *ptr)
{
    struct nogvl_publish_args *args = ptr;
    return (VALUE)mosquitto_publish(args->mosq, args->mid, args->topic, args->payloadlen, args->payload, args->qos, args->retain);
}

/*
 * call-seq:
 *   client.publish(3, "publish", "test", Mosquitto::AT_MOST_ONCE, true) -> Boolean
 *
 * Publish a message on a given topic.
 *
 * @param mid [Integer, nil] If not nil, the function will set this to the message id of this particular message.
 *                           This can be then used with the publish callback to determine when the message has been
 *                           sent. Note that although the MQTT protocol doesn't use message ids for messages with
 *                           QoS=0, libmosquitto assigns them message ids so they can be tracked with this parameter.
 * @param payload [String] Message payload to send. Max 256MB
 * @param qos [Mosquitto::AT_MOST_ONCE, Mosquitto::AT_LEAST_ONCE, Mosquitto::EXACTLY_ONCE] Quality of Service to be
 *            used for the message.
 * @param retain [true, false] set to true to make the message retained
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.publish(3, "publish", "test", Mosquitto::AT_MOST_ONCE, true)
 *
 */
static VALUE rb_mosquitto_client_publish(VALUE obj, VALUE mid, VALUE topic, VALUE payload, VALUE qos, VALUE retain)
{
    struct nogvl_publish_args args;
    int ret, msg_id;
    MosquittoGetClient(obj);
    Check_Type(topic, T_STRING);
    MosquittoEncode(topic);
    Check_Type(payload, T_STRING);
    MosquittoEncode(payload);
    Check_Type(qos, T_FIXNUM);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = NUM2INT(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.topic = StringValueCStr(topic);
    args.payloadlen = (int)RSTRING_LEN(payload);
    args.payload = (const char *)StringValueCStr(payload);
    args.qos = NUM2INT(qos);
    args.retain = (retain == Qtrue) ? true : false;
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_publish_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with broker");
           break;
       case MOSQ_ERR_PAYLOAD_SIZE:
           MosquittoError("payload too large");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_subscribe_nogvl(void *ptr)
{
    struct nogvl_subscribe_args *args = ptr;
    return (VALUE)mosquitto_subscribe(args->mosq, args->mid, args->subscription, args->qos);
}

/*
 * call-seq:
 *   client.subscribe(3, "subscribe", Mosquitto::AT_MOST_ONCE) -> Boolean
 *
 * Subscribe to a topic.
 *
 * @param mid [Integer, nil] If not nil, the function will set this to the message id of this particular message.
 *                           This can be then used with the subscribe callback to determine when the message has been
 *                           sent.
 * @param subscription [String] The subscription pattern
 * @param qos [Mosquitto::AT_MOST_ONCE, Mosquitto::AT_LEAST_ONCE, Mosquitto::EXACTLY_ONCE] Quality of Service to be
 *            used for the subscription
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.subscribe(3, "subscribe", Mosquitto::AT_MOST_ONCE)
 *
 */
static VALUE rb_mosquitto_client_subscribe(VALUE obj, VALUE mid, VALUE subscription, VALUE qos)
{
    struct nogvl_subscribe_args args;
    int ret, msg_id;
    MosquittoGetClient(obj);
    Check_Type(subscription, T_STRING);
    MosquittoEncode(subscription);
    Check_Type(qos, T_FIXNUM);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = NUM2INT(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.subscription = StringValueCStr(subscription);
    args.qos = NUM2INT(qos);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_subscribe_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_unsubscribe_nogvl(void *ptr)
{
    struct nogvl_subscribe_args *args = ptr;
    return (VALUE)mosquitto_unsubscribe(args->mosq, args->mid, args->subscription);
}

/*
 * call-seq:
 *   client.unsubscribe(3, "unsubscribe") -> Boolean
 *
 * Unsubscribe from a topic.
 *
 * @param mid [Integer, nil] If not nil, the function will set this to the message id of this particular message.
 *                           This can be then used with the unsubscribe callback to determine when the message has been
 *                           sent.
 * @param subscription [String] the unsubscription pattern.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.unsubscribe(3, "unsubscribe")
 *
 */
static VALUE rb_mosquitto_client_unsubscribe(VALUE obj, VALUE mid, VALUE subscription)
{
    struct nogvl_subscribe_args args;
    int ret, msg_id;
    MosquittoGetClient(obj);
    Check_Type(subscription, T_STRING);
    MosquittoEncode(subscription);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = NUM2INT(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.subscription = StringValueCStr(subscription);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_unsubscribe_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.socket -> Integer
 *
 * Return the socket handle for a mosquitto instance. Useful if you want to include a mosquitto client in your own
 * select() calls.
 *
 * @return [Integer] socket identifier, or -1 on failure
 * @example
 *   client.socket
 *
 */
static VALUE rb_mosquitto_client_socket(VALUE obj)
{
    int socket;
    MosquittoGetClient(obj);
    socket = mosquitto_socket(client->mosq);
    return INT2NUM(socket);
}

static void *rb_mosquitto_client_loop_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop(args->mosq, args->timeout, args->max_packets);
}

/*
 * call-seq:
 *   client.loop(10, 10) -> Boolean
 *
 * The main network loop for the client. You must call this frequently in order
 * to keep communications between the client and broker working. If incoming
 * data is present it will then be processed. Outgoing commands, from e.g.
 * Mosquitto::Client#publish, are normally sent immediately that their function is
 * called, but this is not always possible. Mosquitto::Client#loop will also attempt
 * to send any remaining outgoing messages, which also includes commands that
 * are part of the flow for messages with QoS>0.
 *
 * An alternative approach is to use Mosquitto::Client#loop_start to run the client
 * loop in its own thread.
 *
 * This calls select() to monitor the client network socket. If you want to
 * integrate mosquitto client operation with your own select() call, use
 * Mosquitto::Client#socket, Mosquitto::Client#loop_read, Mosquitto::Client#loop_write and
 * Mosquitto::Client#loop_misc.
 *
 * @param timeout [Integer] Maximum number of milliseconds to wait for network activity in the select()
 *                          call before timing out. Set to 0 for instant return.  Set negative to use the
 *                          default of 1000ms
 * @param max_packets [Integer] this parameter is currently unused and should be set to 1 for future compatibility.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.loop(10, 10)
 *
 */
static VALUE rb_mosquitto_client_loop(VALUE obj, VALUE timeout, VALUE max_packets)
{
    struct nogvl_loop_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(timeout, T_FIXNUM);
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.timeout = NUM2INT(timeout);
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_forever_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_forever(args->mosq, args->timeout, args->max_packets);
}

static void rb_mosquitto_client_loop_forever_ubf(void *ptr)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)ptr;
    mosquitto_disconnect(client->mosq);
}

/*
 * call-seq:
 *   client.loop_forever(10, 1) -> Boolean
 *
 * This function calls Mosquitto::Client#loop for you in an infinite blocking loop. It is useful
 * for the case where you only want to run the MQTT client loop in your program.
 *
 * It handles reconnecting in case server connection is lost. If you call Mosquitto::Client#disconnect in
 * a callback it will return.
 *
 * @param timeout [Integer] Maximum number of milliseconds to wait for network activity in the select()
 *                          call before timing out. Set to 0 for instant return.  Set negative to use the
 *                          default of 1000ms
 * @param max_packets [Integer] this parameter is currently unused and should be set to 1 for future compatibility.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.loop_forever(10, 1)
 *
 */
static VALUE rb_mosquitto_client_loop_forever(VALUE obj, VALUE timeout, VALUE max_packets)
{
    struct nogvl_loop_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(timeout, T_FIXNUM);
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.timeout = NUM2INT(timeout);
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_forever_nogvl, (void *)&args, rb_mosquitto_client_loop_forever_ubf, client);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_start_nogvl(void *ptr)
{
    return (VALUE)mosquitto_loop_start((struct mosquitto *)ptr);
}

/*
 * call-seq:
 *   client.loop_start -> Boolean
 *
 * This is part of the threaded client interface. Call this once to start a new
 * thread to process network traffic. This provides an alternative to repeatedly calling
 * Mosquitto::Client#loop yourself.
 *
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or if thread support is not available
 * @example
 *   client.loop_start
 *
 */
static VALUE rb_mosquitto_client_loop_start(VALUE obj)
{
    int ret;
    struct timeval time;
    MosquittoGetClient(obj);
    /* Let's not spawn duplicate threaded loops */
    if (!NIL_P(client->callback_thread)) return Qtrue;
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_start_nogvl, (void *)client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           pthread_mutex_init(&client->callback_mutex, NULL);
           pthread_cond_init(&client->callback_cond, NULL);
           client->callback_thread = rb_thread_create(rb_mosquitto_callback_thread, client);
           /* Allow the callback thread some startup time */
           time.tv_sec  = 0;
           time.tv_usec = 100 * 1000;  /* 0.1 sec */
           rb_thread_wait_for(time);
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_stop_nogvl(void *ptr)
{
    struct nogvl_loop_stop_args *args = ptr;
    return (VALUE)mosquitto_loop_stop(args->mosq, args->force);
}

/*
 * call-seq:
 *   client.loop_start -> Boolean
 *
 * This is part of the threaded client interface. Call this once to stop the
 * network thread previously created with Mosquitto::Client#loop_start. This call
 * will block until the network thread finishes. For the network thread to end,
 * you must have previously called Mosquitto::Client#disconnect or have set the force
 * parameter to true.
 *
 * @param force [Boolean] set to true to force thread cancellation. If false, Mosquitto::Client#disconnect
 *                        must have already been called.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or if thread support is not available
 * @example
 *   client.loop_start
 *
 */
static VALUE rb_mosquitto_client_loop_stop(VALUE obj, VALUE force)
{
    struct nogvl_loop_stop_args args;
    int ret;
    MosquittoGetClient(obj);
    args.mosq = client->mosq;
    args.force = ((force == Qtrue) ? true : false);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_stop_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           pthread_mutex_destroy(&client->callback_mutex);
           pthread_cond_destroy(&client->callback_cond);
           rb_thread_kill(client->callback_thread);
           client->callback_thread = Qnil;
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_read_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_read(args->mosq, args->max_packets);
}

/*
 * call-seq:
 *   client.loop_read(10) -> Boolean
 *
 * Carry out network read operations. This should only be used if you are not using Mosquitto::Client#loop and
 * are monitoring the client network socket for activity yourself.
 *
 * @param max_packets [Integer] this parameter is currently unused and should be set to 1 for
 *                              future compatibility.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.loop_read(10)
 *
 */
static VALUE rb_mosquitto_client_loop_read(VALUE obj, VALUE max_packets)
{
    struct nogvl_loop_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_read_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_write_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_write(args->mosq, args->max_packets);
}

/*
 * call-seq:
 *   client.loop_write(1) -> Boolean
 *
 * Carry out network write operations. This should only be used if you are not using Mosquitto::Client#loop and
 * are monitoring the client network socket for activity yourself.
 *
 * @param max_packets [Integer] this parameter is currently unused and should be set to 1 for
 *                              future compatibility.
 * @return [true] on success
 * @raise [Mosquitto::Error, SystemCallError] on invalid input params or system call errors
 * @example
 *   client.loop_write(1)
 *
 */
static VALUE rb_mosquitto_client_loop_write(VALUE obj, VALUE max_packets)
{
    struct nogvl_loop_args args;
    int ret;
    MosquittoGetClient(obj);
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_write_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static void *rb_mosquitto_client_loop_misc_nogvl(void *ptr)
{
    return (VALUE)mosquitto_loop_misc((struct mosquitto *)ptr);
}

/*
 * call-seq:
 *   client.loop_misc -> Boolean
 *
 * Carry out miscellaneous operations required as part of the network loop.
 * This should only be used if you are not using Mosquitto::Client#loop and are
 * monitoring the client network socket for activity yourself.
 *
 * This function deals with handling PINGs and checking whether messages need
 * to be retried, so should be called fairly frequently.
 *
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params or when not connected to the broker
 * @example
 *   client.loop_misc
 *
 */
static VALUE rb_mosquitto_client_loop_misc(VALUE obj)
{
    int ret;
    MosquittoGetClient(obj);
    ret = (int)rb_thread_call_without_gvl(rb_mosquitto_client_loop_misc_nogvl, (void *)client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.want_write? -> Boolean
 *
 * Returns true if there is data ready to be written on the socket.
 *
 * @return [true, false] true if there is data ready to be written on the socket
 * @example
 *   client.want_write
 *
 */
static VALUE rb_mosquitto_client_want_write(VALUE obj)
{
    bool ret;
    MosquittoGetClient(obj);
    ret = mosquitto_want_write(client->mosq);
    return (ret == true) ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   client.reconnect_delay_set(2, 10, true) -> Boolean
 *
 * Control the behaviour of the client when it has unexpectedly disconnected in
 * Mosquitto::Client#loop_forever or after Mosquitto::Client#loop_start. The default
 * behaviour if this function is not used is to repeatedly attempt to reconnect
 * with a delay of 1 second until the connection succeeds.
 *
 * Use reconnect_delay parameter to change the delay between successive
 * reconnection attempts. You may also enable exponential backoff of the time
 * between reconnections by setting reconnect_exponential_backoff to true and
 * set an upper bound on the delay with reconnect_delay_max.
 *
 * Example 1:
 *	delay=2, delay_max=10, exponential_backoff=False
 *	Delays would be: 2, 4, 6, 8, 10, 10, ...
 *
 * Example 2:
 *	delay=3, delay_max=30, exponential_backoff=True
 *	Delays would be: 3, 6, 12, 24, 30, 30, ...
 *
 * @param delay [Integer] the number of seconds to wait between reconnects
 * @param delay_max [Integer] the maximum number of seconds to wait between reconnects
 * @param exponential_backoff [true, false] use exponential backoff between reconnect attempts.
                                        Set to true to enable exponential backoff.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params
 * @example
 *   client.reconnect_delay_set(2, 10, true)
 *
 */
static VALUE rb_mosquitto_client_reconnect_delay_set(VALUE obj, VALUE delay, VALUE delay_max, VALUE exp_backoff)
{
    int ret;
    MosquittoGetClient(obj);
    Check_Type(delay, T_FIXNUM);
    Check_Type(delay_max, T_FIXNUM);
    ret = mosquitto_reconnect_delay_set(client->mosq, INT2NUM(delay), INT2NUM(delay_max), ((exp_backoff == Qtrue) ? true : false));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.max_inflight_messages = 10 -> Boolean
 *
 * Set the number of QoS 1 and 2 messages that can be "in flight" at one time.
 * An in flight message is part way through its delivery flow. Attempts to send
 * further messages with mosquitto::Client#publish will result in the messages being
 * queued until the number of in flight messages reduces.
 *
 * A higher number here results in greater message throughput, but if set
 * higher than the maximum in flight messages on the broker may lead to
 * delays in the messages being acknowledged.
 *
 * Set to 0 for no maximum.
 *
 * @param max_messages [Integer] the maximum number of inflight messages. Defaults to 20.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params
 * @example
 *   client.max_inflight_messages = 10
 *
 */
static VALUE rb_mosquitto_client_max_inflight_messages_equals(VALUE obj, VALUE max_messages)
{
    int ret;
    MosquittoGetClient(obj);
    Check_Type(max_messages, T_FIXNUM);
    ret = mosquitto_max_inflight_messages_set(client->mosq, INT2NUM(max_messages));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       default:
           return Qtrue;
    }
}

/*
 * call-seq:
 *   client.message_retry = 10 -> Boolean
 *
 * Set the number of seconds to wait before retrying messages. This applies to
 * publish messages with QoS>0. May be called at any time.
 *
 * @param message_retry [Integer] the number of seconds to wait for a response before retrying. Defaults to 20.
 * @return [true] on success
 * @raise [Mosquitto::Error] on invalid input params
 * @example
 *   client.message_retry = 10
 *
 */
static VALUE rb_mosquitto_client_message_retry_equals(VALUE obj, VALUE seconds)
{
    MosquittoGetClient(obj);
    Check_Type(seconds, T_FIXNUM);
    mosquitto_message_retry_set(client->mosq, INT2NUM(seconds));
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_connect{|rc| p :connected } -> Boolean
 *
 * Set the connect callback. This is called when the broker sends a CONNACK
 * message in response to a connection.
 *
 * @yield connect callback
 * @yieldparam rc [Integer] the return code of the connection response, one of: 0 - success,
 *                          1 - connection refused (unacceptable protocol version),
 *                          2 - connection refused (identifier rejected)
 *                          3 - connection refused (broker unavailable)
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_connect{|rc| p :connected }
 *
 */
static VALUE rb_mosquitto_client_on_connect(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    if (!NIL_P(client->connect_cb)) rb_gc_unregister_address(&client->connect_cb);
    mosquitto_connect_callback_set(client->mosq, rb_mosquitto_client_on_connect_cb);
    client->connect_cb = cb;
    rb_gc_register_address(&client->connect_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_disconnect{|rc| p :disconnected } -> Boolean
 *
 * Set the disconnect callback. This is called when the broker has received the
 * DISCONNECT command and has disconnected the client.
 *
 * @yield disconnect callback
 * @yieldparam rc [Integer] integer value indicating the reason for the disconnect. A value of 0
 *         means the client has called Mosquitto::Client#disconnect. Any other value indicates that
 *         the disconnect is unexpected.
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_disconnect{|rc| p :disconnected }
 *
 */
static VALUE rb_mosquitto_client_on_disconnect(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    if (!NIL_P(client->disconnect_cb)) rb_gc_unregister_address(&client->disconnect_cb);
    mosquitto_disconnect_callback_set(client->mosq, rb_mosquitto_client_on_disconnect_cb);
    client->disconnect_cb = cb;
    rb_gc_register_address(&client->disconnect_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_publish{|mid| p :published } -> Boolean
 *
 * Set the publish callback. This is called when a message initiated with
 * Mosquitto::Client#publish has been sent to the broker successfully.
 *
 * @yield publish callback
 * @yieldparam mid [Integer] the message id of the sent message
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_publish{|mid| p :published }
 *
 */
static VALUE rb_mosquitto_client_on_publish(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    if (!NIL_P(client->publish_cb)) rb_gc_unregister_address(&client->publish_cb);
    mosquitto_publish_callback_set(client->mosq, rb_mosquitto_client_on_publish_cb);
    client->publish_cb = cb;
    rb_gc_register_address(&client->publish_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_message{|msg| p msg } -> Boolean
 *
 * Set the message callback. This is called when a message is received from the
 * broker.
 *
 * @yield message callback
 * @yieldparam msg [Mosquitto::Message] the message data
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_message{|msg| p msg }
 *
 */
static VALUE rb_mosquitto_client_on_message(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    if (!NIL_P(client->message_cb)) rb_gc_unregister_address(&client->message_cb);
    mosquitto_message_callback_set(client->mosq, rb_mosquitto_client_on_message_cb);
    client->message_cb = cb;
    rb_gc_register_address(&client->message_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_subscribe{|mid, granted_qos| p :subscribed } -> Boolean
 *
 * Set the subscribe callback. This is called when the broker responds to a
 * subscription request.
 *
 * @yield subscription callback
 * @yieldparam mid [Integer] the message id of the subscribe message.
 * @yieldparam granted_qos [Array] an array of integers indicating the granted QoS for each of
 *                                 the subscriptions.
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_subscribe{|mid, granted_qos| p :subscribed }
 *
 */
static VALUE rb_mosquitto_client_on_subscribe(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 2);
    if (!NIL_P(client->subscribe_cb)) rb_gc_unregister_address(&client->subscribe_cb);
    mosquitto_subscribe_callback_set(client->mosq, rb_mosquitto_client_on_subscribe_cb);
    client->subscribe_cb = cb;
    rb_gc_register_address(&client->subscribe_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_unsubscribe{|mid| p :unsubscribed } -> Boolean
 *
 * Set the unsubscribe callback. This is called when the broker responds to a
 * unsubscription request.
 *
 * @yield unsubscribe callback
 * @yieldparam mid [Integer] the message id of the unsubscribe message.

 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_unsubscribe{|mid| p :unsubscribed }
 *
 */
static VALUE rb_mosquitto_client_on_unsubscribe(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    if (!NIL_P(client->unsubscribe_cb)) rb_gc_unregister_address(&client->unsubscribe_cb);
    mosquitto_unsubscribe_callback_set(client->mosq, rb_mosquitto_client_on_unsubscribe_cb);
    client->unsubscribe_cb = cb;
    rb_gc_register_address(&client->unsubscribe_cb);
    return Qtrue;
}

/*
 * call-seq:
 *   client.on_log{|level, msg| p msg } -> Boolean
 *
 * Set the logging callback. This should be used if you want event logging
 * information from the client library.
 *
 * @yield unsubscribe callback
 * @yieldparam level [Mosquitto::LOG_INFO, Mosquitto::LOG_NOTICE, Mosquitto::LOG_WARNING,
 *                    Mosquitto::LOG_ERR, Mosquitto::LOG_DEBUG] the log message level
 * @yieldparam msg [String] log message
 * @return [true] on success
 * @raise [TypeError, ArgumentError] if callback is not a Proc or if the method arity is wrong
 * @example
 *   client.on_log{|level, msg| p msg }
 *
 */
static VALUE rb_mosquitto_client_on_log(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc, cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 2);
    if (!NIL_P(client->log_cb)) rb_gc_unregister_address(&client->log_cb);
    mosquitto_log_callback_set(client->mosq, rb_mosquitto_client_on_log_cb);
    client->log_cb = cb;
    rb_gc_register_address(&client->log_cb);
    return Qtrue;
}

void _init_rb_mosquitto_client()
{
    rb_cMosquittoClient = rb_define_class_under(rb_mMosquitto, "Client", rb_cObject);

    /* Init / setup specific methods */

    rb_define_singleton_method(rb_cMosquittoClient, "new", rb_mosquitto_client_s_new, -1);
    rb_define_method(rb_cMosquittoClient, "reinitialise", rb_mosquitto_client_reinitialise, -1);
    rb_define_method(rb_cMosquittoClient, "will_set", rb_mosquitto_client_will_set, 4);
    rb_define_method(rb_cMosquittoClient, "will_clear", rb_mosquitto_client_will_clear, 0);
    rb_define_method(rb_cMosquittoClient, "auth", rb_mosquitto_client_auth, 2);

    /* Network specific methods */

    rb_define_method(rb_cMosquittoClient, "connect", rb_mosquitto_client_connect, 3);
    rb_define_method(rb_cMosquittoClient, "connect_bind", rb_mosquitto_client_connect_bind, 4);
    rb_define_method(rb_cMosquittoClient, "connect_async", rb_mosquitto_client_connect_async, 3);
    rb_define_method(rb_cMosquittoClient, "connect_bind_async", rb_mosquitto_client_connect_bind_async, 4);
    rb_define_method(rb_cMosquittoClient, "reconnect", rb_mosquitto_client_reconnect, 0);
    rb_define_method(rb_cMosquittoClient, "disconnect", rb_mosquitto_client_disconnect, 0);

    /* Messaging specific methods */

    rb_define_method(rb_cMosquittoClient, "publish", rb_mosquitto_client_publish, 5);
    rb_define_method(rb_cMosquittoClient, "subscribe", rb_mosquitto_client_subscribe, 3);
    rb_define_method(rb_cMosquittoClient, "unsubscribe", rb_mosquitto_client_unsubscribe, 2);

    /* Main / event loop specific methods */

    rb_define_method(rb_cMosquittoClient, "socket", rb_mosquitto_client_socket, 0);
    rb_define_method(rb_cMosquittoClient, "loop", rb_mosquitto_client_loop, 2);
    rb_define_method(rb_cMosquittoClient, "loop_start", rb_mosquitto_client_loop_start, 0);
    rb_define_method(rb_cMosquittoClient, "loop_forever", rb_mosquitto_client_loop_forever, 2);
    rb_define_method(rb_cMosquittoClient, "loop_stop", rb_mosquitto_client_loop_stop, 1);
    rb_define_method(rb_cMosquittoClient, "loop_read", rb_mosquitto_client_loop_read, 1);
    rb_define_method(rb_cMosquittoClient, "loop_write", rb_mosquitto_client_loop_write, 1);
    rb_define_method(rb_cMosquittoClient, "loop_misc", rb_mosquitto_client_loop_misc, 0);
    rb_define_method(rb_cMosquittoClient, "want_write?", rb_mosquitto_client_want_write, 0);

    /* Tuning specific methods */

    rb_define_method(rb_cMosquittoClient, "reconnect_delay_set", rb_mosquitto_client_reconnect_delay_set, 3);
    rb_define_method(rb_cMosquittoClient, "max_inflight_messages=", rb_mosquitto_client_max_inflight_messages_equals, 1);
    rb_define_method(rb_cMosquittoClient, "message_retry=", rb_mosquitto_client_message_retry_equals, 1);

    /* TLS specific methods */

    rb_define_method(rb_cMosquittoClient, "tls_set", rb_mosquitto_client_tls_set, 4);
    rb_define_method(rb_cMosquittoClient, "tls_insecure=", rb_mosquitto_client_tls_insecure_set, 1);
    rb_define_method(rb_cMosquittoClient, "tls_opts_set", rb_mosquitto_client_tls_opts_set, 3);
    rb_define_method(rb_cMosquittoClient, "tls_psk_set", rb_mosquitto_client_tls_psk_set, 3);

    /* Callback specific methods */

    rb_define_method(rb_cMosquittoClient, "on_connect", rb_mosquitto_client_on_connect, -1);
    rb_define_method(rb_cMosquittoClient, "on_disconnect", rb_mosquitto_client_on_disconnect, -1);
    rb_define_method(rb_cMosquittoClient, "on_publish", rb_mosquitto_client_on_publish, -1);
    rb_define_method(rb_cMosquittoClient, "on_message", rb_mosquitto_client_on_message, -1);
    rb_define_method(rb_cMosquittoClient, "on_subscribe", rb_mosquitto_client_on_subscribe, -1);
    rb_define_method(rb_cMosquittoClient, "on_unsubscribe", rb_mosquitto_client_on_unsubscribe, -1);
    rb_define_method(rb_cMosquittoClient, "on_log", rb_mosquitto_client_on_log, -1);
}
