#include "si/channel_analysis/com_cursor_tail_ratio.hpp"
#include "si/channel_analysis/com_voltage_ratio.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace si::channel {
namespace {
void require(bool ok, const char* reason) {
  if (!ok) throw si::Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool hasNotIeeeCom(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
      [](const Diagnostic& d) {
        return d.severity == Severity::Warning && d.code == "NOT_IEEE_COM";
      });
}
ComCursorTailRatioCaseData convert(
    const ComScaledVoltageCase& victim, const ComCombinedTailCaseData& noise,
    double probability, const char* which, const std::atomic_bool* cancelled) {
  check(cancelled);
  require(victim.testCase == which && noise.testCase == which &&
              victim.receiverDecisionPlane == noise.receiverDecisionPlane &&
              victim.calibrationIdentity == noise.calibrationIdentity,
          "012_28: mixed Test/decision-plane/calibration");
  require(std::isnormal(victim.cursorEquivalentV) &&
              std::isfinite(victim.cursorEquivalentV),
          "012_28: invalid or unresolvable PAM4 cursor");
  // The 012_22 derivation has been independently replayed by 012_24.
  const double magnitude = std::abs(victim.cursorEquivalentV);
  const double spacing = magnitude / 3.;
  require(std::isnormal(spacing) && spacing > 0.,
          "012_28: unrepresentable PAM4 half-spacing");
  require(noise.numericalQualification ==
              "NUMERICALLY_BRACKETED_NOT_CERTIFIED" &&
              (noise.boundMode == "NUMERICALLY_BRACKETED_ASSUMED_MIXTURE" ||
               noise.boundMode == "CONSERVATIVE_NUMERICAL_BOUND") &&
              std::isfinite(noise.selectedNumericalLowerV) &&
              std::isfinite(noise.selectedNumericalUpperV) &&
              noise.selectedNumericalLowerV < noise.selectedTailQuantileV &&
              noise.selectedTailQuantileV < noise.selectedNumericalUpperV &&
              noise.lowerTailBoundV <= noise.selectedNumericalLowerV &&
              noise.upperTailBoundV >= noise.selectedNumericalUpperV &&
              noise.lowerTailBoundV < noise.upperTailBoundV &&
              std::isnormal(noise.upperTailBoundV) &&
              noise.upperTailBoundV > 0. &&
              noise.selectedLowerEndpointProbability > probability &&
              noise.selectedUpperEndpointProbability < probability,
          "012_28: unqualified or nonpositive combined-tail upper bound");
  const double ratio = calculateDiagnosticVoltageRatioDb(
      spacing, noise.upperTailBoundV);
  require(std::isfinite(ratio), "012_28: nonfinite diagnostic voltage ratio");
  check(cancelled);
  ComCursorTailRatioCaseData out;
  out.testCase = which;
  out.receiverDecisionPlane = victim.receiverDecisionPlane;
  out.calibrationIdentity = victim.calibrationIdentity;
  out.cursorSignedV = victim.cursorEquivalentV;
  out.cursorMagnitudeV = magnitude;
  out.nominalPam4HalfSpacingV = spacing;
  out.noiseUpperTailV = noise.upperTailBoundV;
  out.exploratoryLowerRatioDb = ratio;
  out.oneSidedTailProbability = probability;
  out.numericalQualification = noise.numericalQualification;
  out.boundMode = noise.boundMode;
  return out;
}
} // namespace

Result<ComCursorTailRatioCasesData> calculateComCursorTailRatioBridge(
    const ComCombinedTailInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled) {
  check(cancelled);
  // Full upstream replay verifies live source/generation, independent
  // receiver-voltage scaling, signed victim samples, aggressors and noise.
  const auto combined =
      calculateComCombinedTailBridge(input, expectedPair, live, cancelled);
  require(combined.usable() && combined.status == Status::Warning &&
              combined.provenance.algorithm ==
                  "com-victim-aggressor-bounded-joint-tail-exploratory" &&
              combined.provenance.version == "012_26-1" &&
              hasNotIeeeCom(combined.diagnostics),
          "012_28: unqualified combined-tail upstream");
  require(input.voltage.victim.usable(),
          "012_28: missing validated victim voltage");
  const auto& victim = *input.voltage.victim.value;
  const auto& bound = *combined.value;
  require(victim.test1.receiverDecisionPlane ==
              victim.test2.receiverDecisionPlane &&
              victim.test1.calibrationIdentity ==
              victim.test2.calibrationIdentity &&
              std::isfinite(input.oneSidedTailProbability) &&
              input.oneSidedTailProbability >= 1e-12 &&
              input.oneSidedTailProbability <= 0.1,
          "012_28: incompatible Test1/Test2 voltage or probability");
  ComCursorTailRatioCasesData data;
  data.test1 = convert(victim.test1, bound.test1,
                       input.oneSidedTailProbability, "TEST1", cancelled);
  data.test2 = convert(victim.test2, bound.test2,
                       input.oneSidedTailProbability, "TEST2", cancelled);
  data.minimumExploratoryRatioDb =
      std::min(data.test1.exploratoryLowerRatioDb,
               data.test2.exploratoryLowerRatioDb);
  check(cancelled);
  Result<ComCursorTailRatioCasesData> out;
  out.status = Status::Warning;
  out.value = std::move(data);
  out.provenance = expectedPair;
  out.provenance.algorithm = "com-cursor-tail-ratio-exploratory";
  out.provenance.version = "012_28-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Nominal PAM4 half-spacing / assumed-mixture upper tail only. "
      "Numerical brackets are NOT certified enclosures; TX/RX voltage "
      "calibration, physical independence and target-DER relation are "
      "NOT verified. Not IEEE Annex 93A COM(dB), DER/FOM or PHY PASS/FAIL."});
  return out;
}
} // namespace si::channel
