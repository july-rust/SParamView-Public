#pragma once
// 012_25: bounded exploratory victim + NEXT/FEXT tail, NOT IEEE COM.
#include "si/channel_analysis/com_aggressor_voltage.hpp"
#include "si/channel_analysis/com_victim_isi_tail.hpp"
#include <atomic>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

enum class ComCombinedIndependenceModel {
  NotDeclared,
  IndependentUniformPam4 // An explicit mathematical assumption, never physical proof.
};
enum class ComCombinedTapRole { VictimSelfIsi, Next, Fext };

struct ComCombinedTailInput {
  // Raw 012_24 input: the bridge replays 012_22 and 012_24 validation.
  // A caller-supplied precomputed voltage summary is deliberately insufficient.
  ComAggressorVoltageInput voltage;
  ComVictimIsiTailNoiseCase test1, test2;
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
  ComCombinedIndependenceModel independenceModel =
      ComCombinedIndependenceModel::NotDeclared;
};

struct ComCombinedTapRecord {
  ComCombinedTapRole role = ComCombinedTapRole::VictimSelfIsi;
  std::string aggressorId; // empty for victim
  int uiOffset = 0;        // -3 ... +9; victim UI 0 is excluded
  double signedVoltageV = 0.;
};

struct ComCombinedTailCaseData {
  std::string testCase;
  std::string receiverDecisionPlane, calibrationIdentity;
  std::string boundMode; // NUMERICALLY_BRACKETED_ASSUMED_MIXTURE or CONSERVATIVE_NUMERICAL_BOUND
  double selectedTailQuantileV = 0.;
  // The selected subset's finite-precision quantile interval (012_26).
  double selectedNumericalLowerV = 0., selectedNumericalUpperV = 0.;
  double selectedLowerEndpointProbability = 0., selectedUpperEndpointProbability = 0.;
  double verifiedSelectedTailProbability = 0.;
  std::string numericalQualification = "NUMERICALLY_BRACKETED_NOT_CERTIFIED";
  double omittedTapBoundV = 0.;
  double lowerTailBoundV = 0., upperTailBoundV = 0.;
  size_t selectedTapCount = 0, omittedTapCount = 0, statesEvaluated = 0;
  std::vector<ComCombinedTapRecord> selectedTaps, omittedTaps;
};

struct ComCombinedTailCasesData {
  ComCombinedTailCaseData test1, test2;
  double worstCaseExploratoryUpperTailV = 0.;
  std::string model = "EXPLORATORY_VICTIM_NEXT_FEXT_BOUNDED_JOINT_TAIL";
};

// expectedPair and live must come from the CURRENT SourceSnapshot.
// Revalidates the original signed 012_21/012_24 input, not cached summaries.
// All usable results are Warning/NOT_IEEE_COM, never Annex 93A COM/PHY.
// Throws si::Error or si::Cancelled for invalid/stale/cancelled input.
Result<ComCombinedTailCasesData> calculateComCombinedTailBridge(
    const ComCombinedTailInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
