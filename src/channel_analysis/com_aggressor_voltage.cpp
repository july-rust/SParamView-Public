#include "si/channel_analysis/com_aggressor_voltage.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t kTapCount = 13;
constexpr size_t kMaxAggressors = 32;
void require(bool ok, const char* reason) {
  if (!ok) throw si::Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool bounded(const std::string& s) { return !s.empty() && s.size() <= 256; }
bool sameSource(const Provenance& a, const Provenance& b) {
  return a.sourceSha256 == b.sourceSha256 && a.revision == b.revision &&
         a.mappingIdentity == b.mappingIdentity &&
         a.settingsIdentity == b.settingsIdentity &&
         a.workspaceGeneration == b.workspaceGeneration &&
         a.mappingGeneration == b.mappingGeneration;
}
bool qualified(const Result<ComInterferenceTestCases>& r,
               const Provenance& expected) {
  if (r.status != Status::Warning || !r.value ||
      !sameSource(r.provenance, expected) ||
      r.provenance.algorithm != "com-120f8-separated-test1-test2-interference" ||
      r.provenance.version != "012_14-1") return false;
  return std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
    [](const Diagnostic& d) {
      return d.severity == Severity::Warning && d.code == "NOT_IEEE_COM";
    });
}
Provenance caseIdentity(const Provenance& pair, const char* which) {
  Provenance p = pair;
  if (!p.settingsIdentity.empty() && p.settingsIdentity.back() != '|')
    p.settingsIdentity.push_back('|');
  p.settingsIdentity += "case=";
  p.settingsIdentity += which;
  require(p.settingsIdentity.size() <= 256, "012_24: case identity too long");
  return p;
}
void validateLive(const Provenance& expected, const SourceIdentity& live) {
  require(expected.sourceSha256 == live.sha256 &&
          expected.revision == live.revision &&
          expected.mappingIdentity == live.mappingIdentity &&
          expected.workspaceGeneration == live.workspaceGeneration &&
          expected.mappingGeneration == live.mappingGeneration,
          "012_24: stale or incompatible live SourceSnapshot");
}
void validateTime(const ComInterferenceData& x,
                  const ComScaledVoltageCase& victim,
                  const std::atomic_bool* cancelled) {
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  const double tol = 1e-6 * ui;
  require(x.model == "EXPLORATORY_BANDLIMITED_INDIVIDUAL_XTALK_PULSES" &&
          x.timeSeconds.size() == kTapCount &&
          std::isfinite(x.symbolPeriodSeconds) &&
          std::abs(x.symbolPeriodSeconds - ui) <= 1e-10 * ui,
          "012_24: unsupported interference model or symbol grid");
  for (size_t i = 0; i < kTapCount; ++i) {
    check(cancelled);
    const double t = x.timeSeconds[i];
    require(std::isfinite(t) && t >= 0. &&
            (i == 0 || t > x.timeSeconds[i - 1]) &&
            std::abs(t - victim.timeSeconds[i]) <= tol &&
            std::abs((t - x.timeSeconds[3]) -
                     (static_cast<double>(i) - 3.) * ui) <= tol,
            "012_24: victim/aggressor decision time or UI mismatch");
  }
}
void validateCalibration(const Result<ComAggressorVoltageEvidence>& r,
                         const ComInterferencePulse& pulse,
                         const ComScaledVoltageCase& victim,
                         const Provenance& expected, const char* which) {
  require(r.usable() && sameSource(r.provenance, expected) &&
          bounded(r.provenance.algorithm) && bounded(r.provenance.version),
          "012_24: missing/stale per-case aggressor calibration");
  const auto& v = *r.value;
  require(v.aggressorId == pulse.id && v.testCase == which &&
          v.coupling == pulse.coupling &&
          std::isnormal(v.voltsPerUnitSymbol) && v.voltsPerUnitSymbol > 0. &&
          bounded(v.transmitterReferencePlane) &&
          v.receiverDecisionPlane == victim.receiverDecisionPlane &&
          v.normalizationIdentity ==
            "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL" &&
          v.normalizationIdentity == victim.normalizationIdentity &&
          bounded(v.calibrationIdentity) &&
          v.calibrationIdentity == victim.calibrationIdentity &&
          bounded(v.calibrationSourceIdentity) &&
          bounded(v.txEqualizerIdentity) &&
          v.rxEqualizerIdentity == victim.rxEqualizerIdentity,
          "012_24: unsupported calibration/plane/EQ or aggressor identity");
}
ComAggressorVoltageCaseData convertCase(
    const ComInterferenceData& interference,
    const ComScaledVoltageCase& victim,
    const ComAggressorVoltageCaseInput& input,
    const Provenance& expected, const char* which,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  validateTime(interference, victim, cancelled);
  require(interference.aggressors.size() <= kMaxAggressors &&
          input.calibrations.size() == interference.aggressors.size(),
          "012_24: aggressor count/calibration budget mismatch");
  std::unordered_map<std::string, const ComInterferencePulse*> pulses;
  for (const auto& p : interference.aggressors) {
    check(cancelled);
    require(bounded(p.id) &&
            (p.coupling == ComCoupling::Next ||
             p.coupling == ComCoupling::Fext) &&
            p.unitSymbolPulse.size() == kTapCount &&
            pulses.emplace(p.id, &p).second,
            "012_24: invalid/duplicate aggressor ID, coupling or pulse length");
  }
  std::unordered_set<std::string> used;
  ComAggressorVoltageCaseData out;
  out.testCase = which;
  out.receiverDecisionPlane = victim.receiverDecisionPlane;
  out.timeSeconds = interference.timeSeconds;
  out.symbolPeriodSeconds = interference.symbolPeriodSeconds;
  out.aggressors.reserve(input.calibrations.size());
  for (const auto& r : input.calibrations) {
    check(cancelled);
    require(r.usable() && bounded(r.value->aggressorId),
            "012_24: missing aggressor calibration ID");
    const auto it = pulses.find(r.value->aggressorId);
    require(it != pulses.end() && used.insert(r.value->aggressorId).second,
            "012_24: unmatched or repeated aggressor calibration");
    const auto& pulse = *it->second;
    validateCalibration(r, pulse, victim, expected, which);
    const auto& e = *r.value;
    ComAggressorVoltagePulse result;
    result.id = pulse.id;
    result.testCase = which;
    result.coupling = pulse.coupling;
    result.voltsPerUnitSymbol = e.voltsPerUnitSymbol;
    result.transmitterReferencePlane = e.transmitterReferencePlane;
    result.receiverDecisionPlane = e.receiverDecisionPlane;
    result.normalizationIdentity = e.normalizationIdentity;
    result.calibrationIdentity = e.calibrationIdentity;
    result.calibrationSourceIdentity = e.calibrationSourceIdentity;
    result.txEqualizerIdentity = e.txEqualizerIdentity;
    result.rxEqualizerIdentity = e.rxEqualizerIdentity;
    double observedInputMaximum = 0.;
    for (size_t i = 0; i < kTapCount; ++i) {
      check(cancelled);
      const double u = pulse.unitSymbolPulse[i];
      require(std::isfinite(u), "012_24: nonfinite unit-symbol pulse");
      const double voltage = u * e.voltsPerUnitSymbol;
      require(std::isfinite(voltage) &&
              (std::fpclassify(u) == FP_ZERO || std::isnormal(voltage)),
              "012_24: voltage overflow or underflow");
      result.unitSymbolPulse[i] = u;
      result.equivalentPulseV[i] = voltage;
      observedInputMaximum = std::max(observedInputMaximum, std::abs(u));
      result.maximumAbsoluteEquivalentV =
          std::max(result.maximumAbsoluteEquivalentV, std::abs(voltage));
    }
    require(std::isfinite(pulse.maximumAbsolutePulse) &&
            pulse.maximumAbsolutePulse >= 0.,
            "012_24: invalid upstream maximum");
    if (std::fpclassify(observedInputMaximum) == FP_ZERO) {
      require(std::fpclassify(pulse.maximumAbsolutePulse) == FP_ZERO,
              "012_24: zero waveform reports nonzero maximum");
    } else {
      require(pulse.maximumAbsolutePulse > 0. &&
              std::abs(pulse.maximumAbsolutePulse - observedInputMaximum) /
              std::max(pulse.maximumAbsolutePulse, observedInputMaximum) <= 1e-12,
              "012_24: tampered upstream pulse maximum");
    }
    out.aggressors.push_back(std::move(result));
  }
  std::sort(out.aggressors.begin(), out.aggressors.end(),
            [](const ComAggressorVoltagePulse& a,
               const ComAggressorVoltagePulse& b) { return a.id < b.id; });
  return out;
}
void sameAggressors(const ComAggressorVoltageCaseData& a,
                    const ComAggressorVoltageCaseData& b) {
  require(a.aggressors.size() == b.aggressors.size(),
          "012_24: Test1/Test2 aggressor count mismatch");
  for (size_t i = 0; i < a.aggressors.size(); ++i)
    require(a.aggressors[i].id == b.aggressors[i].id &&
            a.aggressors[i].coupling == b.aggressors[i].coupling,
            "012_24: Test1/Test2 aggressor/coupling set mismatch");
}
} // namespace

Result<ComAggressorVoltageCasesData> calculateComAggressorReceiverVoltage(
    const ComAggressorVoltageInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled) {
  check(cancelled);
  validateLive(expectedPair, live);
  // 012_22 revalidates original signed 012_21 pulses, time grid and scale.
  const auto victimIsi = calculateComVictimIsi(input.victim, expectedPair, cancelled);
  require(victimIsi.usable() && victimIsi.status == Status::Warning &&
          victimIsi.provenance.algorithm == "com-victim-self-isi-exploratory" &&
          victimIsi.provenance.version == "012_22-1",
          "012_24: unqualified victim pulse source");
  require(qualified(input.interference, expectedPair),
          "012_24: missing/unqualified 012_14 interference");
  const auto p1 = caseIdentity(expectedPair, "TEST1");
  const auto p2 = caseIdentity(expectedPair, "TEST2");
  const auto& pulses = *input.interference.value;
  const auto& voltage = *input.victim.value;
  ComAggressorVoltageCasesData data;
  data.test1 = convertCase(pulses.test1, voltage.test1, input.test1,
                           p1, "TEST1", cancelled);
  data.test2 = convertCase(pulses.test2, voltage.test2, input.test2,
                           p2, "TEST2", cancelled);
  sameAggressors(data.test1, data.test2);
  check(cancelled);
  Result<ComAggressorVoltageCasesData> out;
  out.status = Status::Warning;
  out.value = std::move(data);
  out.provenance = expectedPair;
  out.provenance.algorithm = "com-aggressor-receiver-voltage-exploratory";
  out.provenance.version = "012_24-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Caller-declared NEXT/FEXT voltage scaling only. Provenance consistency "
    "does not prove TX calibration, physically conditioned transfer or "
    "aggressor independence. No statistical x-talk, COM dB or PHY verdict."});
  return out;
}
} // namespace si::channel
