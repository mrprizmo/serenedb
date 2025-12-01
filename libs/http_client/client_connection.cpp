////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>

#include <string>

#include "basics/common.h"
#include "basics/operating-system.h"

#ifdef SERENEDB_HAVE_WINSOCK2_H
#include <WS2tcpip.h>
#include <WinSock2.h>
#endif

#include <absl/cleanup/cleanup.h>

#include "basics/debugging.h"
#include "basics/error.h"
#include "basics/errors.h"
#include "basics/socket-utils.h"
#include "basics/string_buffer.h"
#include "client_connection.h"
#include "endpoint/endpoint.h"

namespace sdb::httpclient {

ClientConnection::ClientConnection(app::CommunicationFeaturePhase& comm,
                                   std::unique_ptr<Endpoint> endpoint,
                                   double request_timeout,
                                   double connect_timeout,
                                   size_t connect_retries)
  : GeneralClientConnection{comm, std::move(endpoint), request_timeout,
                            connect_timeout, connect_retries} {}

ClientConnection::~ClientConnection() { disconnect(); }

bool ClientConnection::connectSocket() {
  SDB_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    _endpoint->disconnect();
    _is_connected = false;
  }

  _error_details.clear();
  _socket = _endpoint->connect(_connect_timeout, _request_timeout);

  if (!Sdbisvalidsocket(_socket)) {
    _error_details = _endpoint->error_message;
    _is_connected = false;
    return false;
  }

  _is_connected = true;

  // note: checkSocket will disconnect the socket if the check fails
  if (checkSocket()) {
    return _endpoint->isConnected();
  }

  return false;
}

void ClientConnection::disconnectSocket() {
  if (_endpoint) {
    _endpoint->disconnect();
  }

  Sdbinvalidatesocket(&_socket);
}

bool ClientConnection::writeClientConnection(const void* buffer, size_t length,
                                             size_t* bytes_written) {
  SDB_ASSERT(bytes_written != nullptr);

  if (!checkSocket()) {
    return false;
  }

  long status = Sdbsend(_socket, buffer, length, MSG_NOSIGNAL);

  if (status < 0) {
    SetError(ERROR_SYS_ERROR);
    disconnect();
    return false;
  } else if (status == 0) {
    disconnect();
    return false;
  }

#ifdef SDB_DEV
  _written += (uint64_t)status;
#endif
  *bytes_written = (size_t)status;

  return true;
}

bool ClientConnection::readClientConnection(basics::StringBuffer& string_buffer,
                                            bool& connection_closed) {
  if (!checkSocket()) {
    connection_closed = true;
    return false;
  }

  SDB_ASSERT(Sdbisvalidsocket(_socket));

  connection_closed = false;

  auto true_size = string_buffer.size();
  absl::Cleanup guard = [&] { string_buffer.erase(true_size); };
  do {
    string_buffer.resizeAdditional(true_size, kReadbufferSize);
    auto len_read = Sdbreadsocket(_socket, string_buffer.data() + true_size,
                                  kReadbufferSize, 0);

    if (len_read == -1) {
      connection_closed = true;
      return false;
    }

    if (len_read == 0) {
      connection_closed = true;
      disconnect();
      return true;
    }
#ifdef SDB_DEV
    _read += static_cast<uint64_t>(len_read);
#endif
    true_size += len_read;
  } while (readable());

  return true;
}

bool ClientConnection::readable() {
  if (prepare(_socket, 0.0, false)) {
    return checkSocket();
  }

  return false;
}

}  // namespace sdb::httpclient
