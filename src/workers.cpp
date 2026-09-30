#include "workers.hpp"
#include <QSemaphore>
#include <exception>
#include <stdexcept>
#include <vector>
QtWorkerPool::QtWorkerPool() {
  pool.setExpiryTimeout(-1);
  pool.setMaxThreadCount(1);
}
void QtWorkerPool::run(size_t workers, const std::function<void()> &work) {
  if (!workers)
    return;
  if (workers > 8)
    throw std::invalid_argument("Analysis worker limit exceeds 8");
  pool.setMaxThreadCount(int(workers));
  QSemaphore completed;
  int submitted = 0;
  std::vector<std::exception_ptr> errors(workers);
  std::exception_ptr submissionFailure;
  try {
    for (size_t i = 0; i < workers; ++i) {
      pool.start([&, i] {
        try {
          work();
        } catch (...) {
          errors[i] = std::current_exception();
        }
        completed.release();
      });
      ++submitted;
    }
  } catch (...) {
    submissionFailure = std::current_exception();
  }
  // QSemaphore prevents waiting callers from executing queued worker tasks.
  // Captures stay alive until every submitted callback is complete.
  completed.acquire(submitted);
  if (submissionFailure)
    std::rethrow_exception(submissionFailure);
  for (const auto &error : errors)
    if (error)
      std::rethrow_exception(error);
}
