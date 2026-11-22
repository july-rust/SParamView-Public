#pragma once
// 012_22: Exploratory receiver-equivalent VICTIM SELF-ISI statistics only.
// Not physical voltage calibration, Annex 93A COM/DER, or PHY PASS/FAIL.
#include "si/channel_analysis/com_voltage_scaling.hpp"
#include <atomic>
#include <string>

namespace si::channel {

struct ComVictimIsiCaseData {
  std::string testCase;
  double cursorSignedV = 0., cursorMagnitudeV = 0.;
  double precursorRmsV = 0., postcursorRmsV = 0., totalIsiRmsV = 0.;
  double worstCaseIsiBoundV = 0., nominalPam4HalfSpacingV = 0.;
  std::string transmitterReferencePlane, receiverDecisionPlane;
  std::string normalizationIdentity, calibrationIdentity;
  std::string calibrationSourceIdentity;
  std::string txEqualizerIdentity, rxEqualizerIdentity;
  std::string componentRole = "VICTIM_SELF_ISI";
  std::string model = "EXPLORATORY_RECEIVER_VOLTAGE_VICTIM_ISI";
};

struct ComVictimIsiCasesData {
  ComVictimIsiCaseData test1, test2;
  std::string model = "EXPLORATORY_TWO_CASE_RECEIVER_VOLTAGE_VICTIM_ISI";
};

// Exactly two already-scaled 13-tap cases: -3..+9 UI, cursor at index 3.
// The RMS assumes INDEPENDENT, equiprobable zero-mean PAM4 symbols with
// levels {-1,-1/3,+1/3,+1} (symbol variance 5/9). No such physical
// independence is certified. Bound=sum(abs(non-cursor taps)) is deterministic.
// Expected must be derived from the CURRENT live source by the caller;
// comparing old Results alone cannot certify live generation freshness.
// Throws si::Error or si::Cancelled. Always Warning/NOT_IEEE_COM on success.
Result<ComVictimIsiCasesData> calculateComVictimIsi(
    const Result<ComScaledVoltageCases>& scaled,
    const Provenance& expected,
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
