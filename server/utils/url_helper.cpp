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

#include "url_helper.h"

#include <absl/strings/internal/ostringstream.h>

#include <sstream>
#include <utility>

using namespace sdb;
using namespace sdb::url;

// used for URI-encoding
static const char* gHexValuesLower = "0123456789abcdef";

std::ostream& Query::toStream(std::ostream& ostream) const {
  struct Output {
    std::ostream& ostream;

    void operator()(const QueryString& query_string) {
      ostream << query_string.value();
    }
    void operator()(const QueryParameters& query_parameters) {
      ostream << query_parameters;
    }
  };
  std::visit(Output{ostream}, _content);
  return ostream;
}

Query::Query(QueryString query_string) : _content(std::move(query_string)) {}
Query::Query(QueryParameters query_parameters)
  : _content(std::move(query_parameters)) {}

bool Query::empty() const noexcept {
  struct Output {
    bool operator()(const QueryString& query_string) const {
      return query_string.value().empty();
    }
    bool operator()(const QueryParameters& query_parameters) const {
      return query_parameters.empty();
    }
  };
  return std::visit(Output{}, _content);
}

std::ostream& QueryParameters::toStream(std::ostream& ostream) const {
  bool first = true;
  for (const auto& it : _pairs) {
    if (!first) {
      ostream << "&";
    }
    first = false;
    ostream << UriEncode(it.first) << "=" << UriEncode(it.second);
  }

  return ostream;
}

void QueryParameters::add(const std::string& key, const std::string& value) {
  _pairs.emplace_back(key, value);
}

bool QueryParameters::empty() const noexcept { return _pairs.empty(); }

Scheme::Scheme(std::string scheme) : _value(std::move(scheme)) {}

const std::string& Scheme::value() const noexcept { return _value; }

User::User(std::string user) : _value(std::move(user)) {}

const std::string& User::value() const noexcept { return _value; }

Password::Password(std::string password) : _value(std::move(password)) {}

const std::string& Password::value() const noexcept { return _value; }

Host::Host(std::string host) : _value(std::move(host)) {}

const std::string& Host::value() const noexcept { return _value; }

Port::Port(uint16_t port) : _value(port) {}

const uint16_t& Port::value() const noexcept { return _value; }

Authority::Authority(std::optional<UserInfo> user_info, Host host,
                     std::optional<Port> port)
  : _user_info(std::move(user_info)),
    _host(std::move(host)),
    _port(std::move(port)) {}
const std::optional<UserInfo>& Authority::userInfo() const noexcept {
  return _user_info;
}

const Host& Authority::host() const noexcept { return _host; }

const std::optional<Port>& Authority::port() const noexcept { return _port; }

UserInfo::UserInfo(User user, Password password)
  : _user(std::move(user)), _password(std::move(password)) {}

UserInfo::UserInfo(User user)
  : _user(std::move(user)), _password(std::nullopt) {}

const User& UserInfo::user() const noexcept { return _user; }

const std::optional<Password>& UserInfo::password() const noexcept {
  return _password;
}

Path::Path(std::string path) : _value(std::move(path)) {}
const std::string& Path::value() const noexcept { return _value; }

QueryString::QueryString(std::string query_string)
  : _value(std::move(query_string)) {}

const std::string& QueryString::value() const noexcept { return _value; }

Fragment::Fragment(std::string fragment) : _value(std::move(fragment)) {}

const std::string& Fragment::value() const noexcept { return _value; }

std::string Url::toString() const {
  std::string url_str;
  absl::strings_internal::OStringStream{&url_str} << *this;
  return url_str;
}

Url::Url(Scheme scheme, std::optional<Authority> authority, Path path,
         std::optional<Query> query, std::optional<Fragment> fragment)
  : _scheme(std::move(scheme)),
    _authority(std::move(authority)),
    _path(std::move(path)),
    _query(std::move(query)),
    _fragment(std::move(fragment)) {}

const Scheme& Url::scheme() const noexcept { return _scheme; }

const std::optional<Authority>& Url::authority() const noexcept {
  return _authority;
}

const Path& Url::path() const noexcept { return _path; }

const std::optional<Query>& Url::query() const noexcept { return _query; }

const std::optional<Fragment>& Url::fragment() const noexcept {
  return _fragment;
}

Location::Location(Path path, std::optional<Query> query,
                   std::optional<Fragment> fragment)
  : _path(std::move(path)),
    _query(std::move(query)),
    _fragment(std::move(fragment)) {}

std::string Location::toString() const {
  std::string location_str;
  absl::strings_internal::OStringStream{&location_str} << *this;
  return location_str;
}

const Path& Location::path() const noexcept { return _path; }

const std::optional<Query>& Location::query() const noexcept { return _query; }

const std::optional<Fragment>& Location::fragment() const noexcept {
  return _fragment;
}

// unreserved are A-Z, a-z, 0-9 and - _ . ~
bool sdb::url::IsUnreserved(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == '~';
}

// reserved are:
// ! * ' ( ) ; : @ & = + $ , / ? % # [ ]
bool sdb::url::IsReserved(char c) {
  return c == '!' || c == '*' || c == '\'' || c == '(' || c == ')' ||
         c == ';' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' ||
         c == '$' || c == ',' || c == '/' || c == '?' || c == '%' || c == '#' ||
         c == '[' || c == ']';
}

std::string sdb::url::UriEncode(const std::string& raw) {
  std::string encoded;

  for (const auto c : raw) {
    if (IsUnreserved(c)) {
      // append character as is
      encoded.push_back(c);
    } else {
      // must hex-encode the character
      encoded.push_back('%');
      auto u = static_cast<unsigned char>(c);
      encoded.push_back(::gHexValuesLower[u >> 4]);
      encoded.push_back(::gHexValuesLower[u % 16]);
    }
  }

  return encoded;
}

std::ostream& sdb::url::operator<<(std::ostream& ostream,
                                   const Location& location) {
  ostream << location.path().value();

  if (location.query()) {
    ostream << "?" << *location.query();
  }

  if (location.fragment()) {
    ostream << "#" << location.fragment()->value();
  }

  return ostream;
}

std::ostream& sdb::url::operator<<(std::ostream& ostream, const Url& url) {
  ostream << url.scheme().value() << ":";

  if (url.authority()) {
    ostream << "//" << *url.authority();
  }

  ostream << Location{url.path(), url.query(), url.fragment()};

  return ostream;
}

std::ostream& sdb::url::operator<<(std::ostream& ostream,
                                   const Authority& authority) {
  if (authority.userInfo()) {
    ostream << *authority.userInfo() << "@";
  }
  ostream << authority.host().value();
  if (authority.port()) {
    ostream << authority.port()->value();
  }
  return ostream;
}

std::ostream& sdb::url::operator<<(std::ostream& ostream,
                                   const UserInfo& user_info) {
  ostream << user_info.user().value();
  if (user_info.password()) {
    ostream << ":" << user_info.password()->value();
  }
  return ostream;
}

std::ostream& sdb::url::operator<<(std::ostream& ostream, const Query& query) {
  return query.toStream(ostream);
}

std::ostream& sdb::url::operator<<(std::ostream& ostream,
                                   const QueryParameters& query_parameters) {
  return query_parameters.toStream(ostream);
}
