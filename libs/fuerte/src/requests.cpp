////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/requests.h>

namespace sdb::fuerte {

std::unique_ptr<Request> CreateRequest(RestVerb verb,
                                       ContentType content_type) {
  auto request = std::make_unique<Request>();
  request->header.rest_verb = verb;
  request->header.contentType(content_type);
  request->header.acceptType(content_type);
  return request;
}

// For User
std::unique_ptr<Request> CreateRequest(RestVerb verb, const std::string& path,
                                       const StringMap& parameters,
                                       vpack::BufferUInt8 payload) {
  auto request = CreateRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  request->addVPack(std::move(payload));
  return request;
}

std::unique_ptr<Request> CreateRequest(RestVerb verb, const std::string& path,
                                       const StringMap& parameters,
                                       const vpack::Slice payload) {
  auto request = CreateRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  request->addVPack(payload);
  return request;
}

std::unique_ptr<Request> CreateRequest(RestVerb verb, const std::string& path,
                                       const StringMap& parameters) {
  auto request = CreateRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  return request;
}

}  // namespace sdb::fuerte
