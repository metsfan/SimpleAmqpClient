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

// Put these first to avoid warnings about INT#_C macro redefinition
#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#ifdef SAC_SSL_SUPPORT_ENABLED
#include <amqp_ssl_socket.h>

#endif

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "SimpleAmqpClient/AmqpResponseLibraryException.h"
#include "SimpleAmqpClient/AmqpException.h"
#include "SimpleAmqpClient/AmqpLibraryException.h"
#include "SimpleAmqpClient/BadUriException.h"
#include "SimpleAmqpClient/Connection.h"

namespace AmqpClient
{
Connection::ptr_t Connection::CreateFromUri(const std::string &uri, int frame_max) {
  amqp_connection_info info;
  amqp_default_connection_info(&info);

  boost::shared_ptr<char> uri_dup =
          boost::shared_ptr<char>(strdup(uri.c_str()), free);

  if (0 != amqp_parse_url(uri_dup.get(), &info)) {
    throw BadUriException();
  }

  return Create(std::string(info.host), info.port, std::string(info.user),
                std::string(info.password), std::string(info.vhost), frame_max);
}

Connection::ptr_t Connection::CreateSecureFromUri(
        const std::string &uri, const std::string &path_to_ca_cert,
        const std::string &path_to_client_key,
        const std::string &path_to_client_cert, bool verify_hostname,
        int frame_max) {
  amqp_connection_info info;
  amqp_default_connection_info(&info);

  boost::shared_ptr<char> uri_dup =
          boost::shared_ptr<char>(strdup(uri.c_str()), free);

  if (0 != amqp_parse_url(uri_dup.get(), &info)) {
    throw BadUriException();
  }

  if (info.ssl) {
    return CreateSecure(path_to_ca_cert, std::string(info.host),
                        path_to_client_key, path_to_client_cert, info.port,
                        std::string(info.user), std::string(info.password),
                        std::string(info.vhost), frame_max, verify_hostname);
  }
  throw std::runtime_error(
          "CreateSecureFromUri only supports SSL-enabled URIs.");
}

Connection::Connection(const std::string &host, int port, const std::string &username,
                 const std::string &password, const std::string &vhost,
                 int frame_max) {
  m_connection = amqp_new_connection();
  m_is_connected = false;

  if (NULL == m_connection) {
    throw std::bad_alloc();
  }

  try {
    amqp_socket_t *socket = amqp_tcp_socket_new(m_connection);
    int sock = amqp_socket_open(socket, host.c_str(), port);
    this->CheckForError(sock);

    DoLogin(username, password, vhost, frame_max);
  } catch (...) {
    amqp_destroy_connection(m_connection);
    throw;
  }

  SetIsConnected(true);
}

#ifdef SAC_SSL_SUPPORT_ENABLED
Connection::Connection(const std::string &host, int port, const std::string &username,
                 const std::string &password, const std::string &vhost,
                 int frame_max, const SSLConnectionParams &ssl_params) {
  m_connection = amqp_new_connection();
  m_is_connected = false;

  if (NULL == m_connection) {
    throw std::bad_alloc();
  }

  amqp_socket_t *socket = amqp_ssl_socket_new(m_connection);
  if (NULL == socket) {
    throw std::bad_alloc();
  }
#if AMQP_VERSION >= 0x00080001
  amqp_ssl_socket_set_verify_peer(socket, ssl_params.verify_hostname);
  amqp_ssl_socket_set_verify_hostname(socket, ssl_params.verify_hostname);
#else
  amqp_ssl_socket_set_verify(socket, ssl_params.verify_hostname);
#endif

  try {
    int status =
            amqp_ssl_socket_set_cacert(socket, ssl_params.path_to_ca_cert.c_str());
    if (status) {
      throw AmqpLibraryException::CreateException(
              status, "Error setting CA certificate for socket");
    }

    if (ssl_params.path_to_client_key != "" &&
        ssl_params.path_to_client_cert != "") {
      status = amqp_ssl_socket_set_key(socket,
                                       ssl_params.path_to_client_cert.c_str(),
                                       ssl_params.path_to_client_key.c_str());
      if (status) {
        throw AmqpLibraryException::CreateException(
                status, "Error setting client certificate for socket");
      }
    }

    status = amqp_socket_open(socket, host.c_str(), port);
    if (status) {
      throw AmqpLibraryException::CreateException(
              status, "Error setting client certificate for socket");
    }

    DoLogin(username, password, vhost, frame_max);
  } catch (...) {
    amqp_destroy_connection(m_connection);
    throw;
  }

  SetIsConnected(true);
}
#else
Connection::Connection(const std::string &, int, const std::string &,
                 const std::string &, const std::string &, int,
                 const SSLConnectionParams &) {
  throw std::logic_error(
      "SSL support has not been compiled into SimpleAmqpClient");
}
#endif

Connection::~Connection() {
  amqp_connection_close(m_connection, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(m_connection);
}

void Connection::CheckForError(int ret) {
  if (ret < 0) {
    throw AmqpLibraryException::CreateException(ret);
  }
}

#define BROKER_HEARTBEAT 0

void Connection::DoLogin(const std::string &username,
                          const std::string &password, const std::string &vhost,
                          int frame_max) {
  amqp_table_entry_t capabilties[1];
  amqp_table_entry_t capability_entry;
  amqp_table_t client_properties;

  capabilties[0].key = amqp_cstring_bytes("consumer_cancel_notify");
  capabilties[0].value.kind = AMQP_FIELD_KIND_BOOLEAN;
  capabilties[0].value.value.boolean = 1;

  capability_entry.key = amqp_cstring_bytes("capabilities");
  capability_entry.value.kind = AMQP_FIELD_KIND_TABLE;
  capability_entry.value.value.table.num_entries =
          sizeof(capabilties) / sizeof(amqp_table_entry_t);
  capability_entry.value.value.table.entries = capabilties;

  client_properties.num_entries = 1;
  client_properties.entries = &capability_entry;

  CheckRpcReply(amqp_login_with_properties(m_connection, vhost.c_str(), 0, frame_max,
                                        BROKER_HEARTBEAT, &client_properties,
                                        AMQP_SASL_METHOD_PLAIN, username.c_str(),
                                        password.c_str()));

  m_brokerVersion = ComputeBrokerVersion(m_connection);
}

void Connection::CheckRpcReply(const amqp_rpc_reply_t &reply) {
  switch (reply.reply_type) {
    case AMQP_RESPONSE_NORMAL:
      return;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
      // If we're getting this likely is the socket is already closed
      throw AmqpResponseLibraryException::CreateException(reply, "");

    case AMQP_RESPONSE_SERVER_EXCEPTION:
    default:
      AmqpException::Throw(reply);
  }
}

namespace {
bool bytesEqual(amqp_bytes_t r, amqp_bytes_t l) {
  if (r.len == l.len) {
    if (0 == memcmp(r.bytes, l.bytes, r.len)) {
      return true;
    }
  }
  return false;
}
}

boost::uint32_t Connection::ComputeBrokerVersion(
        amqp_connection_state_t state) {
  const amqp_table_t *properties = amqp_get_server_properties(state);
  const amqp_bytes_t version = amqp_cstring_bytes("version");
  amqp_table_entry_t *version_entry = NULL;

  for (int i = 0; i < properties->num_entries; ++i) {
    if (bytesEqual(properties->entries[i].key, version)) {
      version_entry = &properties->entries[i];
      break;
    }
  }
  if (NULL == version_entry) {
    return 0;
  }

  std::string version_string(
          static_cast<char *>(version_entry->value.value.bytes.bytes),
          version_entry->value.value.bytes.len);
  std::vector<std::string> version_components;
  boost::split(version_components, version_string, boost::is_any_of("."));
  if (version_components.size() != 3) {
    return 0;
  }
  boost::uint32_t version_major =
          boost::lexical_cast<boost::uint32_t>(version_components[0]);
  boost::uint32_t version_minor =
          boost::lexical_cast<boost::uint32_t>(version_components[1]);
  boost::uint32_t version_patch =
          boost::lexical_cast<boost::uint32_t>(version_components[2]);
  return (version_major & 0xFF) << 16 | (version_minor & 0xFF) << 8 |
         (version_patch & 0xFF);
}

}