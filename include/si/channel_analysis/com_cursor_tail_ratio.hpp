#pragma once
// 012_28: exploratory cursor/combined-tail voltage-ratio bridge; NOT IEEE COM.
#include "si/channel_analysis/com_combined_tail.hpp"
#include <atomic>
#include <optional>
#include <string>

namespace si::channel {

struct ComCursorTailRatioCaseData {
  std::string testCase, receiverDecisionPlane, calibrationIdentity;
  double cursorSignedV = 0.;
  double cursorMagnitudeV = 0.;
  double nominalPam4HalfSpacingV = 0.;
  double noiseUpperTailV = 0.;
  // Raw assumed-mixture voltage lower bound may be nonpositive due to
  // conservative omitted taps; never treat it as positive without checking.
  double noiseLowerTailBoundV = 0.;
  double exploratoryLowerRatioDb = 0.;
  // Absent when N_lower <= 0. Never substitute infinity, zero, or DBL_MAX.
  std::optional<double> exploratoryUpperRatioDb;
  double oneSidedTailProbability = 0.;
  std::string numericalQualification, boundMode;
};

struct ComCursorTailRatioCasesData {
  ComCursorTailRatioCaseData test1, test2;
  double minimumExploratoryRatioDb = 0.;
  // min(Test1,Test2) upper bound exists when at least one Test upper
  // bound is finite, even if the other Test is one-sided only.
  std::optional<double> minimumExploratoryRatioUpperDb;
  std::string model = "EXPLORATORY_CURSOR_TAIL_RATIO_NOT_IEEE_COM";
};

// Replays the original 012_21->012_22->012_24->012_25/26 source checks;
// do not accept caller-fabricated precomputed results. The ratio uses a
// model-conditional numerical upper noise quantile and (when positive) lower
// noise bound; their dB images are NOT certified DER/COM enclosures,
// IEEE COM, or a PHY PASS/FAIL result.
// All usable results are Warning/NOT_IEEE_COM; throws Error or Cancelled.
Result<ComCursorTailRatioCasesData> calculateComCursorTailRatioBridge(
    const ComCombinedTailInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
