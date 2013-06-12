* Let close_session be an argument to new client + reinit as well
* https://github.com/jobytaffey/mqtt-http-server
* Thread support
  - release GIL when processing callbacks ?
  - [X] rb_mosquitto_client_loop_write
  - [X] rb_mosquitto_client_loop_read
  - [X] rb_mosquitto_client_loop_stop
  - [X] rb_mosquitto_client_loop_start
  - [X] rb_mosquitto_client_loop
  - [X] rb_mosquitto_client_loop_forever
  - [X] rb_mosquitto_client_subscribe
  - [X] rb_mosquitto_client_unsubscribe
  - [X] rb_mosquitto_client_publish
  - [X] rb_mosquitto_client_connect
  - [X] rb_mosquitto_client_disconnect
  - [X] rb_mosquitto_client_reconnect
  - [X] rb_mosquitto_client_connect_async
  - [X] rb_mosquitto_client_reinitialise
* TLS support