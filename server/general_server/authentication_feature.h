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

#include "auth/role_utils.h"
#include "auth/token_cache.h"
#include "rest_server/serened.h"

namespace sdb {

class AuthenticationFeature final : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Authentication"; }

  explicit AuthenticationFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() final;
  void start() final;
  void unprepare() final;

  static AuthenticationFeature* instance() noexcept;

  bool isActive() const noexcept;

  bool authenticationUnixSockets() const noexcept;
  bool authenticationSystemOnly() const noexcept;

  /// @return Cache to deal with authentication tokens
  auth::TokenCache& tokenCache() const noexcept;

  bool hasUserdefinedJwt() const;

  /// verification only secrets
  std::vector<std::string> jwtSecrets() const;

  double sessionTimeout() const { return _session_timeout; }

  // load secrets from file(s)
  Result loadJwtSecretsFromFile();

 private:
  /// load JWT secret from file specified at startup
  Result LoadJwtSecretKeyfile();

  /// load JWT secrets from folder
  Result LoadJwtSecretFolder();

  static constexpr size_t kMaxSecretLength = 64;

  std::unique_ptr<auth::TokenCache> _auth_cache;
  bool _authentication_unix_sockets = true;
  bool _authentication_system_only = true;
  bool _active = true;
  double _authentication_timeout = 0.0;
  double _session_timeout;

  mutable absl::Mutex _jwt_secrets_lock;

  std::vector<std::string> _jwt_secrets;
  std::string _jwt_secret_keyfile;
  std::string _jwt_secret_folder;

  inline static std::atomic<AuthenticationFeature*> gInstance = nullptr;
};

}  // namespace sdb
