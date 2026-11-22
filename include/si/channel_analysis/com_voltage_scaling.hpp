#pragma once
// 012_21: EXPLORATORY unit-symbol pulse -> receiver-equivalent voltage scale.
// NOT physical Vout/Vin qualification, Annex 93A COM, or PHY PASS/FAIL.
#include "si/channel_analysis/com_120f8.hpp"
#include <array>
#include <atomic>
#include <limits>
#include <string>

namespace si::channel {
struct ComVoltageScaleEvidence {
  // Caller-declared VOLTS per one signed dimensionless unit-symbol response.
  // This does not turn a matched-wave Sdd21 into a measured voltage gain.
  double voltsPerUnitSymbol = std::numeric_limits<double>::quiet_NaN();
  std::string testCase;                // Exactly TEST1 or TEST2.
  std::string transmitterReferencePlane, receiverDecisionPlane;
  std::string normalizationIdentity, calibrationIdentity;
  std::string calibrationSourceIdentity; // May differ from channel source SHA.
  std::string txEqualizerIdentity, rxEqualizerIdentity;
};

struct ComVoltageScalingInput {
  Result<Com120F8RectangularPulseCases> pulses;
  Result<ComVoltageScaleEvidence> test1Scale, test2Scale;
};

struct ComScaledVoltageCase {
  std::string testCase;
  std::array<double,13> timeSeconds{}, unitSymbolPulse{}, equivalentPulseV{};
  double voltsPerUnitSymbol = 0., cursorEquivalentV = 0.;
  double maximumAbsoluteEquivalentV = 0.;
  std::string transmitterReferencePlane, receiverDecisionPlane;
  std::string normalizationIdentity, calibrationIdentity;
  std::string calibrationSourceIdentity;
  std::string txEqualizerIdentity, rxEqualizerIdentity;
};

struct ComScaledVoltageCases {
  ComScaledVoltageCase test1, test2;
  std::string model = "EXPLORATORY_RECEIVER_EQUIVALENT_UNIT_SYMBOL_VOLTAGE";
};

// One bounded 2x13 multiplication with case isolation and provenance checks.
// Valid input still returns Warning/NOT_IEEE_COM: self-described calibration
// provenance cannot prove physical correctness or statistical independence.
// Throws si::Error or si::Cancelled; does not feed COM or Joint Tail.
Result<ComScaledVoltageCases> scaleCom120F8ReceiverEquivalentVoltage(
    const ComVoltageScalingInput& input, const Provenance& expected,
    const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
