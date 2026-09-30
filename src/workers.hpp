#pragma once
#include <QThreadPool>
#include <cstddef>
#include <functional>
// One caller at a time from a coordinator outside this pool.
class QtWorkerPool {
public:
  QtWorkerPool();
  void run(size_t workers, const std::function<void()> &work);

private:
  QThreadPool pool;
};
