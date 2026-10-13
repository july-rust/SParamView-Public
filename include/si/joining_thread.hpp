#pragma once
#include <thread>
#include <utility>

namespace si {
// RAII join also covers partial pool construction when std::thread throws.
// No stop_token is used by the engine; cancellation remains Control-owned.
class JoiningThread {
  std::thread thread_;
public:
  template<class F> explicit JoiningThread(F &&work)
      : thread_(std::forward<F>(work)) {}
  JoiningThread(JoiningThread &&) noexcept = default;
  JoiningThread(const JoiningThread &) = delete;
  JoiningThread &operator=(const JoiningThread &) = delete;
  ~JoiningThread() { if (thread_.joinable()) thread_.join(); }
};
}
