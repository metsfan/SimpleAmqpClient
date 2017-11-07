#ifndef SIMPLEAMQPCLIENT_CONNECTION_H
#define SIMPLEAMQPCLIENT_CONNECTION_H
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Copyright (c) 2010-2013 Alan Antonuk
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#include <boost/cstdint.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <string>
#include <vector>

#include <amqp.h>

#include "SimpleAmqpClient/Util.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251 4275)
#endif

namespace AmqpClient
{

class SIMPLEAMQPCLIENT_EXPORT Connection : boost::noncopyable {
public:
  typedef boost::shared_ptr<Connection> ptr_t;

  /**
  * Creates a new channel object
  * Creates a new connection to an AMQP broker using the supplied parameters
 * and opens
  * a single channel for use
  * @param host The hostname or IP address of the AMQP broker
  * @param port The port to connect to the AMQP broker on
  * @param username The username used to authenticate with the AMQP broker
  * @param password The password corresponding to the username used to
 * authenticate with the AMQP broker
  * @param vhost The virtual host on the AMQP we should connect to
  * @param channel_max Request that the server limit the number of channels
 * for
  * this connection to the specified parameter, a value of zero will use the
 * broker-supplied value
  * @param frame_max Request that the server limit the maximum size of any
 * frame to this value
  * @return a new Channel object pointer
  */
  static ptr_t Create(const std::string &host = "127.0.0.1", int port = 5672,
                      const std::string &username = "guest",
                      const std::string &password = "guest",
                      const std::string &vhost = "/", int frame_max = 131072) {
      return boost::make_shared<Connection>(host, port, username, password, vhost,
                                            frame_max);
  }

  struct SSLConnectionParams {
    std::string path_to_ca_cert;
    std::string path_to_client_key;
    std::string path_to_client_cert;
    bool verify_hostname;
  };

  /**
  * Creates a new channel object
  * Creates a new connection to an AMQP broker using the supplied parameters and
  * opens
  * a single channel for use
  * @param path_to_ca_cert Path to ca certificate file
  * @param host The hostname or IP address of the AMQP broker
  * @param path_to_client_key Path to client key file
  * @param path_to_client_cert Path to client certificate file
  * @param port The port to connect to the AMQP broker on
  * @param username The username used to authenticate with the AMQP broker
  * @param password The password corresponding to the username used to
  * authenticate with the AMQP broker
  * @param vhost The virtual host on the AMQP we should connect to
  * @param channel_max Request that the server limit the number of channels for
  * this connection to the specified parameter, a value of zero will use the
  * broker-supplied value
  * @param frame_max Request that the server limit the maximum size of any frame
  * to this value
  * @param verify_host Verify the hostname against the certificate when
  * opening the SSL connection.
  *
  * @return a new Channel object pointer
  */

  static ptr_t CreateSecure(const std::string &path_to_ca_cert = "",
                            const std::string &host = "127.0.0.1",
                            const std::string &path_to_client_key = "",
                            const std::string &path_to_client_cert = "",
                            int port = 5671,
                            const std::string &username = "guest",
                            const std::string &password = "guest",
                            const std::string &vhost = "/",
                            int frame_max = 131072,
                            bool verify_hostname = true) {
      SSLConnectionParams ssl_params;
      ssl_params.path_to_ca_cert = path_to_ca_cert;
      ssl_params.path_to_client_key = path_to_client_key;
      ssl_params.path_to_client_cert = path_to_client_cert;
      ssl_params.verify_hostname = verify_hostname;

      return boost::make_shared<Connection>(host, port, username, password, vhost,
                                            frame_max, ssl_params);
  }

  /**
   * Create a new Channel object from an AMQP URI
   *
   * @param uri [in] a URI of the form:
   * amqp://[username:password@]{HOSTNAME}[:PORT][/VHOST]
   * @param frame_max [in] requests that the broker limit the maximum size of
   * any frame to this value
   * @returns a new Channel object
   */
  static ptr_t CreateFromUri(const std::string &uri, int frame_max = 131072);

  /**
   * Create a new Channel object from an AMQP URI, secured with SSL.
   * If URI should start with amqps://
   *
   * @param uri [in] a URI of the form:
   * amqp[s]://[username:password@]{HOSTNAME}[:PORT][/VHOST]
   * @param path_to_ca_cert Path to ca certificate file
   * @param host The hostname or IP address of the AMQP broker
   * @param path_to_client_key Path to client key file
   * @param path_to_client_cert Path to client certificate file
   * @param verify_hostname Verify the hostname against the certificate when
   * opening the SSL connection.
   * @param frame_max [in] requests that the broker limit the maximum size of
   * any frame to this value
   * @returns a new Channel object
   */
  static ptr_t CreateSecureFromUri(const std::string &uri,
                                   const std::string &path_to_ca_cert,
                                   const std::string &path_to_client_key = "",
                                   const std::string &path_to_client_cert = "",
                                   bool verify_hostname = true,
                                   int frame_max = 131072);

  explicit Connection(const std::string &host, int port,
                      const std::string &username, const std::string &password,
                      const std::string &vhost, int frame_max);

  explicit Connection(const std::string &host, int port,
                      const std::string &username, const std::string &password,
                      const std::string &vhost, int frame_max,
                      const SSLConnectionParams &ssl_params);

  virtual ~Connection();

  boost::uint32_t GetBrokerVersion() { return m_brokerVersion; }
  amqp_connection_state_t GetConnectionState() { return m_connection; }

private:
  amqp_connection_state_t m_connection;
  bool m_is_connected;
  boost::uint32_t m_brokerVersion;

  void CheckForError(int ret);
  void DoLogin(const std::string &username, const std::string &password,
               const std::string &vhost, int frame_max);
  void SetIsConnected(bool state) { m_is_connected = state; }
  static boost::uint32_t ComputeBrokerVersion(
          const amqp_connection_state_t state);
  void CheckRpcReply(const amqp_rpc_reply_t &reply);
};

}

#endif