#pragma once
// 012_18: NON-NORMATIVE bounded 1-step local equalizer/ideal-DFE ISI search.
#include "si/channel_analysis/com_dfe.hpp"
#include "si/channel_analysis/com_physical_channel.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace si::channel {
struct ComEqualizerSearchCandidate {
  Com120F8EqualizerChoice equalizer;
  std::string settingsIdentity;
  std::array<double,6> dfeTaps{};
  double cursorAmplitudeUnitSymbol = 0;
  double precursorRss = 0, postcursorRss = 0, residualScore = 0;
};
struct ComEqualizerSearchData {
  std::string testCase;
  double decisionTimeSeconds = 0;
  ComEqualizerSearchCandidate initial, best;
  std::vector<ComEqualizerSearchCandidate> evaluatedCandidates;
  size_t attempted = 0, evaluated = 0, rejected = 0;
  std::vector<std::string> rejectionReasons;
  std::string model = "NON_NORMATIVE_ONE_STEP_120F8_ISI_LOCAL_SEARCH";
};
// Minimizes sum((a[k]-b[k])^2) independently at k=+1..+6,
// subject ONLY to final published DFE continuous min/max constraints.
// 13 finite normalized symbol samples, cursor at index 3 == 1.
// No receiver noise, error propagation, or IEEE COM optimum.
std::array<double,6> minimizeCom120F8IdealDfeResidual(
    const std::array<double,13>& normalized);

// Each call evaluates one explicitly selected Test1 or Test2 package.
// Requires the complete provenance-bearing Result of the established
// historical causal S4P physical engine. The source SHA/revision/mapping/
// generations and upstream algorithm must match caller provenance. No
// SourceSnapshot is inferred; live generation freshness is a caller duty.
// Exactly 13 UI sample times, the SAME decision time for all candidates.
// Fixed upper bound of 13 candidates and finite convolution-work budget.
// No normative COM, PHY/BER/DER verdict, global optimum, or GUI.
Result<ComEqualizerSearchData> searchCom120F8LocalEqualizer(
    const Result<ComPhysicalChannelData>& physical,
    const Com120F8EqualizerChoice& initial,
    const std::string& testCase, double decisionTimeSeconds,
    Provenance provenance, const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
