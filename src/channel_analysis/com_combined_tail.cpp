#include "si/channel_analysis/com_combined_tail.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace si::channel {
namespace {
constexpr size_t kCursor = 3;
constexpr size_t kSamples = 13;
constexpr size_t kMaxTaps = 6;
constexpr size_t kMaxCandidates = 12 + 32 * 13;

void require(bool ok, const char* why) {
  if (!ok) throw si::Error(why);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool bounded(const std::string& v) {
  return !v.empty() && v.size() <= 256;
}
Provenance caseIdentity(const Provenance& pair, const char* which) {
  Provenance p = pair;
  if (!p.settingsIdentity.empty() && p.settingsIdentity.back() != '|')
    p.settingsIdentity.push_back('|');
  p.settingsIdentity += "case=";
  p.settingsIdentity += which;
  require(p.settingsIdentity.size() <= 256,
          "012_25: Test identity exceeds settings budget");
  return p;
}
void validateNoiseComponent(const Result<ComJointVoltageEvidence>& r,
                            const ComScaledVoltageCase& scaled,
                            const Provenance& expected, const char* which,
                            ComJointComponentKind kind) {
  require(r.usable() && r.value.has_value(),
          "012_25: missing or unusable noise component");
  const auto& p = r.provenance;
  require(p.sourceSha256 == expected.sourceSha256 &&
          p.revision == expected.revision &&
          p.mappingIdentity == expected.mappingIdentity &&
          p.settingsIdentity == expected.settingsIdentity &&
          p.workspaceGeneration == expected.workspaceGeneration &&
          p.mappingGeneration == expected.mappingGeneration &&
          bounded(p.algorithm) && bounded(p.version),
          "012_25: noise source, Test or generation mismatch");
  const auto& v = *r.value;
  require(v.testCase == which && v.kind == kind &&
          v.referencePlane == scaled.receiverDecisionPlane &&
          v.calibrationIdentity == scaled.calibrationIdentity,
          "012_25: noise Test, plane or calibration mismatch");
  // Remaining ownership, independence, values and IDs are checked by 012_20.
}
void validateNoise(const ComVictimIsiTailNoiseCase& noise,
                   const ComScaledVoltageCase& scaled,
                   const Provenance& expected, const char* which) {
  require(bounded(noise.normalizationIdentity) &&
          noise.normalizationIdentity == scaled.normalizationIdentity &&
          bounded(noise.calibrationSourceIdentity) &&
          noise.calibrationSourceIdentity == scaled.calibrationSourceIdentity &&
          bounded(noise.txEqualizerIdentity) &&
          noise.txEqualizerIdentity == scaled.txEqualizerIdentity &&
          bounded(noise.rxEqualizerIdentity) &&
          noise.rxEqualizerIdentity == scaled.rxEqualizerIdentity,
          "012_25: normalization, calibration source or EQ mismatch");
  validateNoiseComponent(noise.receiverGaussian, scaled, expected, which,
                         ComJointComponentKind::ReceiverGaussian);
  validateNoiseComponent(noise.randomJitterGaussian, scaled, expected, which,
                         ComJointComponentKind::RandomJitterGaussian);
  validateNoiseComponent(noise.dualDiracPeak, scaled, expected, which,
                         ComJointComponentKind::DualDiracPeak);
}
bool validVoltage(double v) {
  return std::fpclassify(v) == FP_ZERO || std::isnormal(v);
}
void addTap(std::vector<ComCombinedTapRecord>& taps, ComCombinedTapRole role,
            const std::string& aggressorId, int uiOffset, double voltage) {
  require(validVoltage(voltage), "012_25: invalid or unresolvable tap voltage");
  if (std::fpclassify(voltage) != FP_ZERO)
    taps.push_back({role, aggressorId, uiOffset, voltage});
}
double addOutward(double previous, double magnitude) {
  require(std::isfinite(previous) && std::isfinite(magnitude) &&
          magnitude > 0. &&
          previous <= std::numeric_limits<double>::max() - magnitude,
          "012_25: omitted-tail bound overflow");
  const double rounded = previous + magnitude;
  const double outward =
      std::nextafter(rounded, std::numeric_limits<double>::infinity());
  require(std::isfinite(outward), "012_25: bound not representable");
  return outward;
}
bool tapOrder(const ComCombinedTapRecord& a, const ComCombinedTapRecord& b) {
  const double aa = std::abs(a.signedVoltageV);
  const double bb = std::abs(b.signedVoltageV);
  if (aa > bb) return true;
  if (aa < bb) return false;
  if (a.role != b.role)
    return static_cast<int>(a.role) < static_cast<int>(b.role);
  if (a.aggressorId != b.aggressorId)
    return a.aggressorId < b.aggressorId;
  return a.uiOffset < b.uiOffset;
}
ComCombinedTailCaseData calculateCase(
    const ComScaledVoltageCase& victim,
    const ComAggressorVoltageCaseData& interference,
    const ComVictimIsiTailNoiseCase& noise,
    const Provenance& expectedCase, const char* which, double p,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  require(victim.testCase == which && interference.testCase == which &&
          victim.receiverDecisionPlane == interference.receiverDecisionPlane,
          "012_25: incompatible Test case or receiver plane");
  validateNoise(noise, victim, expectedCase, which);
  std::vector<ComCombinedTapRecord> taps;
  taps.reserve(kMaxCandidates);
  for (size_t i = 0; i < kSamples; ++i) {
    check(cancelled);
    if (i != kCursor)
      addTap(taps, ComCombinedTapRole::VictimSelfIsi, "",
             static_cast<int>(i) - static_cast<int>(kCursor),
             victim.equivalentPulseV[i]);
  }
  require(interference.aggressors.size() <= 32,
          "012_25: aggressor budget exceeded");
  for (const auto& aggressor : interference.aggressors) {
    check(cancelled);
    require(aggressor.testCase == which &&
            aggressor.receiverDecisionPlane == victim.receiverDecisionPlane &&
            aggressor.calibrationIdentity == victim.calibrationIdentity &&
            aggressor.normalizationIdentity == victim.normalizationIdentity &&
            aggressor.rxEqualizerIdentity == victim.rxEqualizerIdentity &&
            bounded(aggressor.calibrationSourceIdentity) &&
            bounded(aggressor.txEqualizerIdentity) &&
            bounded(aggressor.transmitterReferencePlane),
            "012_25: incompatible aggressor voltage evidence");
    require(aggressor.coupling == ComCoupling::Next ||
            aggressor.coupling == ComCoupling::Fext,
            "012_25: unsupported aggressor coupling");
    const auto role = aggressor.coupling == ComCoupling::Next
                          ? ComCombinedTapRole::Next
                          : ComCombinedTapRole::Fext;
    for (size_t i = 0; i < kSamples; ++i) {
      check(cancelled);
      // Aggressor UI 0 IS interference: it must not be removed as a cursor.
      addTap(taps, role, aggressor.id,
             static_cast<int>(i) - static_cast<int>(kCursor),
             aggressor.equivalentPulseV[i]);
    }
  }
  require(taps.size() <= kMaxCandidates,
          "012_25: combined tap budget exceeded");
  std::sort(taps.begin(), taps.end(), tapOrder);

  ComJointTailInput joint;
  joint.receiverGaussian = noise.receiverGaussian;
  joint.randomJitterGaussian = noise.randomJitterGaussian;
  joint.dualDiracPeak = noise.dualDiracPeak;
  joint.oneSidedTailProbability = p;
  ComCombinedTailCaseData out;
  out.testCase = which;
  out.receiverDecisionPlane = victim.receiverDecisionPlane;
  out.calibrationIdentity = victim.calibrationIdentity;
  for (const auto& tap : taps) {
    check(cancelled);
    if (out.selectedTaps.size() < kMaxTaps) {
      Result<ComJointVoltageEvidence> r;
      r.status = Status::Valid;
      r.provenance = expectedCase;
      r.provenance.algorithm = "012_25-revalidated-victim-aggressor-tap";
      r.provenance.version = "012_25-1";
      ComJointVoltageEvidence e;
      e.amplitudeV = tap.signedVoltageV; // Never rectify PAM4 tap polarity.
      e.kind = ComJointComponentKind::IndependentPam4Tap;
      // Short, deterministic and unique even when upstream IDs are 256 chars.
      e.componentId =
          "combined-selected-tap-" + std::to_string(out.selectedTaps.size());
      e.testCase = which;
      e.referencePlane = victim.receiverDecisionPlane;
      e.calibrationIdentity = victim.calibrationIdentity;
      e.assumedIndependent = true; // Explicit input model, not physical proof.
      e.includesReceiverGaussianNoise = false;
      r.value = std::move(e);
      joint.pam4Taps.push_back(std::move(r));
      out.selectedTaps.push_back(tap);
    } else {
      out.omittedTapBoundV =
          addOutward(out.omittedTapBoundV, std::abs(tap.signedVoltageV));
      out.omittedTaps.push_back(tap);
    }
  }
  out.selectedTapCount = out.selectedTaps.size();
  out.omittedTapCount = out.omittedTaps.size();
  // Reuse the ONLY 012_20 statistical solver: no duplicate enumeration/erfc.
  const auto tail = calculateComJointTail(joint, which, expectedCase, cancelled);
  require(tail.usable() && tail.status == Status::Warning &&
          tail.provenance.algorithm == "com-joint-tail-exploratory" &&
          tail.provenance.version == "012_20-1" &&
          !tail.diagnostics.empty() &&
          tail.diagnostics.front().code == "NOT_IEEE_COM",
          "012_25: unqualified joint-tail output");
  out.selectedTailQuantileV = tail.value->oneSidedNoiseQuantileV;
  out.verifiedSelectedTailProbability = tail.value->verifiedTailProbability;
  out.statesEvaluated = tail.value->statesEvaluated;
  require(std::isfinite(out.selectedTailQuantileV) &&
          out.selectedTailQuantileV >= 0. &&
          out.statesEvaluated > 0 && out.statesEvaluated <= 8192,
          "012_25: unqualified selected quantile or state count");
  if (out.omittedTapCount == 0) {
    out.boundMode = "EXACT_ASSUMED_MIXTURE";
    out.lowerTailBoundV = out.upperTailBoundV = out.selectedTailQuantileV;
  } else {
    out.boundMode = "CONSERVATIVE_BOUNDED_MIXTURE";
    require(out.selectedTailQuantileV <=
            std::numeric_limits<double>::max() - out.omittedTapBoundV,
            "012_25: tail upper bound overflow");
    const double lower = out.selectedTailQuantileV - out.omittedTapBoundV;
    const double upper = out.selectedTailQuantileV + out.omittedTapBoundV;
    out.lowerTailBoundV =
        std::nextafter(lower, -std::numeric_limits<double>::infinity());
    out.upperTailBoundV =
        std::nextafter(upper, std::numeric_limits<double>::infinity());
    require(std::isfinite(out.lowerTailBoundV) &&
            std::isfinite(out.upperTailBoundV) &&
            out.lowerTailBoundV <= out.selectedTailQuantileV &&
            out.upperTailBoundV >= out.selectedTailQuantileV,
            "012_25: invalid outward-rounded interval");
  }
  check(cancelled);
  return out;
}
} // namespace

Result<ComCombinedTailCasesData> calculateComCombinedTailBridge(
    const ComCombinedTailInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled) {
  check(cancelled);
  require(input.independenceModel ==
              ComCombinedIndependenceModel::IndependentUniformPam4,
          "012_25: combined independence model not explicitly selected");
  require(std::isfinite(input.oneSidedTailProbability) &&
          input.oneSidedTailProbability >= 1e-12 &&
          input.oneSidedTailProbability <= 0.1,
          "012_25: unsupported one-sided tail probability");
  // Strict replay: 012_24 independently reruns 012_22 checks and verifies live.
  const auto voltages = calculateComAggressorReceiverVoltage(
      input.voltage, expectedPair, live, cancelled);
  require(voltages.usable() && voltages.status == Status::Warning &&
          voltages.provenance.algorithm ==
              "com-aggressor-receiver-voltage-exploratory" &&
          voltages.provenance.version == "012_24-1" &&
          !voltages.diagnostics.empty() &&
          voltages.diagnostics.front().code == "NOT_IEEE_COM",
          "012_25: unqualified 012_24 upstream");
  const auto& victim = *input.voltage.victim.value;
  const auto& aggressor = *voltages.value;
  require(victim.test1.receiverDecisionPlane == victim.test2.receiverDecisionPlane &&
          victim.test1.calibrationIdentity == victim.test2.calibrationIdentity,
          "012_25: Test1/Test2 receiver plane or calibration mismatch");
  const auto p1 = caseIdentity(expectedPair, "TEST1");
  const auto p2 = caseIdentity(expectedPair, "TEST2");
  ComCombinedTailCasesData data;
  data.test1 = calculateCase(victim.test1, aggressor.test1, input.test1, p1,
                             "TEST1", input.oneSidedTailProbability, cancelled);
  data.test2 = calculateCase(victim.test2, aggressor.test2, input.test2, p2,
                             "TEST2", input.oneSidedTailProbability, cancelled);
  data.worstCaseExploratoryUpperTailV =
      std::max(data.test1.upperTailBoundV, data.test2.upperTailBoundV);
  check(cancelled);
  Result<ComCombinedTailCasesData> result;
  result.status = Status::Warning;
  result.value = std::move(data);
  result.provenance = expectedPair;
  result.provenance.algorithm =
      "com-victim-aggressor-bounded-joint-tail-exploratory";
  result.provenance.version = "012_25-1";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Assumed independent uniform PAM4 victim self-ISI and NEXT/FEXT taps; "
      "omitted voltage terms receive an outward-rounded magnitude envelope. "
      "Caller-supplied TX/voltage calibration and physical independence are "
      "NOT certified. The selected numerical quantile has solver tolerance: "
      "bounds are not rigorous floating-point enclosures. Not Annex 93A "
      "COM, DER/FOM, BER, PHY PASS/FAIL or a production verdict."});
  return result;
}
} // namespace si::channel
