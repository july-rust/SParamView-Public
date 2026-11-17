#include "si/channel_analysis/com_interference.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t kMaxAggressors = 32;
constexpr size_t kMaxWork = 16u * 1024u * 1024u; // frequency bins x times x sources

void require(bool ok, const char* reason) {
  if (!ok) throw Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
bool finite(Complex z) {
  return std::isfinite(z.real()) && std::isfinite(z.imag());
}
size_t work(const ComPreparedData& input, size_t samples) {
  const size_t bins = input.frequencyHz.size();
  const size_t sources = std::max<size_t>(1, input.aggressors.size());
  // Division before multiplication makes the global bound overflow-proof.
  require(samples != 0 && bins != 0 &&
          sources <= kMaxWork / samples / bins,
          "COM interference aggregate numerical work budget exceeded");
  return sources * samples * bins;
}
void validate(const ComPreparedData& input, const std::vector<double>& times,
              const std::atomic_bool* cancelled) {
  check(cancelled);
  const auto& freq = input.frequencyHz;
  validateCom120F8DirectGrid(freq);
  require(freq.size() <= 32769 && input.conditionedThru.size() == freq.size(),
          "COM interference requires complete bounded conditioned THRU");
  require(!times.empty() && times.size() <= 512,
          "COM interference requires 1..512 explicit time samples");
  require(input.aggressors.size() <= kMaxAggressors,
          "COM interference exceeds aggressor count limit");
  (void)work(input, times.size());
  const double df = freq[1] - freq[0];
  require(std::isfinite(df) && df > 0, "Invalid interference frequency step");
  for (size_t i = 0; i < freq.size(); ++i) {
    if (!(i & 1023u)) check(cancelled);
    require(finite(input.conditionedThru[i]), "Nonfinite conditioned THRU");
    if (i >= 2)
      require(std::abs((freq[i] - freq[i-1]) - df) <= 1e-9 * df,
              "COM interference requires uniform frequency grid");
  }
  require(std::abs(input.conditionedThru.front().imag()) <=
              1e-10 * std::max(1., std::abs(input.conditionedThru.front().real())),
          "COM interference THRU DC must be real");
  for (size_t i = 0; i < times.size(); ++i) {
    require(std::isfinite(times[i]) && times[i] >= 0 && times[i] * df <= 0.45,
            "COM interference time exceeds frequency-grid support");
    if (i)
      require(times[i] > times[i-1],
              "COM interference time samples must be increasing");
  }
  std::unordered_set<std::string> ids;
  for (const auto& a : input.aggressors) {
    check(cancelled);
    require(!a.id.empty() && ids.insert(a.id).second,
            "Duplicate or empty COM aggressor ID");
    require(a.coupling == ComCoupling::Next || a.coupling == ComCoupling::Fext,
            "Unsupported COM coupling enum");
    require(a.conditionedTransfer.size() == freq.size(),
            "COM aggressor spectrum length mismatch");
    for (size_t i = 0; i < freq.size(); ++i) {
      if (!(i & 1023u)) check(cancelled);
      require(finite(a.conditionedTransfer[i]),
              "Nonfinite COM aggressor transfer");
    }
    const auto dc = a.conditionedTransfer.front();
    require(std::abs(dc.imag()) <= 1e-10 * std::max(1., std::abs(dc.real())),
            "COM aggressor DC must be real");
  }
}
void sameAggressors(const ComPreparedData& first,
                    const ComPreparedData& second) {
  require(first.aggressors.size() == second.aggressors.size(),
          "Test1/Test2 aggressor count mismatch");
  std::unordered_map<std::string, ComCoupling> ids;
  for (const auto& a : first.aggressors) ids.emplace(a.id, a.coupling);
  for (const auto& a : second.aggressors) {
    const auto it = ids.find(a.id);
    require(it != ids.end() && it->second == a.coupling,
            "Test1/Test2 aggressor identity or coupling mismatch");
  }
}
} // namespace

Result<ComInterferenceData> calculateComInterferencePulses(
    const ComPreparedData& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance, const std::atomic_bool* cancelled) {
  validate(conditioned, queryTimeSeconds, cancelled);
  ComInterferenceData data;
  data.timeSeconds = queryTimeSeconds;
  data.symbolPeriodSeconds = 1. / final8023ck120F8().symbolRateHz;
  data.aggressors.reserve(conditioned.aggressors.size());

  // Bound the temporary spectrum to exactly ONE aggressor, reusing the
  // already-qualified phase-preserving rectangular-symbol quadrature.
  ComPreparedData single;
  single.frequencyHz = conditioned.frequencyHz;
  single.conditionedThru.reserve(conditioned.frequencyHz.size());
  for (const auto& a : conditioned.aggressors) {
    check(cancelled);
    single.conditionedThru.assign(a.conditionedTransfer.begin(),
                                  a.conditionedTransfer.end());
    ComInterferencePulse item;
    item.id = a.id;
    item.coupling = a.coupling;
    item.unitSymbolPulse.reserve(queryTimeSeconds.size());
    // The existing single-pulse integrator also has its own stricter
    // 4,000,000 bin-time cap. Chunk the explicitly validated common time grid
    // so the advertised aggregate budget is actually executable.
    const size_t maxChunk = 4000000u / single.frequencyHz.size();
    require(maxChunk > 0, "COM pulse primitive chunk budget unavailable");
    for (size_t begin = 0; begin < queryTimeSeconds.size(); begin += maxChunk) {
      check(cancelled);
      const size_t end = std::min(queryTimeSeconds.size(), begin + maxChunk);
      const std::vector<double> chunk(queryTimeSeconds.begin() + begin,
                                      queryTimeSeconds.begin() + end);
      auto pulse = calculateCom120F8RectangularPulse(
          single, chunk, {}, cancelled);
      require(pulse.usable(), "COM interference pulse primitive not usable");
      item.maximumAbsolutePulse =
          std::max(item.maximumAbsolutePulse, pulse.value->maximumAbsolutePulse);
      item.unitSymbolPulse.insert(item.unitSymbolPulse.end(),
          pulse.value->unitSymbolPulse.begin(), pulse.value->unitSymbolPulse.end());
    }
    data.aggressors.push_back(std::move(item));
  }
  check(cancelled);
  Result<ComInterferenceData> result;
  result.status = Status::Warning;
  result.value = std::move(data);
  result.provenance = std::move(provenance);
  result.provenance.algorithm = "com-120f8-individual-interference-pulses";
  result.provenance.version = "012_14-1";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Individual unit-symbol, finite-band NEXT/FEXT pulses only; no verified "
    "port mapping from scalar inputs, stochastic crosstalk combination, "
    "calibrated voltage, noise, COM dB or IEEE PHY PASS/FAIL."});
  return result;
}

Result<ComInterferenceTestCases> calculateComInterferenceTestCases(
    const Com120F8LoadedTestCaseSpectra& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance, const std::atomic_bool* cancelled) {
  validate(conditioned.test1, queryTimeSeconds, cancelled);
  validate(conditioned.test2, queryTimeSeconds, cancelled);
  require(conditioned.test1.frequencyHz == conditioned.test2.frequencyHz,
          "Test1/Test2 must share an identical frequency grid");
  sameAggressors(conditioned.test1, conditioned.test2);
  const size_t firstWork = work(conditioned.test1, queryTimeSeconds.size());
  const size_t secondWork = work(conditioned.test2, queryTimeSeconds.size());
  require(firstWork <= kMaxWork - secondWork,
          "Combined Test1/Test2 interference work budget exceeded");
  check(cancelled);
  auto first = calculateComInterferencePulses(
      conditioned.test1, queryTimeSeconds, provenance, cancelled);
  auto second = calculateComInterferencePulses(
      conditioned.test2, queryTimeSeconds, provenance, cancelled);
  require(first.usable() && second.usable(),
          "Test1/Test2 interference calculation unavailable");
  Result<ComInterferenceTestCases> out;
  out.status = Status::Warning;
  out.value = ComInterferenceTestCases{
      std::move(*first.value), std::move(*second.value)};
  out.provenance = std::move(provenance);
  out.provenance.algorithm = "com-120f8-separated-test1-test2-interference";
  out.provenance.version = "012_14-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Test1/Test2 per-aggressor pulse diagnostics; neither case constitutes "
    "a standard-qualified COM margin, full PHY oracle or PASS/FAIL."});
  return out;
}
} // namespace si::channel
