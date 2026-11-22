#include "si/channel_analysis/com_victim_isi_tail.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace si::channel {
namespace {
constexpr size_t kCursorIndex = 3;
constexpr size_t kMaxExactTaps = 6;

void require(bool yes, const char* reason) {
  if (!yes) throw si::Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool bounded(const std::string& s) {
  return !s.empty() && s.size() <= 256;
}
Provenance caseIdentity(const Provenance& pair, const char* which) {
  Provenance p = pair;
  // 012_22 has already rejected any single-case pair settings. Use a
  // canonical, unambiguous one-field suffix for the 012_20 contract.
  if (!p.settingsIdentity.empty() && p.settingsIdentity.back() != '|')
    p.settingsIdentity.push_back('|');
  p.settingsIdentity += "case=";
  p.settingsIdentity += which;
  require(p.settingsIdentity.size() <= 256,
          "012_23: case settings exceed identity budget");
  return p;
}
void matchNoiseComponent(const Result<ComJointVoltageEvidence>& component,
                         const ComScaledVoltageCase& scaled,
                         const Provenance& p, const char* which,
                         ComJointComponentKind kind) {
  require(component.usable() && component.value.has_value(),
          "012_23: missing/unusable independent noise input");
  const auto& a = component.provenance;
  require(a.sourceSha256 == p.sourceSha256 && a.revision == p.revision &&
          a.mappingIdentity == p.mappingIdentity &&
          a.settingsIdentity == p.settingsIdentity &&
          a.workspaceGeneration == p.workspaceGeneration &&
          a.mappingGeneration == p.mappingGeneration &&
          bounded(a.algorithm) && bounded(a.version),
          "012_23: noise component from another source, case or generation");
  const auto& v = *component.value;
  require(v.testCase == which && v.kind == kind &&
          v.referencePlane == scaled.receiverDecisionPlane &&
          v.calibrationIdentity == scaled.calibrationIdentity,
          "012_23: noise test/decision-plane/calibration mismatch");
  // The 012_20 kernel validates kinds, unique IDs, independence flags,
  // noise-ownership flags, nonnegative amplitudes, and double counting.
}
void validateNoise(const ComVictimIsiTailNoiseCase& noise,
                   const ComScaledVoltageCase& scaled,
                   const Provenance& p, const char* which) {
  require(bounded(noise.normalizationIdentity) &&
          noise.normalizationIdentity == scaled.normalizationIdentity &&
          bounded(noise.calibrationSourceIdentity) &&
          noise.calibrationSourceIdentity == scaled.calibrationSourceIdentity &&
          bounded(noise.txEqualizerIdentity) &&
          noise.txEqualizerIdentity == scaled.txEqualizerIdentity &&
          bounded(noise.rxEqualizerIdentity) &&
          noise.rxEqualizerIdentity == scaled.rxEqualizerIdentity,
          "012_23: missing or mismatched noise normalization/calibration/EQ");
  matchNoiseComponent(noise.receiverGaussian, scaled, p, which,
                      ComJointComponentKind::ReceiverGaussian);
  matchNoiseComponent(noise.randomJitterGaussian, scaled, p, which,
                      ComJointComponentKind::RandomJitterGaussian);
  matchNoiseComponent(noise.dualDiracPeak, scaled, p, which,
                      ComJointComponentKind::DualDiracPeak);
}
struct Tap {
  int uiOffset;
  double valueV;
};
double addOutwardBound(double old, double absV) {
  require(std::isfinite(old) && std::isfinite(absV) && absV > 0. &&
          old <= std::numeric_limits<double>::max() - absV,
          "012_23: omitted ISI upper bound overflow");
  const double sum = old + absV;
  // Outward-rounded addition: an ordinary FP addition could otherwise
  // round the allegedly conservative bound slightly downward.
  const double upward = std::nextafter(
      sum, std::numeric_limits<double>::infinity());
  require(std::isfinite(upward), "012_23: outward bound not representable");
  return upward;
}
ComVictimIsiTailCaseData calculateCase(
    const ComScaledVoltageCase& scaled,
    const ComVictimIsiCaseData& validated,
    const ComVictimIsiTailNoiseCase& noise,
    const Provenance& expectedCase, const char* which,
    double tailProbability, const std::atomic_bool* cancelled) {
  check(cancelled);
  validateNoise(noise, scaled, expectedCase, which);
  std::vector<Tap> taps;
  taps.reserve(12);
  for (size_t i = 0; i < scaled.equivalentPulseV.size(); ++i) {
    check(cancelled);
    if (i == kCursorIndex) continue; // Cursor is NEVER a noise tap.
    const double value = scaled.equivalentPulseV[i];
    if (std::fpclassify(value) != FP_ZERO)
      taps.push_back(Tap{static_cast<int>(i) - 3, value});
  }
  std::sort(taps.begin(), taps.end(), [](const Tap& a, const Tap& b) {
    const double aa = std::abs(a.valueV), bb = std::abs(b.valueV);
    if (aa != bb) return aa > bb;
    return a.uiOffset < b.uiOffset; // Stable deterministic tie-break.
  });
  ComJointTailInput mix;
  mix.receiverGaussian = noise.receiverGaussian;
  mix.randomJitterGaussian = noise.randomJitterGaussian;
  mix.dualDiracPeak = noise.dualDiracPeak;
  mix.oneSidedTailProbability = tailProbability;
  ComVictimIsiTailCaseData out;
  out.testCase = which;
  out.totalIsiRmsV = validated.totalIsiRmsV;
  out.nominalPam4HalfSpacingV = validated.nominalPam4HalfSpacingV;
  out.receiverDecisionPlane = scaled.receiverDecisionPlane;
  out.calibrationIdentity = scaled.calibrationIdentity;
  for (size_t i = 0; i < taps.size(); ++i) {
    check(cancelled);
    const Tap& tap = taps[i];
    if (i < kMaxExactTaps) {
      out.selectedUiOffsets.push_back(tap.uiOffset);
      Result<ComJointVoltageEvidence> r;
      r.status = Status::Valid;
      r.provenance = expectedCase;
      r.provenance.algorithm = "012_23-validated-012_21-victim-self-isi-tap";
      r.provenance.version = "012_23-1";
      ComJointVoltageEvidence evidence;
      evidence.amplitudeV = tap.valueV; // Preserve negative tap polarity.
      evidence.kind = ComJointComponentKind::IndependentPam4Tap;
      evidence.componentId = "victim-self-isi-ui-" +
                             std::to_string(tap.uiOffset);
      evidence.testCase = which;
      evidence.referencePlane = scaled.receiverDecisionPlane;
      evidence.calibrationIdentity = scaled.calibrationIdentity;
      evidence.assumedIndependent = true; // Mathematical assumption only.
      evidence.includesReceiverGaussianNoise = false;
      r.value = std::move(evidence);
      mix.pam4Taps.push_back(std::move(r));
    } else {
      out.omittedUiOffsets.push_back(tap.uiOffset);
      out.omittedTapBoundV =
          addOutwardBound(out.omittedTapBoundV, std::abs(tap.valueV));
    }
  }
  out.selectedTapCount = out.selectedUiOffsets.size();
  out.omittedTapCount = out.omittedUiOffsets.size();
  // The existing 012_20 mixture engine is the only numerical tail solver;
  // no new FFT, Gaussian CDF or state enumerator is introduced here.
  const auto selected =
      calculateComJointTail(mix, which, expectedCase, cancelled);
  require(selected.usable() && selected.status == Status::Warning &&
          selected.provenance.algorithm == "com-joint-tail-exploratory" &&
          selected.provenance.version == "012_20-1" &&
          !selected.diagnostics.empty() &&
          selected.diagnostics.front().code == "NOT_IEEE_COM",
          "012_23: upstream selected-mixture qualification missing");
  out.selectedTailQuantileV = selected.value->oneSidedNoiseQuantileV;
  out.verifiedSelectedTailProbability = selected.value->verifiedTailProbability;
  out.statesEvaluated = selected.value->statesEvaluated;
  require(std::isfinite(out.selectedTailQuantileV) &&
          out.selectedTailQuantileV >= 0. &&
          out.statesEvaluated > 0 && out.statesEvaluated <= 8192,
          "012_23: invalid selected mixture result");
  if (out.omittedTapCount == 0) {
    out.boundMode = "EXACT_ASSUMED_MIXTURE";
    out.lowerTailBoundV = out.upperTailBoundV = out.selectedTailQuantileV;
  } else {
    out.boundMode = "CONSERVATIVE_BOUNDED_MIXTURE";
    require(out.selectedTailQuantileV >=
            -std::numeric_limits<double>::max() + out.omittedTapBoundV &&
            out.selectedTailQuantileV <=
             std::numeric_limits<double>::max() - out.omittedTapBoundV,
            "012_23: unrepresentable quantile bounds");
    // q_selected +/- B is the *mathematical* bracketing theorem for exact
    // q_selected. Outward rounding prevents narrowing in floating arithmetic.
    // Numeric q_selected still inherits 012_20's finite solver tolerance.
    out.lowerTailBoundV = std::nextafter(
        out.selectedTailQuantileV - out.omittedTapBoundV,
        -std::numeric_limits<double>::infinity());
    out.upperTailBoundV = std::nextafter(
        out.selectedTailQuantileV + out.omittedTapBoundV,
         std::numeric_limits<double>::infinity());
    require(std::isfinite(out.lowerTailBoundV) &&
            std::isfinite(out.upperTailBoundV) &&
            out.lowerTailBoundV <= out.selectedTailQuantileV &&
            out.upperTailBoundV >= out.selectedTailQuantileV,
            "012_23: invalid outward-rounded interval");
  }
  check(cancelled);
  return out;
}
} // namespace

Result<ComVictimIsiTailCasesData> calculateComVictimIsiTailBridge(
    const ComVictimIsiTailInput& input, const Provenance& expectedPair,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  // Re-validate the *raw* signed 012_21 pulses through 012_22 rather than
  // trusting an externally supplied, possibly stale RMS summary.
  const auto isi = calculateComVictimIsi(input.scaled, expectedPair, cancelled);
  require(isi.usable() && isi.status == Status::Warning &&
          isi.provenance.algorithm == "com-victim-self-isi-exploratory" &&
          isi.provenance.version == "012_22-1",
          "012_23: validated upstream victim ISI unavailable");
  require(std::isfinite(input.oneSidedTailProbability) &&
          input.oneSidedTailProbability >= 1e-12 &&
          input.oneSidedTailProbability <= 0.1,
          "012_23: unsupported one-sided probability");
  const auto p1 = caseIdentity(expectedPair, "TEST1");
  const auto p2 = caseIdentity(expectedPair, "TEST2");
  ComVictimIsiTailCasesData data;
  data.test1 = calculateCase(input.scaled.value->test1,
                            isi.value->test1, input.test1, p1, "TEST1",
                            input.oneSidedTailProbability, cancelled);
  data.test2 = calculateCase(input.scaled.value->test2,
                            isi.value->test2, input.test2, p2, "TEST2",
                            input.oneSidedTailProbability, cancelled);
  data.worstCaseExploratoryUpperTailV =
      std::max(data.test1.upperTailBoundV, data.test2.upperTailBoundV);
  check(cancelled);
  Result<ComVictimIsiTailCasesData> result;
  result.status = Status::Warning;
  result.value = std::move(data);
  result.provenance = expectedPair;
  result.provenance.algorithm = "com-victim-self-isi-joint-tail-bridge-exploratory";
  result.provenance.version = "012_23-1";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Assumed independent PAM4 VICTIM_SELF_ISI with a bounded omitted-tap "
      "envelope and declared receiver-voltage calibration. Selected-mixture "
      "quantile has numerical tolerance; the interval is NOT a rigorously "
      "certified floating-point enclosure. Not aggressor crosstalk, "
      "Annex 93A DER, COM dB, FOM or PHY PASS/FAIL."});
  return result;
}
} // namespace si::channel
