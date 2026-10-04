#include "si/core.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#undef far
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace si {
#include "core_common.cpp"

// These small platform-sensitive primitives remain in the aggregator so the
// established macOS release shim continues to patch the same verified contexts.
static double number(std::string s) {
  for (char &c : s)
    if (c == 'D' || c == 'd')
      c = 'e';
  if (!s.empty() && s[0] == '+')
    s.erase(0, 1);
  double v;
  auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (r.ec != std::errc() || r.ptr != s.data() + s.size() || !std::isfinite(v))
    throw Error("Invalid finite number: " + s);
  return v;
}
static uint64_t fingerprint(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw Error("Cannot read: " + utf8(path));
  auto n = fs::file_size(path);
  uint64_t h = hashBytes(&n, sizeof(n));
  std::array<char, 65536> b;
  for (uint64_t off :
       {uint64_t(0), n / 2, n > 65536 ? n - 65536 : uint64_t(0)}) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(off));
    if (!in)
      throw Error("Fingerprint seek failed");
    in.read(b.data(), b.size());
    if (in.bad())
      throw Error("Fingerprint read failed");
    h = hashBytes(b.data(), static_cast<size_t>(in.gcount()), h);
  }
  return h;
}

#include "touchstone.cpp"
#include "cache.cpp"
#include "mapping.cpp"
#include "termination.cpp"
#include "trace.cpp"
#include "tdr.cpp"
#include "analysis.cpp"
#include "jobs.cpp"

std::vector<Result> runJobs(const std::vector<Job> &jobs,
                            const Settings &settings, Control *control,
                            const WorkerRunner &runner) {
  check(control);
  if (jobs.empty())
    return {};
  std::vector<Result> out(jobs.size());
  std::atomic_size_t next{0}, done{0};
  std::mutex progressMutex;
  size_t threads = workerLimit(settings.performance, jobs.size(),
                               std::thread::hardware_concurrency());
  auto work = [&]() {
    while (true) {
      size_t i = next.fetch_add(1);
      if (i >= jobs.size())
        return;
      if (control && control->cancelled)
        return;
      auto &j = jobs[i];
      Settings jobSettings = settings;
      if (j.reverse) jobSettings.reverse = *j.reverse;
      Result r;
      try {
        if (!j.cache)
          throw Error("Job has no cache");
        auto t = loadTrace(*j.cache, j.victim, j.metric, jobSettings,
                           j.aggressor ? &*j.aggressor : nullptr, control, j.termination);
        r = analyze(t, j.metric, jobSettings, control);
      } catch (const Cancelled &) {
        return;
      } catch (const Error &e) {
        r.metric = j.metric;
        r.status = "N/A";
        r.note = e.what();
        r.termination = j.termination;
        r.reflection = j.metric == Metric::TDR && jobSettings.tdrReflection;
        if(j.metric==Metric::TDR) {
          r.quality="UNSUITABLE";
          r.parameter=std::string(j.victim.differential()?"Sdd":"S")+(jobSettings.reverse?"22":"11");
        }
      }
      r.channel = j.victim.name;
      r.channelId = j.victim.id;
      r.group = j.victim.group;
      r.revision = j.revision;
      r.reverse = jobSettings.reverse;
      r.direction = metricDirection(j.metric, jobSettings.reverse);
      if (j.aggressor) {
        r.aggressor = j.aggressor->name;
        r.aggressorId = j.aggressor->id;
      }
      if (jobs.size() > 128) {
        // Retain an immediate preview under a shared ~32 MiB point budget.
        size_t buckets = std::clamp<size_t>((32 * 1024 * 1024 / jobs.size()) / (4 * sizeof(double)), 8, 128);
        auto indices = envelope(r.plotX, r.plotY, buckets, r.start, r.stop);
        std::vector<double> x, y;
        for (auto k : indices) { x.push_back(r.plotX[k]); y.push_back(r.plotY[k]); }
        r.plotComplete = r.plotComplete && indices.size() == r.plotX.size();
        r.plotX = std::move(x); r.plotY = std::move(y);
      }
      if (jobs.size() > 512) {
        r.heatX.clear();
        r.heatRaw.clear();
        r.heatMargin.clear();
      }
      out[i] = std::move(r);
      {
        std::lock_guard lock(progressMutex);
        ++done;
        if (control && control->progress && !control->cancelled)
          control->progress(double(done) / std::max<size_t>(1, jobs.size()),
                            "Analyzing " + std::to_string(done) + " / " +
                                std::to_string(jobs.size()));
      }
      if (settings.performance == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  };
  std::exception_ptr failure;
  std::mutex failureMutex;
  auto guarded = [&] {
    try {
      work();
    } catch (...) {
      std::lock_guard lock(failureMutex);
      if (!failure)
        failure = std::current_exception();
    }
  };
  if (runner)
    runner(threads, guarded);
  else {
    std::vector<std::jthread> pool;
    for (size_t i = 0; i < threads; ++i)
      pool.emplace_back(guarded);
  }
  if (failure)
    std::rethrow_exception(failure);
  check(control);
  return out;
}

#include "export.cpp"
} // namespace si
