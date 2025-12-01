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

#include "cluster_utils.h"

#include "basics/logger/logger.h"
#include "network/connection_pool.h"
#include "network/network_feature.h"
#include "network/utils.h"

namespace sdb {
namespace network {

/// Create Cluster Communication result for insert
OperationResult ClusterResultInsert(sdb::fuerte::StatusCode code,
                                    std::shared_ptr<vpack::BufferUInt8> body,
                                    OperationOptions options,
                                    ErrorsCount error_counter) {
  switch (code) {
    case fuerte::kStatusAccepted:
      return OperationResult{Result{}, std::move(body), std::move(options),
                             std::move(error_counter)};
    case fuerte::kStatusCreated: {
      options.wait_for_sync = true;  // wait for sync is abused here
      // operationResult should get a return code.
      return OperationResult{Result{}, std::move(body), std::move(options),
                             std::move(error_counter)};
    }
    case fuerte::kStatusPreconditionFailed:
      return network::OpResultFromBody(std::move(body), ERROR_SERVER_CONFLICT,
                                       std::move(options));
    case fuerte::kStatusBadRequest:
      return network::OpResultFromBody(std::move(body), ERROR_INTERNAL,
                                       std::move(options));
    case fuerte::kStatusNotFound:
      return network::OpResultFromBody(std::move(body),
                                       ERROR_SERVER_DATA_SOURCE_NOT_FOUND,
                                       std::move(options));
    case fuerte::kStatusConflict:
      return network::OpResultFromBody(std::move(body),
                                       ERROR_SERVER_UNIQUE_CONSTRAINT_VIOLATED,
                                       std::move(options));
    default:
      return network::OpResultFromBody(std::move(body), ERROR_INTERNAL,
                                       std::move(options));
  }
}

/// Create Cluster Communication result for document
OperationResult ClusterResultDocument(sdb::fuerte::StatusCode code,
                                      std::shared_ptr<vpack::BufferUInt8> body,
                                      OperationOptions options,
                                      ErrorsCount error_counter) {
  switch (code) {
    case fuerte::kStatusOk:
      return OperationResult{Result{}, std::move(body), std::move(options),
                             std::move(error_counter)};
    case fuerte::kStatusConflict:
    case fuerte::kStatusPreconditionFailed:
      return OperationResult{Result(ERROR_SERVER_CONFLICT), std::move(body),
                             std::move(options), std::move(error_counter)};
    case fuerte::kStatusNotFound:
      return network::OpResultFromBody(
        std::move(body), ERROR_SERVER_DOCUMENT_NOT_FOUND, std::move(options));
    default:
      return network::OpResultFromBody(std::move(body), ERROR_INTERNAL,
                                       std::move(options));
  }
}

/// Create Cluster Communication result for modify
OperationResult ClusterResultModify(sdb::fuerte::StatusCode code,
                                    std::shared_ptr<vpack::BufferUInt8> body,
                                    OperationOptions options,
                                    ErrorsCount error_counter) {
  switch (code) {
    case fuerte::kStatusAccepted:
    case fuerte::kStatusCreated: {
      options.wait_for_sync = (code == fuerte::kStatusCreated);
      return OperationResult{Result{}, std::move(body), std::move(options),
                             std::move(error_counter)};
    }
    case fuerte::kStatusConflict:
      return OperationResult{
        network::ResultFromBody(body, ERROR_SERVER_UNIQUE_CONSTRAINT_VIOLATED),
        body, std::move(options), std::move(error_counter)};
    case fuerte::kStatusPreconditionFailed:
      return OperationResult{
        network::ResultFromBody(body, ERROR_SERVER_CONFLICT), body,
        std::move(options), std::move(error_counter)};
    case fuerte::kStatusNotFound:
      return network::OpResultFromBody(
        std::move(body), ERROR_SERVER_DOCUMENT_NOT_FOUND, std::move(options));
    default: {
      return network::OpResultFromBody(std::move(body), ERROR_INTERNAL,
                                       std::move(options));
    }
  }
}

/// Create Cluster Communication result for remove
OperationResult ClusterResultRemove(sdb::fuerte::StatusCode code,
                                    std::shared_ptr<vpack::BufferUInt8> body,
                                    OperationOptions options,
                                    ErrorsCount error_counter) {
  switch (code) {
    case fuerte::kStatusOk:
    case fuerte::kStatusAccepted:
    case fuerte::kStatusCreated: {
      options.wait_for_sync = (code != fuerte::kStatusAccepted);
      return OperationResult{Result{}, std::move(body), std::move(options),
                             std::move(error_counter)};
    }
    case fuerte::kStatusConflict:
    case fuerte::kStatusPreconditionFailed:
      return OperationResult{
        network::ResultFromBody(body, ERROR_SERVER_CONFLICT), body,
        std::move(options), std::move(error_counter)};
    case fuerte::kStatusNotFound:
      return network::OpResultFromBody(
        std::move(body), ERROR_SERVER_DOCUMENT_NOT_FOUND, std::move(options));
    default: {
      return network::OpResultFromBody(std::move(body), ERROR_INTERNAL,
                                       std::move(options));
    }
  }
}

}  // namespace network
}  // namespace sdb
