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

#include <vpack/builder.h>
#include <vpack/slice.h>

#include <atomic>
#include <mutex>
#include <string>

#include "basics/common.h"
#include "basics/containers/node_hash_map.h"
#include "basics/debugging.h"
#include "basics/lru_cache.h"
#include "basics/read_write_lock.h"
#include "basics/result.h"
#include "basics/system-functions.h"
#include "general_server/state.h"
#include "rest/common_defines.h"

namespace sdb::auth {

// Caches the basic and JWT authentication tokens
class TokenCache {
 public:
  // timeout default token expiration timeout
  explicit TokenCache(double timeout);
  ~TokenCache();

 public:
  struct Entry {
    friend class auth::TokenCache;

   public:
    explicit Entry(std::string username, bool a, double t)
      : _username(std::move(username)), _expiry(t), _authenticated(a) {}

    static Entry Unauthenticated() { return Entry("", false, 0); }
    static Entry Superuser() { return Entry("", true, 0); }

    const std::string& username() const noexcept { return _username; }
    bool authenticated() const noexcept { return _authenticated; }
    void authenticated(bool value) noexcept { _authenticated = value; }
    void setExpiry(double expiry) noexcept { _expiry = expiry; }
    double expiry() const noexcept { return _expiry; }
    bool expired() const noexcept {
      return _expiry != 0 && _expiry < utilities::GetMicrotime();
    }
    const std::vector<std::string>& allowedPaths() const {
      return _allowed_paths;
    }

   private:
    /// username
    std::string _username;
    // paths that are valid for this token
    std::vector<std::string> _allowed_paths;
    /// expiration time (in seconds since epoch) of this entry
    double _expiry;
    /// User exists and password was checked
    bool _authenticated;
  };

 public:
  TokenCache::Entry checkAuthentication(rest::AuthenticationMethod auth_type,
                                        ServerState::Mode mode,
                                        const std::string& secret);

  /// Clear the cache of username / password auth
  void invalidateBasicCache();

  /// set new jwt secret, regenerate _jwt_token
  void setJwtSecrets(std::vector<std::string> secrets);

  /// Get the jwt token, which should be used for communication
  const std::string& jwtToken() const noexcept {
    SDB_ASSERT(!_jwt_super_token.empty());
    return _jwt_super_token;
  }

  std::string jwtSecret() const;

 private:
  /// Check basic HTTP Authentication header
  TokenCache::Entry checkAuthenticationBasic(const std::string& secret);
  /// Check JWT token contents
  TokenCache::Entry checkAuthenticationJWT(const std::string& secret);

  bool validateJwtHeader(std::string_view header_web_base64);
  TokenCache::Entry validateJwtBody(std::string_view body_web_base64);
  bool validateJwtHMAC256Signature(std::string_view message,
                                   std::string_view signature_web_base64);

  std::shared_ptr<vpack::Builder> parseJson(std::string_view str,
                                            const char* hint);

  /// generate new superuser jwtToken for internal cluster communication
  void generateSuperToken();

 private:
  mutable absl::Mutex _basic_lock;
  containers::NodeHashMap<std::string, TokenCache::Entry> _basic_cache;
  std::atomic<uint64_t> _basic_cache_version{0};

  mutable absl::Mutex _jwt_secret_lock;
  std::vector<std::string> _jwt_secrets;
  std::string _jwt_super_token;  /// token for internal use

  mutable absl::Mutex _jwt_cache_mutex;
  basics::LruCache<std::string, TokenCache::Entry> _jwt_cache;

  /// Timeout in seconds
  const double _auth_timeout;
};

}  // namespace sdb::auth
