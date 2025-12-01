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

#include "connection_cache.h"

#include <absl/cleanup/cleanup.h>
#include <absl/strings/str_cat.h>

#include "app/app_server.h"
#include "basics/debugging.h"
#include "basics/down_cast.h"
#include "basics/exceptions.h"
#include "basics/logger/logger.h"
#include "endpoint/endpoint.h"
#include "http_client/ssl_client_connection.h"

namespace sdb::httpclient {

ConnectionLease::~ConnectionLease() {
  if (cache && connection &&
      !prevent_recycling.load(std::memory_order_relaxed)) {
    cache->release(std::move(connection));
  }
}

ConnectionCache::ConnectionCache(app::CommunicationFeaturePhase& comm,
                                 const Options& options)
  : _comm{comm}, _options{options} {}

ConnectionLease ConnectionCache::acquire(std::string endpoint,
                                         double connect_timeout,
                                         double request_timeout,
                                         size_t connect_retries,
                                         uint64_t ssl_protocol) {
  SDB_ASSERT(!endpoint.empty());

  // we must unify the endpoint here, because when we return the connection, we
  // will only have the unified form available
  endpoint = Endpoint::unifiedForm(endpoint);

  SDB_TRACE("xxxxx", Logger::REPLICATION,
            "trying to find connection for endpoint ", endpoint,
            " in connections cache");

  std::unique_ptr<GeneralClientConnection> connection;
  uint64_t metric = 0;

  auto try_get_connection_from_cache = [&] {
    std::lock_guard locker{_lock};

    absl::Cleanup account = [&] {
      if (connection) {
        metric = ++_connections_recycled;
      } else {
        metric = ++_connections_created;
      }
    };

    auto it = _connections.find(endpoint);
    if (it == _connections.end()) {
      return;
    }

    auto& connections_for_endpoint = it->second;
    for (auto it = connections_for_endpoint.rbegin();
         it != connections_for_endpoint.rend();) {
      auto& candidate = it->connection;
      SDB_ASSERT(candidate);
      const auto* ep = candidate->GetEndpoint();
      SDB_ASSERT(ep);
      if (ep->encryption() != Endpoint::EncryptionType::None &&
          basics::downCast<SslClientConnection>(*candidate).sslProtocol() !=
            ssl_protocol) {
        ++it;
        continue;
      }
      SDB_ASSERT(ep->specification() == endpoint);

      const auto age = std::chrono::steady_clock::now() - it->last_used;
      if (age < std::chrono::seconds{_options.idle_connection_timeout_sec}) {
        connection = std::move(candidate);
      }

      *it = std::move(connections_for_endpoint.back());
      ++it;
      connections_for_endpoint.pop_back();
      if (connection) {
        // TODO(mbkkt) fast-path time should be choosen more meaningfully
        if (age < std::chrono::seconds{3} || connection->isIdleConnection()) {
          return;
        }
        connection.reset();
        SDB_DEBUG("xxxxx", Logger::COMMUNICATION, "Connection for endpoint ",
                  endpoint, " is invalid, closing it");
      }
    }
  };

  try_get_connection_from_cache();

  if (!connection) {
    SDB_TRACE("xxxxx", Logger::REPLICATION,
              "did not find connection for endpoint ", endpoint,
              " in connections cache. creating new connection... created "
              "connections: ",
              metric);
    auto ep = Endpoint::clientFactory(endpoint);
    if (!ep) {
      SDB_THROW(ERROR_BAD_PARAMETER,
                absl::StrCat("unable to create endpoint: ", endpoint));
    }
    connection = GeneralClientConnection::factory(
      _comm, std::move(ep), request_timeout, connect_timeout, connect_retries,
      ssl_protocol);
    SDB_ASSERT(connection);
  } else {
    connection->repurpose(connect_timeout, request_timeout, connect_retries);
    SDB_TRACE("xxxxx", Logger::REPLICATION, "found connection for endpoint ",
              endpoint,
              " in connections cache. recycled connections: ", metric);
  }

  return {this, std::move(connection)};
}

void ConnectionCache::release(
  std::unique_ptr<GeneralClientConnection> connection, bool force) try {
  if (!connection) {
    return;
  }

  if (connection->isConnected() || force) {
    const auto endpoint = connection->GetEndpoint()->specification();
    SDB_ASSERT(!endpoint.empty());

    SDB_TRACE("xxxxx", Logger::REPLICATION, "putting connection for endpoint ",
              endpoint, " back into connections cache");

    std::lock_guard locker{_lock};
    auto& connections_for_endpoint = _connections[endpoint];
    if (connections_for_endpoint.size() <
        _options.max_connections_per_endpoint) {
      connections_for_endpoint.emplace_back(std::move(connection),
                                            std::chrono::steady_clock::now());
    }
  }
} catch (...) {
}

}  // namespace sdb::httpclient
