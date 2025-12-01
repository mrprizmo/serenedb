////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/loop.h>
#include <fuerte/types.h>

#include <memory>

#include "basics/assert.h"

#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace sdb::fuerte {

EventLoopService::EventLoopService(unsigned int thread_count, const char* name)
  : _last_used(0) {
  _io_contexts.reserve(thread_count);
  _guards.reserve(thread_count);
  _threads.reserve(thread_count);
  for (unsigned i = 0; i < thread_count; i++) {
    _io_contexts.emplace_back(std::make_shared<asio_ns::io_context>(1));
    _guards.emplace_back(asio_ns::make_work_guard(*_io_contexts.back()));
    asio_ns::io_context* ctx = _io_contexts.back().get();
    _threads.emplace_back([=]() {
#ifdef __linux__
      // set name of threadpool thread, so threads can be distinguished from
      // each other
      if (name != nullptr && *name != '\0') {
        prctl(PR_SET_NAME, name, 0, 0, 0);
      }
#endif
      ctx->run();
    });
  }
}

EventLoopService::~EventLoopService() { stop(); }

// io_context returns a reference to the boost io_context.
std::shared_ptr<asio_ns::io_context>& EventLoopService::nextIOContext() {
  SDB_ASSERT(!_io_contexts.empty());
  return _io_contexts[_last_used.fetch_add(1, std::memory_order_relaxed) %
                      _io_contexts.size()];
}

asio_ns::ssl::context& EventLoopService::sslContext() {
  std::lock_guard guard(_ssl_context_mutex);
  if (!_ssl_context) {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    _ssl_context =
      std::make_unique<asio_ns::ssl::context>(asio_ns::ssl::context::tls);
#else
    _ssl_context =
      std::make_unique<asio_ns::ssl::context>(asio_ns::ssl::context::sslv23);
#endif
    _ssl_context->set_default_verify_paths();
  }
  return *_ssl_context;
}

void EventLoopService::stop() {
  // allow run() to exit, wait for threads to finish only then stop the context
  std::for_each(_guards.begin(), _guards.end(), [](auto& g) { g.reset(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::for_each(_io_contexts.begin(), _io_contexts.end(),
                [](auto& c) { c->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::for_each(_threads.begin(), _threads.end(), [](auto& t) {
    if (t.joinable()) {
      t.join();
    }
  });
  _threads.clear();
  _io_contexts.clear();
  _guards.clear();
}

}  // namespace sdb::fuerte
