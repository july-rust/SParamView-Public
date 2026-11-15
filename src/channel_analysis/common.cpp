#include "si/channel_analysis/common.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t maxSamples = 1u << 20;
void require(bool condition, const char *message) {
  if (!condition) throw Error(message);
}
void check(Control *control) { if (control) control->check(); }
bool finite(Complex c) {
  return std::isfinite(c.real()) && std::isfinite(c.imag());
}
std::string encode(std::string_view value) {
  return std::to_string(value.size()) + ":" + std::string(value);
}
std::string hexDouble(double value) {
  require(std::isfinite(value), "Nonfinite cache key setting");
  std::ostringstream ss;
  ss << std::hex << std::setw(16) << std::setfill('0')
     << std::bit_cast<uint64_t>(value);
  return ss.str();
}
std::string mappingIdentity(const std::vector<Channel> &mapping) {
  // Length-prefixing separates fields even when IDs contain delimiters.
  std::string result = "map-v1:" + std::to_string(mapping.size()) + ":";
  for (const auto &ch : mapping) {
    result += encode(ch.id) + encode(ch.name) +
        encode(ch.group) + encode(ch.alias);
    for (int p : {ch.nearP, ch.nearN, ch.farP, ch.farN})
      result += std::to_string(p) + ";";
  }
  return result;
}
} // namespace

SourceSnapshot::SourceSnapshot(std::shared_ptr<const Cache> source,
    std::vector<Channel> mapping, SourceIdentity identity)
  : source_(std::move(source)), mapping_(std::move(mapping)),
    identity_(std::move(identity)) {}

SourceSnapshot SourceSnapshot::capture(std::shared_ptr<const Cache> source,
    std::vector<Channel> confirmedMapping, std::string revision,
    uint64_t workspaceGeneration, uint64_t mappingGeneration,
    Control *control) {
  require(source != nullptr, "Missing source cache");
  require(!revision.empty() && workspaceGeneration && mappingGeneration,
          "Unconfirmed source revision or generation");
  const auto &meta = source->meta();
  require(meta.sha256.size() == 64 && !meta.source.empty(),
          "Missing SHA-256-qualified source");
  require(!confirmedMapping.empty(), "Unconfirmed mapping");
  require(meta.reference.size() == meta.ports && meta.ports != 0,
          "Missing physical port reference impedances");
  for (double z : meta.reference)
    require(std::isfinite(z) && z > 0, "Invalid reference impedance");
  validateMapping(confirmedMapping, meta);
  check(control);
  // Rehash the actual source file: a cached fingerprint is NOT a verified SHA.
  const auto actual = sha256File(pathFromUtf8(meta.source), control);
  require(actual == meta.sha256, "Source SHA-256 differs from cached revision");
  check(control);
  const auto mappingKey = mappingIdentity(confirmedMapping);
  return SourceSnapshot(std::move(source), std::move(confirmedMapping),
    {actual, std::move(revision), mappingKey,
     workspaceGeneration, mappingGeneration});
}

const Channel &SourceSnapshot::channel(std::string_view id) const {
  const auto it = std::find_if(mapping_.begin(), mapping_.end(),
      [&](const Channel &ch) { return ch.id == id; });
  if (it == mapping_.end()) throw Error("Channel is not in confirmed mapping");
  return *it;
}

FrequencyQuality validateFrequency(const std::vector<double> &frequency,
                                   bool requireDc) {
  require(frequency.size() >= 2 && frequency.size() <= maxSamples,
          "Insufficient or excessive frequency samples");
  for (size_t i = 0; i < frequency.size(); ++i) {
    require(std::isfinite(frequency[i]) && frequency[i] >= 0,
            "Invalid or negative frequency");
    if (i) require(frequency[i] > frequency[i - 1],
                   "Non-monotone or duplicate frequency");
  }
  FrequencyQuality out;
  out.minimumHz = frequency.front();
  out.maximumHz = frequency.back();
  out.hasDc = frequency.front() == 0;
  out.nominalStepHz = (frequency.back() - frequency.front()) /
                      double(frequency.size() - 1);
  out.uniform = true;
  for (size_t i = 1; i < frequency.size(); ++i) {
    const double step = frequency[i] - frequency[i - 1];
    if (std::abs(step - out.nominalStepHz) >
        1e-9 * std::max(step, out.nominalStepHz)) {
      out.uniform = false;
      break;
    }
  }
  if (!out.uniform)
    out.diagnostics.push_back({Severity::Warning, "NONUNIFORM",
       "Nonuniform frequency intervals; explicit resampling required for FFT"});
  if (!out.hasDc) {
    if (requireDc) throw Error("DC sample required; extrapolation forbidden");
    out.diagnostics.push_back({Severity::Warning, "NO_DC",
       "No measured DC; time-domain inversion is not qualified"});
  }
  return out;
}
void validateTrace(const ComplexTrace &trace) {
  require(trace.transfer.size() == trace.frequencyHz.size(),
          "Frequency/complex trace length mismatch");
  validateFrequency(trace.frequencyHz);
  require(std::isfinite(trace.stimulusOhm) && trace.stimulusOhm > 0 &&
          std::isfinite(trace.responseOhm) && trace.responseOhm > 0,
          "Missing or invalid reference impedance");
  for (Complex value : trace.transfer)
    require(finite(value), "Nonfinite complex S-parameter");
}
ComplexTrace extract(const SourceSnapshot &source, std::string_view victimId,
                     Metric metric, bool reverse,
                     std::optional<std::string_view> aggressorId,
                     Control *control) {
  require(metric != Metric::TDR, "TDR conditioning is owned by legacy TDR");
  const auto &victim = source.channel(victimId);
  const Channel *aggressor = nullptr;
  if (metric == Metric::NEXT || metric == Metric::FEXT) {
    require(aggressorId.has_value(), "Aggressor required for NEXT/FEXT");
    aggressor = &source.channel(*aggressorId);
    require(aggressor->id != victim.id, "Victim equals aggressor");
    require(aggressor->differential() == victim.differential(),
            "Mixed single-ended/differential modes are unsupported");
  } else {
    require(!aggressorId.has_value(), "Unexpected aggressor");
  }
  Settings settings;
  settings.reverse = reverse;
  check(control);
  auto raw = loadTrace(source.cache(), victim, metric, settings,
                       aggressor, control);
  ComplexTrace out{std::move(raw.x), std::move(raw.s), raw.referenceOhm,
                   raw.responseReferenceOhm, std::move(raw.parameter), {}};
  validateTrace(out);
  const auto frequency = validateFrequency(out.frequencyHz);
  out.diagnostics = frequency.diagnostics;
  if (std::abs(out.stimulusOhm - out.responseOhm) >
      1e-10 * std::max(out.stimulusOhm, out.responseOhm))
    out.diagnostics.push_back({Severity::Warning, "UNEQUAL_REFERENCE",
      "Stimulus/response reference impedances differ; transfer normalization required"});
  return out;
}

Complex interpolateComplex(const ComplexTrace &source, double frequencyHz) {
  validateTrace(source);
  require(std::isfinite(frequencyHz) &&
      frequencyHz >= source.frequencyHz.front() &&
      frequencyHz <= source.frequencyHz.back(),
      "Interpolation outside source band");
  auto it = std::lower_bound(source.frequencyHz.begin(),
                             source.frequencyHz.end(), frequencyHz);
  const size_t i = size_t(it - source.frequencyHz.begin());
  if (i == 0 || *it == frequencyHz) return source.transfer[i];
  const double t = (frequencyHz - source.frequencyHz[i - 1]) /
                   (source.frequencyHz[i] - source.frequencyHz[i - 1]);
  return (1.0 - t) * source.transfer[i - 1] + t * source.transfer[i];
}
ComplexTrace resample(const ComplexTrace &source,
                      const std::vector<double> &newFrequencyHz,
                      Control *control) {
  validateTrace(source);
  validateFrequency(newFrequencyHz);
  require(newFrequencyHz.front() >= source.frequencyHz.front() &&
          newFrequencyHz.back() <= source.frequencyHz.back(),
          "Resampling outside measured frequency band");
  ComplexTrace result;
  result.frequencyHz = newFrequencyHz;
  result.stimulusOhm = source.stimulusOhm;
  result.responseOhm = source.responseOhm;
  result.parameter = source.parameter;
  result.diagnostics = source.diagnostics;
  result.transfer.reserve(newFrequencyHz.size());
  for (size_t i = 0; i < newFrequencyHz.size(); ++i) {
    if (!(i % 1024)) check(control);
    // Preserve complex rectangular components (never magnitude-only).
    const auto it = std::lower_bound(source.frequencyHz.begin(),
                                    source.frequencyHz.end(), newFrequencyHz[i]);
    const size_t k = size_t(it - source.frequencyHz.begin());
    if (k == 0 || *it == newFrequencyHz[i]) {
      result.transfer.push_back(source.transfer[k]);
    } else {
      const double a = (newFrequencyHz[i] - source.frequencyHz[k - 1]) /
        (source.frequencyHz[k] - source.frequencyHz[k - 1]);
      result.transfer.push_back((1.0 - a) * source.transfer[k - 1] +
                                a * source.transfer[k]);
    }
  }
  if (newFrequencyHz != source.frequencyHz)
    result.diagnostics.push_back({Severity::Warning, "RESAMPLED",
      "Piecewise-linear complex interpolation; verify spectral convergence"});
  return result;
}
ComplexTrace resampleUniform(const ComplexTrace &source, size_t positiveBins,
                             Control *control) {
  validateTrace(source);
  validateFrequency(source.frequencyHz, true);
  require(positiveBins >= 3 && positiveBins <= maxSamples / 2 + 1 &&
          std::has_single_bit(2 * (positiveBins - 1)),
          "Uniform FFT bins require radix-2 real spectrum with >= 4 time samples");
  const double fmax = source.frequencyHz.back();
  require(fmax > 0, "Insufficient measured bandwidth");
  std::vector<double> bins;
  bins.reserve(positiveBins);
  for (size_t i = 0; i < positiveBins; ++i)
    bins.push_back(fmax * double(i) / double(positiveBins - 1));
  return resample(source, bins, control);
}
void fft(std::vector<Complex> &values, bool inverse, Control *control) {
  const size_t n = values.size();
  require(n >= 2 && n <= maxSamples && std::has_single_bit(n),
          "FFT length must be a bounded power of two");
  for (Complex z : values) require(finite(z), "Nonfinite FFT input");
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(values[i], values[j]);
  }
  for (size_t length = 2; length <= n; length <<= 1) {
    check(control);
    const double angle = (inverse ? 2.0 : -2.0) *
                         std::numbers::pi / double(length);
    const Complex root = std::polar(1.0, angle);
    for (size_t i = 0; i < n; i += length) {
      Complex factor{1, 0};
      for (size_t j = 0; j < length / 2; ++j) {
        const Complex u = values[i + j];
        const Complex v = factor * values[i + j + length / 2];
        values[i + j] = u + v;
        values[i + j + length / 2] = u - v;
        factor *= root;
      }
    }
  }
  if (inverse)
    for (auto &value : values) value /= double(n);
}
TimeSeries impulse(const ComplexTrace &positiveSpectrum, Control *control) {
  validateTrace(positiveSpectrum);
  const auto quality = validateFrequency(positiveSpectrum.frequencyHz, true);
  require(quality.uniform, "Nonuniform spectrum must be explicitly resampled");
  const size_t bins = positiveSpectrum.transfer.size();
  require(bins >= 3 && bins - 1 <= maxSamples / 2 &&
          std::has_single_bit(2 * (bins - 1)),
          "Real IFFT requires radix-2 even-length spectrum");
  const size_t n = 2 * (bins - 1);
  const double df = quality.nominalStepHz;
  require(df > 0 && std::isfinite(double(n) * df),
          "Invalid transform sampling frequency");
  auto endpointReal = [&](Complex z) {
    return std::abs(z.imag()) <= 1e-10 * std::max(1.0, std::abs(z.real()));
  };
  require(endpointReal(positiveSpectrum.transfer.front()) &&
          endpointReal(positiveSpectrum.transfer.back()),
          "DC and Nyquist must be real for Hermitian impulse");
  std::vector<Complex> spectrum(n);
  spectrum[0] = {positiveSpectrum.transfer.front().real(), 0};
  spectrum[n / 2] = {positiveSpectrum.transfer.back().real(), 0};
  for (size_t k = 1; k < n / 2; ++k) {
    spectrum[k] = positiveSpectrum.transfer[k];
    spectrum[n - k] = std::conj(positiveSpectrum.transfer[k]);
  }
  fft(spectrum, true, control);
  const double dt = 1.0 / (double(n) * df);
  TimeSeries result;
  result.stepSeconds = dt;
  result.binStepHz = df;
  result.diagnostics = positiveSpectrum.diagnostics;
  result.seconds.reserve(n);
  result.values.reserve(n);
  for (size_t k = 0; k < n; ++k) {
    if (!(k % 1024)) check(control);
    result.seconds.push_back(double(k) * dt);
    result.values.push_back(spectrum[k].real());
  }
  result.diagnostics.push_back({Severity::Info, "CIRCULAR_IMPULSE",
    "Band-limited circular IFFT; window/causality and tail must be qualified by caller"});
  return result;
}

std::string cacheKey(const SourceSnapshot &source, const CacheRequest &r) {
  require(!r.module.empty() && !r.algorithmVersion.empty() &&
          !r.settingsIdentity.empty(), "Incomplete analysis cache configuration");
  require(std::isfinite(r.startHz) && std::isfinite(r.stopHz) &&
          r.startHz >= 0 && r.stopHz > r.startHz,
          "Invalid analysis frequency selection");
  (void)source.channel(r.victimId);
  if (!r.aggressorId.empty()) (void)source.channel(r.aggressorId);
  if ((r.metric == Metric::NEXT || r.metric == Metric::FEXT) &&
      (r.aggressorId.empty() || r.aggressorId == r.victimId))
    throw Error("Invalid aggressor cache identity");
  const auto &i = source.identity();
  std::string key = "channel-cache-v1:";
  for (const auto &part : {i.sha256, i.revision, i.mappingIdentity,
        std::to_string(i.workspaceGeneration),
        std::to_string(i.mappingGeneration), r.module, r.algorithmVersion,
        r.victimId, r.aggressorId, r.settingsIdentity,
        r.profileIdentity, r.txIdentity, r.rxIdentity})
    key += encode(part);
  key += std::to_string(static_cast<int>(r.metric)) + ";" +
         std::to_string(int(r.reverse)) + ";" +
         hexDouble(r.startHz) + hexDouble(r.stopHz);
  return key;
}
} // namespace si::channel
