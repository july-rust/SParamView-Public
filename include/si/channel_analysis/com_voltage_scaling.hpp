#pragma once
// 012_21: EXPLORATORY unit-symbol pulse -> receiver-equivalent voltage scale.
// NOT physical Vout/Vin qualification, Annex 93A COM, or PHY PASS/FAIL.
#include "si/channel_analysis/com_120f8.hpp"
#include "si/channel_analysis/com_dfe.hpp"
#include <optional>
#include <string_view>
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

// 012_34: source-backed fixed decisions, with independent absolute times.
// Optional scale is explicitly caller-declared, never physical calibration.
struct ComFixedVictimCaseChoice {
  std::string testCase;
  double decisionTimeSeconds = std::numeric_limits<double>::quiet_NaN();
  std::array<double,6> dfeTaps{};
  std::optional<Result<ComVoltageScaleEvidence>> voltageScale;
};
struct ComObservedIsi {
  double precursorRss=0., postcursorRss=0., totalRss=0.;
  double pam4Rms=0., absoluteSumBound=0.;
};
struct ComFixedVictimVoltage {
  ComScaledVoltageCase before;
  std::array<double,13> residualV{};
  ComObservedIsi isiBeforeV, isiAfterV;
};
struct ComFixedVictimCaseData {
  std::string testCase;
  std::string settingsIdentity;
  double phaseUi=0.;
  Com120F8RectangularPulseData pulse;
  ComDfeData dfe;
  std::array<double,13> residualUnitSymbol{};
  ComObservedIsi isiBefore, isiAfter;
  std::optional<ComFixedVictimVoltage> voltage;
};
struct ComFixedVictimCases {
  ComFixedVictimCaseData test1, test2;
  std::string model="EXPLORATORY_SOURCE_FIXED_EQ_VICTIM_OBSERVED_ISI_DFE";
};
// SourceSnapshot is replayed, not a caller-supplied preconditioned pulse.
// Scale evidence, when supplied, must bind to exact 012_33 physical
// provenance and both EQ identities must equal that combined settings key.
// Caller must recheck actual live workspace state before presentation.
Result<ComFixedVictimCases> calculateComFixedVictimFromSource(
    const SourceSnapshot& source, std::string_view victimChannelId,
    const Com120F8EqualizerChoice& eq, const SourceIdentity& observedLiveIdentity,
    const ComFixedVictimCaseChoice& test1, const ComFixedVictimCaseChoice& test2,
    double maxAllowedModeConversion=0., Control* control=nullptr);

} // namespace si::channel
