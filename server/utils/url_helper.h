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

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sdb {
namespace url {

// TODO Add string validation

class Scheme {
 public:
  explicit Scheme(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class User {
 public:
  explicit User(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class Password {
 public:
  explicit Password(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class UserInfo {
 public:
  UserInfo(User, Password);
  explicit UserInfo(User);

  const User& user() const noexcept;
  const std::optional<Password>& password() const noexcept;

 private:
  User _user;
  std::optional<Password> _password;
};

class Host {
 public:
  explicit Host(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class Port {
 public:
  explicit Port(uint16_t);
  const uint16_t& value() const noexcept;

 private:
  uint16_t _value;
};

class Authority {
 public:
  Authority(std::optional<UserInfo> user_info, Host host,
            std::optional<Port> port);

  const std::optional<UserInfo>& userInfo() const noexcept;
  const Host& host() const noexcept;
  const std::optional<Port>& port() const noexcept;

 private:
  std::optional<UserInfo> _user_info;
  Host _host;
  std::optional<Port> _port;
};

class Path {
 public:
  explicit Path(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class QueryString {
 public:
  explicit QueryString(std::string);
  const std::string& value() const noexcept;

 private:
  std::string _value;
};

// TODO Add a QueryParameterMap as an option?
//      This should maybe support arrays (e.g. foo[]=bar)?
class QueryParameters {
 public:
  // Keys and values will be url-encoded as necessary
  void add(const std::string& key, const std::string& value);

  bool empty() const noexcept;

  std::ostream& toStream(std::ostream&) const;

 private:
  std::vector<std::pair<std::string, std::string>> _pairs;
};

class Query {
 public:
  explicit Query(QueryString);
  explicit Query(QueryParameters);

  bool empty() const noexcept;

  std::ostream& toStream(std::ostream&) const;

 private:
  // Optionally use either a string, or a vector of pairs as representation
  std::variant<QueryString, QueryParameters> _content;
};

class Fragment {
 public:
  explicit Fragment(std::string);

  const std::string& value() const noexcept;

 private:
  std::string _value;
};

class Url {
 public:
  Url(Scheme, std::optional<Authority>, Path, std::optional<Query>,
      std::optional<Fragment>);

  std::string toString() const;

  const Scheme& scheme() const noexcept;
  const std::optional<Authority>& authority() const noexcept;
  const Path& path() const noexcept;
  const std::optional<Query>& query() const noexcept;
  const std::optional<Fragment>& fragment() const noexcept;

 private:
  Scheme _scheme;
  std::optional<Authority> _authority;
  Path _path;
  std::optional<Query> _query;
  std::optional<Fragment> _fragment;
};

// Artificial part of an URI, including path and optionally query and fragment,
// but omitting scheme and authority.
class Location {
 public:
  Location(Path, std::optional<Query>, std::optional<Fragment>);

  std::string toString() const;

  const Path& path() const noexcept;
  const std::optional<Query>& query() const noexcept;
  const std::optional<Fragment>& fragment() const noexcept;

 private:
  Path _path;
  std::optional<Query> _query;
  std::optional<Fragment> _fragment;
};

std::string UriEncode(const std::string&);
bool IsUnreserved(char);
bool IsReserved(char);

std::ostream& operator<<(std::ostream&, const Authority&);
std::ostream& operator<<(std::ostream&, const Query&);
std::ostream& operator<<(std::ostream&, const QueryParameters&);
std::ostream& operator<<(std::ostream&, const Location&);
std::ostream& operator<<(std::ostream&, const Url&);
std::ostream& operator<<(std::ostream&, const UserInfo&);

}  // namespace url
}  // namespace sdb
