#pragma once
// 012_17: NON-NORMATIVE fixed six-tap ideal-decision DFE component.
// Requires already calculated rectangular SYMBOL pulse, not an impulse.
#include "si/channel_analysis/com_120f8.hpp"
#include <array>
#include <atomic>
#include <string>

namespace si::channel {
struct ComDfeInput {
  // Exactly 13 requested samples at -3..+9 UI around index 3 cursor.
  Result<Com120F8RectangularPulseData> symbolPulse;
  // Positive taps SUBTRACT earlier correctly decided symbol amplitudes.
  std::array<double,6> taps{};
  std::string testCase;  // exactly TEST1 or TEST2
};
struct ComDfeData {
  double cursorAmplitudeUnitSymbol = 0;
  double symbolPeriodSeconds = 0, decisionTimeSeconds = 0;
  std::array<double,13> normalizedBefore{}, normalizedAfter{};
  std::array<double,6> taps{};
  double precursorRssBefore = 0, precursorRssAfter = 0;
  double postcursorRssBefore = 0, postcursorRssAfter = 0;
  std::string testCase;
  std::string model =
    "EXPLORATORY_FIXED_TAP_IDEAL_DECISIONS_RECTANGULAR_SYMBOL_DFE";
};
// Published min/max only; no invented quantization step or optimization.
void validateCom120F8DfeTaps(const std::array<double,6>& taps);
// No statistical COM, error propagation, calibrated voltage, BER or PHY verdict.
// Invalid data -> si::Error; cancellation -> si::Cancelled.
Result<ComDfeData> calculateComDfe(const ComDfeInput& input,
    Provenance provenance, const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
