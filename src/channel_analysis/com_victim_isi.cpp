#include "si/channel_analysis/com_victim_isi.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t kTapCount = 13;
constexpr size_t kCursor = 3;
constexpr double kPam4StdDev = 0.7453559924999298988; // sqrt(5/9)
void require(bool ok, const char* why) {
  if (!ok) throw si::Error(why);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool bounded(const std::string& s) {
  return !s.empty() && s.size() <= 256;
}
bool validSha(const std::string& sha) {
  return sha.size() == 64 &&
      std::all_of(sha.begin(), sha.end(),
                  [](unsigned char c) {return std::isxdigit(c) != 0;});
}
bool sameSource(const Provenance& a, const Provenance& b) {
  return a.sourceSha256 == b.sourceSha256 &&
      a.revision == b.revision &&
      a.mappingIdentity == b.mappingIdentity &&
      a.settingsIdentity == b.settingsIdentity &&
      a.workspaceGeneration == b.workspaceGeneration &&
      a.mappingGeneration == b.mappingGeneration;
}
bool hasCaseField(const std::string& settings) {
  size_t start = 0;
  while (start <= settings.size()) {
    const size_t end = settings.find('|', start);
    if (settings.compare(start, 5, "case=") == 0)
      return true;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return false;
}
void validateSource(const Result<ComScaledVoltageCases>& result,
                    const Provenance& expected) {
  require(validSha(expected.sourceSha256) && bounded(expected.revision) &&
          bounded(expected.mappingIdentity) && bounded(expected.settingsIdentity) &&
          expected.workspaceGeneration != 0 && expected.mappingGeneration != 0 &&
          !hasCaseField(expected.settingsIdentity),
          "012_22: invalid or case-specific expected source identity");
  require(result.status == Status::Warning && result.value.has_value(),
          "012_22: missing, stale or unqualified 012_21 result");
  require(sameSource(result.provenance, expected) &&
          result.provenance.algorithm ==
            "com-receiver-equivalent-unit-symbol-scaling" &&
          result.provenance.version == "012_21-1",
          "012_22: source, settings, generation or upstream version mismatch");
  require(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
          [](const Diagnostic& d) {
            return d.code == "NOT_IEEE_COM" && d.severity == Severity::Warning;
          }),
          "012_22: missing explicit upstream NOT_IEEE_COM diagnostic");
  require(result.value->model ==
            "EXPLORATORY_RECEIVER_EQUIVALENT_UNIT_SYMBOL_VOLTAGE",
          "012_22: upstream model is not receiver-equivalent voltage");
}
void requireNormalOrZero(double value, const char* message) {
  require(std::isnormal(value) || value == 0., message);
}
void validateCase(const ComScaledVoltageCase& v,
                  const char* which, double ui,
                  const std::atomic_bool* cancelled) {
  require(v.testCase == which &&
          bounded(v.transmitterReferencePlane) &&
          bounded(v.receiverDecisionPlane) &&
          v.normalizationIdentity == "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL" &&
          bounded(v.calibrationIdentity) &&
          bounded(v.calibrationSourceIdentity) &&
          bounded(v.txEqualizerIdentity) && bounded(v.rxEqualizerIdentity),
          "012_22: missing case/normalization/plane/EQ/calibration evidence");
  require(std::isnormal(v.voltsPerUnitSymbol) && v.voltsPerUnitSymbol > 0.,
          "012_22: invalid volts-per-unit-symbol scale");
  const double cursorTime = v.timeSeconds[kCursor];
  require(std::isfinite(cursorTime) && cursorTime >= 0.,
          "012_22: nonfinite or negative cursor time");
  const double tol = 1e-6 * ui;
  const double next = std::nextafter(
      cursorTime, std::numeric_limits<double>::infinity());
  require(std::isfinite(next) && next - cursorTime <= tol * 0.5,
          "012_22: insufficient UI time resolution");
  double maxObserved = 0.;
  for (size_t i = 0; i < kTapCount; ++i) {
    check(cancelled);
    const double t = v.timeSeconds[i], u = v.unitSymbolPulse[i];
    const double volts = v.equivalentPulseV[i];
    require(std::isfinite(t) && t >= 0. && std::isfinite(u) &&
            std::isfinite(volts),
            "012_22: invalid time or signed pulse sample");
    require(i == 0 || t > v.timeSeconds[i - 1],
            "012_22: non-monotonic time axis");
    const double nominal = (static_cast<double>(i) - 3.) * ui;
    require(std::abs((t - cursorTime) - nominal) <= tol,
            "012_22: sample not on the -3..+9 UI grid");
    const double reconstructed = v.voltsPerUnitSymbol * u;
    require(std::isfinite(reconstructed) &&
            (u == 0. || std::isnormal(reconstructed)),
            "012_22: sample conversion overflow or underflow");
    // Relative to the sample itself, not to a different large cursor;
    // even a tiny malicious or stale tap must not pass.
    const double denominator = std::max(std::abs(reconstructed), std::abs(volts));
    if (denominator == 0.) {
      require(reconstructed == volts, "012_22: inconsistent zero sample");
    } else {
      require(std::abs(reconstructed - volts) / denominator <= 1e-12,
              "012_22: voltage does not match signed pulse times scale");
    }
    maxObserved = std::max(maxObserved, std::abs(volts));
  }
  require(std::isfinite(v.maximumAbsoluteEquivalentV) &&
          ((maxObserved == 0. && v.maximumAbsoluteEquivalentV == 0.) ||
           (v.maximumAbsoluteEquivalentV > 0. &&
            std::isfinite(maxObserved) &&
            std::abs(v.maximumAbsoluteEquivalentV - maxObserved) /
                std::max(v.maximumAbsoluteEquivalentV, maxObserved) <= 1e-12)),
          "012_22: invalid reported voltage maximum");
  require(v.cursorEquivalentV == v.equivalentPulseV[kCursor],
          "012_22: inconsistent cursor voltage");
  require(std::isnormal(v.cursorEquivalentV),
          "012_22: zero/subnormal cursor cannot define PAM4 spacing");
}
void addMagnitude(double& accum, double value) {
  require(std::isfinite(value) && value >= 0. &&
          value <= std::numeric_limits<double>::max() - accum,
          "012_22: ISI upper bound overflow");
  accum += value;
  require(std::isfinite(accum), "012_22: nonfinite ISI upper bound");
}
ComVictimIsiCaseData analyze(const ComScaledVoltageCase& in,
                             const std::atomic_bool* cancelled) {
  ComVictimIsiCaseData out;
  out.testCase = in.testCase;
  out.cursorSignedV = in.equivalentPulseV[kCursor];
  out.cursorMagnitudeV = std::abs(out.cursorSignedV);
  out.transmitterReferencePlane = in.transmitterReferencePlane;
  out.receiverDecisionPlane = in.receiverDecisionPlane;
  out.normalizationIdentity = in.normalizationIdentity;
  out.calibrationIdentity = in.calibrationIdentity;
  out.calibrationSourceIdentity = in.calibrationSourceIdentity;
  out.txEqualizerIdentity = in.txEqualizerIdentity;
  out.rxEqualizerIdentity = in.rxEqualizerIdentity;

  double preMagnitude = 0., postMagnitude = 0., bound = 0.;
  for (size_t i = 0; i < kTapCount; ++i) {
    check(cancelled);
    if (i == kCursor) continue;
    const double a = std::abs(in.equivalentPulseV[i]);
    if (i < kCursor) preMagnitude = std::hypot(preMagnitude, a);
    else postMagnitude = std::hypot(postMagnitude, a);
    require(std::isfinite(preMagnitude) && std::isfinite(postMagnitude),
            "012_22: ISI root sum of squares overflow");
    addMagnitude(bound, a);
  }
  out.precursorRmsV = kPam4StdDev * preMagnitude;
  out.postcursorRmsV = kPam4StdDev * postMagnitude;
  out.totalIsiRmsV = kPam4StdDev * std::hypot(preMagnitude, postMagnitude);
  out.worstCaseIsiBoundV = bound;
  out.nominalPam4HalfSpacingV = out.cursorMagnitudeV / 3.;
  requireNormalOrZero(out.precursorRmsV, "012_22: precursor RMS underflow");
  requireNormalOrZero(out.postcursorRmsV, "012_22: postcursor RMS underflow");
  requireNormalOrZero(out.totalIsiRmsV, "012_22: total RMS underflow");
  require(std::isfinite(out.totalIsiRmsV) &&
          std::isfinite(out.nominalPam4HalfSpacingV) &&
          std::isnormal(out.nominalPam4HalfSpacingV),
          "012_22: invalid PAM4 half-spacing or ISI RMS");
  return out;
}
} // namespace

Result<ComVictimIsiCasesData> calculateComVictimIsi(
    const Result<ComScaledVoltageCases>& scaled,
    const Provenance& expected,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  validateSource(scaled, expected);
  const auto& pair = *scaled.value;
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  validateCase(pair.test1, "TEST1", ui, cancelled);
  validateCase(pair.test2, "TEST2", ui, cancelled);
  const auto& a = pair.test1;
  const auto& b = pair.test2;
  require(a.transmitterReferencePlane == b.transmitterReferencePlane &&
          a.receiverDecisionPlane == b.receiverDecisionPlane &&
          a.normalizationIdentity == b.normalizationIdentity &&
          a.calibrationIdentity == b.calibrationIdentity &&
          a.txEqualizerIdentity == b.txEqualizerIdentity &&
          a.rxEqualizerIdentity == b.rxEqualizerIdentity,
          "012_22: Test1/Test2 receiver planes, calibration or EQ mismatch");
  const double pairTimeTolerance = 1e-6 * ui;
  for (size_t i = 0; i < kTapCount; ++i) {
    check(cancelled);
    require(std::abs(a.timeSeconds[i] - b.timeSeconds[i]) <= pairTimeTolerance,
            "012_22: Test1/Test2 sample grids disagree");
  }
  ComVictimIsiCasesData data;
  data.test1 = analyze(a, cancelled);
  data.test2 = analyze(b, cancelled);
  check(cancelled);
  Result<ComVictimIsiCasesData> out;
  out.status = Status::Warning;
  out.value = std::move(data);
  out.provenance = expected;
  out.provenance.algorithm = "com-victim-self-isi-exploratory";
  out.provenance.version = "012_22-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Exploratory victim SELF-ISI voltage diagnostics; uniform independent "
      "PAM4 symbols are a mathematical assumption, not physical proof. "
      "Not aggressor crosstalk, BER, DER, COM or PHY PASS/FAIL."});
  return out;
}
} // namespace si::channel
