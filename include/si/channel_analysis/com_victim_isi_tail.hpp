#pragma once
// 012_23: bounded exploratory VICTIM SELF-ISI joint-tail bridge.
// No physical-voltage proof, Annex 93A COM, DER, FOM or PHY verdict.
#include "si/channel_analysis/com_joint_tail.hpp"
#include "si/channel_analysis/com_victim_isi.hpp"
#include <atomic>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

// Three independently supplied receiver-referenced noise components.
// No caller-supplied PAM4 taps: the bridge derives ALL self-ISI taps from
// validated 012_21 signed voltage pulses and 012_22 checks.
struct ComVictimIsiTailNoiseCase {
  Result<ComJointVoltageEvidence> receiverGaussian;
  Result<ComJointVoltageEvidence> randomJitterGaussian;
  Result<ComJointVoltageEvidence> dualDiracPeak;
  std::string normalizationIdentity;
  std::string calibrationSourceIdentity;
  std::string txEqualizerIdentity, rxEqualizerIdentity;
};

struct ComVictimIsiTailInput {
  Result<ComScaledVoltageCases> scaled;
  ComVictimIsiTailNoiseCase test1, test2;
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
};

struct ComVictimIsiTailCaseData {
  std::string testCase, componentRole = "VICTIM_SELF_ISI";
  std::string boundMode; // EXACT_ASSUMED_MIXTURE or CONSERVATIVE_BOUNDED_MIXTURE
  double selectedTailQuantileV = 0.;
  double omittedTapBoundV = 0.;
  double lowerTailBoundV = 0., upperTailBoundV = 0.;
  double verifiedSelectedTailProbability = 0.;
  double totalIsiRmsV = 0., nominalPam4HalfSpacingV = 0.;
  size_t selectedTapCount = 0, omittedTapCount = 0, statesEvaluated = 0;
  std::vector<int> selectedUiOffsets, omittedUiOffsets;
  std::string receiverDecisionPlane, calibrationIdentity;
};

struct ComVictimIsiTailCasesData {
  ComVictimIsiTailCaseData test1, test2;
  double worstCaseExploratoryUpperTailV = 0.;
  std::string model = "EXPLORATORY_TWO_CASE_VICTIM_SELF_ISI_TAIL_BOUNDS";
};

// expectedPair is an identity from the CURRENT live SourceSnapshot, not from
// an old cached result. Noise components must have canonical Test-specific
// settings: expectedPair.settingsIdentity + (trailing '|' if needed) + case=TESTx.
// The noise metadata must match each scaled case, including its independent
// Test-specific calibration-source identity. These are caller declarations;
// this function cannot verify physical calibration or independence.
// Throws Error for incompatible inputs and Cancelled for cancellation.
// Always Warning + NOT_IEEE_COM. Does NOT call/enable production calculateCom().
Result<ComVictimIsiTailCasesData> calculateComVictimIsiTailBridge(
    const ComVictimIsiTailInput& input, const Provenance& expectedPair,
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
