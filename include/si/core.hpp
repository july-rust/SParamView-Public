#pragma once
#include <algorithm>
#include <atomic>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace si {
using Complex = std::complex<double>;
namespace fs = std::filesystem;
inline constexpr const char *version = "1.2.1";
inline constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Cancelled : Error {
  Cancelled() : Error("Cancelled") {}
};
struct Control {
  std::atomic_bool cancelled{false};
  std::function<void(double, const std::string &)> progress;
  void check() const {
    if (cancelled.load())
      throw Cancelled();
  }
  void update(double fraction, const std::string &text) const;
};
std::string utf8(const fs::path &path);
fs::path pathFromUtf8(const std::string &text);
std::string sha256File(const fs::path &, Control * = nullptr);
struct Metadata {
  std::string source, version = "1.0", format = "MA", matrix = "Full",
                      order = "21_12", sha256;
  uint64_t ports = 0, points = 0, sourceSize = 0, fingerprint = 0;
  int64_t modified = 0;
  std::vector<double> reference;
  std::vector<std::string> labels, mixedOrder, warnings, differentialHints;
};
class CacheFile;
class Cache {
public:
  static std::shared_ptr<Cache> open(const fs::path &source,
                                     const fs::path &cacheDirectory,
                                     Control *control = nullptr,
                                     bool fullVerify = false);
  const Metadata &meta() const { return metadata_; }
  const fs::path &path() const { return path_; }
  bool reused() const { return reused_; }
  std::vector<double> frequencies(Control * = nullptr) const;
  // response/stimulus weights refer to physical single-ended ports
  // (zero-based).
  std::vector<Complex>
  trace(const std::vector<std::pair<int, double>> &response,
        const std::vector<std::pair<int, double>> &stimulus,
        Control * = nullptr) const;

private:
  std::shared_ptr<CacheFile> file_; // Keep the opened cache generation alive.
  fs::path path_;
  Metadata metadata_;
  uint64_t tileFrames_ = 0, blockBytes_ = 0, dataOffset_ = 512;
  bool reused_ = false;
};
struct Channel {
  std::string id, name, alias, group = "Default";
  int nearP = -1, nearN = -1, farP = -1, farN = -1;
  bool differential() const { return nearN >= 0; }
};
void validateMapping(const std::vector<Channel> &, const Metadata &);
std::vector<Channel> suggestMapping(const Metadata &,
                                    std::string *diagnostic = nullptr);
std::vector<Channel> readMappingCsv(const fs::path &);
void writeMappingCsv(const fs::path &, const std::vector<Channel> &);
std::vector<int> matchChannels(const std::vector<Channel> &baseline,
                               const std::vector<Channel> &other);
enum class Metric { RL, IL, NEXT, FEXT, TDR };
std::string name(Metric);
enum class Termination { Reference, Resistor, Split, Open, Short };
std::string name(Termination, bool differential);
std::string shortName(Termination, bool differential);
struct Limit {
  bool enabled = false;
  double constant = 0;
  std::vector<std::pair<double, double>> points; // Hz, dB; empty = constant
  double at(double frequency) const;
  void validate() const;
};
struct Settings {
  double startHz = 0, stopHz = std::numeric_limits<double>::infinity();
  bool reverse = false;
  int markers = 3;
  double prominence = 0.15;
  std::vector<double> manualMarkersHz;
  Limit rl, il, next, fext;
  double targetOhm =
      0; // 0: automatic 50 SE / 100 differential, NEVER changes reference
  bool tdrLimitEnabled = false;
  double tolerancePercent = 10;
  double tdrStartSeconds = 0,
         tdrStopSeconds = std::numeric_limits<double>::infinity();
  double kaiserBeta = 6;
  std::vector<Termination> tdrTerminations{Termination::Resistor};
  bool tdrReflection = false;
  std::vector<Metric> quick = {Metric::RL, Metric::IL, Metric::NEXT,
                               Metric::FEXT, Metric::TDR};
  int performance = 1; // 0 Low, 1 Normal, 2 Maximum
  const Limit &limit(Metric metric) const;
};
struct Trace {
  std::vector<double> x;
  std::vector<Complex> s;
  double referenceOhm = 50;
  double responseReferenceOhm = 50;
  std::string parameter;
  Termination termination = Termination::Reference;
};
struct Peak {
  double x = 0, y = 0;
  bool global = false, manual = false;
  double phaseDegrees = NaN;
};
struct Result {
  Metric metric = Metric::RL;
  Termination termination = Termination::Reference;
  bool reflection = false, reverse = false;
  std::string channel, channelId, group, revision, aggressor, aggressorId,
      parameter, direction;
  std::string status = "N/A", quality, note;
  double worst = NaN, worstX = NaN, margin = NaN, marginX = NaN,
         limitAtMargin = NaN, directionDelta = NaN, directionDeltaX = NaN;
  double minimum = NaN, maximum = NaN, targetOhm = NaN, referenceOhm = NaN;
  double start = NaN, stop = NaN, minErrorPercent = NaN, maxErrorPercent = NaN;
  double transformDf = NaN, transformFmax = NaN;
  std::vector<Peak> peaks;
  // Bounded display buffers; statistics always use full-resolution samples.
  std::vector<double> plotX, plotY, heatX, heatRaw, heatMargin;
  bool plotComplete = false; // All computed samples are cached for navigation.
};
Trace loadTrace(const Cache &, const Channel &victim, Metric, const Settings &,
                const Channel *aggressor = nullptr, Control * = nullptr,
                Termination = Termination::Reference);
struct DirectionDelta {
  double maximumDb = NaN, frequency = NaN;
};
DirectionDelta directionDelta(const Cache &, const Channel &, Metric,
                              const Settings &, Control * = nullptr);
Complex interpolate(const std::vector<double> &, const std::vector<Complex> &,
                    double);
double logMagnitude(Complex);
Trace crop(const Trace &, double start, double stop);
struct Tdr {
  std::vector<double> time, impedance, reflection;
  std::string quality = "GOOD", note;
  double df = 0, fmax = 0;
};
Tdr transformTdr(const Trace &, const Settings &, Control * = nullptr);
struct TdrCacheStats { size_t hits, misses, bytes, entries; };
TdrCacheStats tdrCacheStats();
// Clear only when no transform workers are running (tests/session cleanup).
void clearTdrCache();
std::vector<size_t> envelope(const std::vector<double> &x,
                             const std::vector<double> &y, size_t buckets,
                             double lo, double hi);
Result analyze(const Trace &, Metric, const Settings &, Control * = nullptr);
struct Job {
  std::shared_ptr<Cache> cache;
  Channel victim;
  std::optional<Channel> aggressor;
  Metric metric = Metric::RL;
  std::string revision;
  Termination termination = Termination::Reference;
  // nullopt keeps legacy callers tied to Settings::reverse. GUI dual-direction
  // jobs set this explicitly so both views can be calculated in one run.
  std::optional<bool> reverse;
};
std::vector<Job> expandTdrJobs(const std::vector<Job> &, const Settings &);
std::string metricDirection(Metric, bool reverse);
// Runner executes exactly workers callbacks and joins all before returning.
using WorkerRunner = std::function<void(size_t, const std::function<void()> &)>;
size_t workerLimit(int performance, size_t jobs, unsigned hardwareThreads);
std::vector<Result> runJobs(const std::vector<Job> &, const Settings &,
                            Control * = nullptr, const WorkerRunner & = {});
void rankResults(std::vector<Result> &, const std::string &by = "Margin");
struct Delta {
  std::vector<double> frequency, rawDb, improvement;
  double worstImprovement = NaN;
};
Delta compare(const Trace &baseline, const Trace &candidate, Metric);
std::string overall(const std::vector<std::string> &);
void exportRawCsv(const fs::path &, const Trace &, Metric, const Settings &,
                  Control * = nullptr);
} // namespace si
