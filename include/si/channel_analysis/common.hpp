#pragma once
#include "si/core.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace si::channel {
enum class Status {
  Valid, Warning, Stale, NotEvaluated, Unsupported, InvalidSource,
  Cancelled, Error
};
enum class Severity { Info, Warning, Error };
struct Diagnostic {
  Severity severity = Severity::Info;
  std::string code, message;
};
struct Provenance {
  std::string sourceSha256, revision, mappingIdentity, algorithm, version,
      settingsIdentity;
  uint64_t workspaceGeneration = 0, mappingGeneration = 0;
};
template<class T> struct Result {
  Status status = Status::NotEvaluated;
  std::optional<T> value;
  std::vector<Diagnostic> diagnostics;
  Provenance provenance;
  bool usable() const {
    return (status == Status::Valid || status == Status::Warning) &&
           value.has_value();
  }
};
struct SourceIdentity {
  std::string sha256, revision, mappingIdentity;
  uint64_t workspaceGeneration = 0, mappingGeneration = 0;
  bool operator==(const SourceIdentity &) const = default;
};
class SourceSnapshot final {
public:
  static SourceSnapshot capture(std::shared_ptr<const Cache> source,
      std::vector<Channel> confirmedMapping, std::string revision,
      uint64_t workspaceGeneration, uint64_t mappingGeneration,
      Control *control = nullptr);
  const Cache &cache() const { return *source_; }
  const std::vector<Channel> &mapping() const { return mapping_; }
  const SourceIdentity &identity() const { return identity_; }
  bool current(const SourceIdentity &live) const { return identity_ == live; }
  const Channel &channel(std::string_view id) const;
private:
  SourceSnapshot(std::shared_ptr<const Cache> source,
                 std::vector<Channel> mapping, SourceIdentity identity);
  std::shared_ptr<const Cache> source_;
  std::vector<Channel> mapping_;
  SourceIdentity identity_;
};
struct FrequencyQuality {
  bool uniform = false, hasDc = false;
  double minimumHz = 0, maximumHz = 0, nominalStepHz = 0;
  std::vector<Diagnostic> diagnostics;
};
struct ComplexTrace {
  std::vector<double> frequencyHz;
  std::vector<Complex> transfer;
  double stimulusOhm = 0, responseOhm = 0;
  std::string parameter;
  std::vector<Diagnostic> diagnostics;
};
FrequencyQuality validateFrequency(const std::vector<double> &frequency,
                                   bool requireDc = false);
void validateTrace(const ComplexTrace &trace);
ComplexTrace extract(const SourceSnapshot &source, std::string_view victimId,
                     Metric metric, bool reverse = false,
                     std::optional<std::string_view> aggressorId = {},
                     Control *control = nullptr);
Complex interpolateComplex(const ComplexTrace &source, double frequencyHz);
ComplexTrace resample(const ComplexTrace &source,
                      const std::vector<double> &newFrequencyHz,
                      Control *control = nullptr);
ComplexTrace resampleUniform(const ComplexTrace &source, size_t positiveBins,
                             Control *control = nullptr);
void fft(std::vector<Complex> &values, bool inverse,
         Control *control = nullptr);
struct TimeSeries {
  std::vector<double> seconds, values;
  double stepSeconds = 0, binStepHz = 0;
  std::vector<Diagnostic> diagnostics;
};
// Inverse discrete transform of a Hermitian spectrum including DC and Nyquist.
// Samples use h[n] = (1/N) sum_k H[k] exp(+j 2*pi*k*n/N), dt=1/(N*df).
// This is a circular band-limited discrete impulse, not a measured eye/BER.
TimeSeries impulse(const ComplexTrace &positiveSpectrum,
                   Control *control = nullptr);
struct CacheRequest {
  std::string victimId, aggressorId, module, algorithmVersion,
      settingsIdentity, profileIdentity, txIdentity, rxIdentity;
  Metric metric = Metric::IL;
  bool reverse = false;
  double startHz = 0, stopHz = 0;
};
std::string cacheKey(const SourceSnapshot &source, const CacheRequest &request);
} // namespace si::channel
