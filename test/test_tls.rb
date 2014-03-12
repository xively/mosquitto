# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestTls < MosquittoTestCase
  def test_tls_set
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.tls_set(nil, nil, ssl_object('client.crt'), ssl_object('client.key'))
    end
    assert_raises Mosquitto::Error do
      client.tls_set(ssl_object('all-ca.crt'), ssl_path, ssl_object('client.crt'), nil)
    end
    assert_raises Mosquitto::Error do
      client.tls_set(ssl_object('all-ca.crt'), ssl_path, nil, ssl_object('client.key'))
    end
    assert_raises TypeError do
      client.tls_set(ssl_object('all-ca.crt'), ssl_path, ssl_object('client.crt'), :invalid)
    end
  end

  def test_connect
    connected = false
    client = Mosquitto::Client.new
    assert client.loop_start
    client.logger = Logger.new(STDOUT)
    client.on_connect do |rc|
      connected = true
    end
    assert client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil)
    client.tls_set(ssl_object('all-ca.crt'), nil, ssl_object('client.crt'), ssl_object('client.key'))
    assert client.connect(TLS_TEST_HOST, TLS_TEST_PORT, 10)
    sleep 2
    assert connected
  ensure
    client.loop_stop(true)
  end

  def test_insecure
    client = Mosquitto::Client.new
    assert_raises TypeError do
      client.tls_insecure = nil
    end
    assert (client.tls_insecure = true)
  end

  def test_tls_opts_set
    client = Mosquitto::Client.new
    assert_raises Mosquitto::Error do
      client.tls_opts_set(3, nil, nil)
    end
    assert_raises TypeError do
      client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, :invalid, nil)
    end
    assert client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil)
    assert client.tls_opts_set(Mosquitto::SSL_VERIFY_PEER, "tlsv1.2", nil)
  end

  def test_tls_psk_set
    client = Mosquitto::Client.new
    assert_raises TypeError do
      client.tls_psk_set("deadbeef", :invalid, nil)
    end
    assert client.tls_psk_set("deadbeef", "psk-id", nil)
  end
end