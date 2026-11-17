#pragma once
// 012_14: Non-normative per-aggressor, band-limited time-domain diagnostics.
// Already-conditioned complex NEXT/FEXT transfers only; no IEEE COM margin.
#include "si/channel_analysis/com_120f8.hpp"
#include <atomic>
#include <string>
#include <vector>

namespace si::channel {

struct ComInterferencePulse {
  std::string id;
  ComCoupling coupling = ComCoupling::Next;
  std::vector<double> unitSymbolPulse; // dimensionless; NOT calibrated voltage
  double maximumAbsolutePulse = 0;
};

struct ComInterferenceData {
  std::vector<double> timeSeconds;
  double symbolPeriodSeconds = 0;
  std::vector<ComInterferencePulse> aggressors;
  std::string model = "EXPLORATORY_BANDLIMITED_INDIVIDUAL_XTALK_PULSES";
};

// Input transfers MUST already have undergone source mapping, reference-plane,
// package and RX-conditioning checks upstream. Physical provenance cannot be
// reconstructed from this prepared spectrum.
Result<ComInterferenceData> calculateComInterferencePulses(
    const ComPreparedData& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

struct ComInterferenceTestCases {
  ComInterferenceData test1, test2;
};

// Typed pair retains distinct Test 1 and Test 2 pulse results; aggressor
// source identity and NEXT/FEXT coupling must agree between the two cases.
Result<ComInterferenceTestCases> calculateComInterferenceTestCases(
    const Com120F8LoadedTestCaseSpectra& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
