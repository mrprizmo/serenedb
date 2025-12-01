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

#include <sys/stat.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "basics/logger/appender.h"
#include "basics/logger/log_level.h"

namespace sdb::log {

class AppenderStream : public Appender {
 public:
  AppenderStream(const std::string& filename, int fd);
  ~AppenderStream() override = default;

  void logMessage(const Message& message) final;

  int fd() const { return _fd; }

 protected:
  void updateFd(int fd) { _fd = fd; }

  virtual void writeLogMessage(LogLevel level, const std::string& message) = 0;

  /// maximum size for reusable log buffer
  /// if the buffer exceeds this size, it will be freed after the log
  /// message was produced. otherwise it will be kept for recycling
  static constexpr size_t kMaxBufferSize = 64 * 1024;

  /// file descriptor
  int _fd;

  /// whether or not we should use colors
  bool _use_colors;
};

class AppenderFile : public AppenderStream {
  friend struct AppenderFileFactory;

 public:
  explicit AppenderFile(const std::string& filename, int fd);
  ~AppenderFile() override = default;

  void writeLogMessage(LogLevel level, const std::string& message) final;

  const std::string& filename() const { return _filename; }

 private:
  const std::string _filename;
};

struct AppenderFileFactory {
  AppenderFileFactory() = delete;

  static std::shared_ptr<AppenderFile> getFileAppender(
    const std::string& filename);

  static void reopenAll();
  static void closeAll();

#ifdef SDB_GTEST
  static std::vector<
    std::tuple<int, std::string, std::shared_ptr<AppenderFile>>> getAppenders();

  static void setAppenders(
    const std::vector<
      std::tuple<int, std::string, std::shared_ptr<AppenderFile>>>& fds);
#endif

  static void setFileMode(int mode) { gFileMode = mode; }
  static void setFileGroup(int group) { gFileGroup = group; }

 private:
  inline static constinit absl::Mutex gOpenAppendersMutex{absl::kConstInit};
  inline static std::vector<std::shared_ptr<AppenderFile>> gOpenAppenders;

  inline static int gFileMode = S_IRUSR | S_IWUSR | S_IRGRP;
  inline static int gFileGroup = 0;
};

class AppenderStdStream : public AppenderStream {
 public:
  AppenderStdStream(const std::string& filename, int fd);
  ~AppenderStdStream() override;

  static void writeLogMessage(int fd, bool use_colors, LogLevel level,
                              const std::string& message);

 private:
  void writeLogMessage(LogLevel level, const std::string& message) final;
};

class AppenderStderr final : public AppenderStdStream {
 public:
  AppenderStderr();
};

class AppenderStdout final : public AppenderStdStream {
 public:
  AppenderStdout();
};

}  // namespace sdb::log
