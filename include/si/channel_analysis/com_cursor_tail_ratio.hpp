#pragma once
// 012_28: exploratory cursor/combined-tail voltage-ratio bridge; NOT IEEE COM.
#include "si/channel_analysis/com_combined_tail.hpp"
#include <atomic>
#include <string>

namespace si::channel {

struct ComCursorTailRatioCaseData {
  std::string testCase, receiverDecisionPlane, calibrationIdentity;
  double cursorSignedV = 0.;
  double cursorMagnitudeV = 0.;
  double nominalPam4HalfSpacingV = 0.;
  double noiseUpperTailV = 0.;
  double exploratoryLowerRatioDb = 0.;
  double oneSidedTailProbability = 0.;
  std::string numericalQualification, boundMode;
};

struct ComCursorTailRatioCasesData {
  ComCursorTailRatioCaseData test1, test2;
  double minimumExploratoryRatioDb = 0.;
  std::string model = "EXPLORATORY_CURSOR_TAIL_RATIO_NOT_IEEE_COM";
};

// Replays the original 012_21->012_22->012_24->012_25/26 source checks;
// do not accept caller-fabricated precomputed results. The ratio uses a
// model-conditional numerical upper noise quantile, NOT a certified DER
// threshold, IEEE COM, or a PHY PASS/FAIL result.
// All usable results are Warning/NOT_IEEE_COM; throws Error or Cancelled.
Result<ComCursorTailRatioCasesData> calculateComCursorTailRatioBridge(
    const ComCombinedTailInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
