#pragma once
#include "si/channel_analysis/com_fixed_noise.hpp"
#include <array>

namespace si::channel {
// Complete, independently declared synthetic inputs only. The currently
// available source replay is deliberately not eligible for this score.
struct ComFomCaseIdentity {
  std::string testCase, sourceIdentity, settingsIdentity, decisionPlane;
  bool operator==(const ComFomCaseIdentity&) const = default;
};
enum class ComFomNoiseKind {
  FullResidualIsi, FullReferenceJitter, FullCombinedCrosstalk, FullReferenceRx,
  PreDfeIsi, ObservedJitter, LocalJitter, ObservedCrosstalk, MeasuredBandRx,
  CombinedGaussian, IndividualAggressor
};
struct ComFomNoise {
  ComFomCaseIdentity identity;
  ComFomNoiseKind kind=ComFomNoiseKind::PreDfeIsi;
  double rmsV=std::numeric_limits<double>::quiet_NaN();
  // Unique ownership keys (one per underlying source, including aggregate
  // membership). A declaration is not evidence of physical calibration.
  std::vector<std::string> owners;
  std::string evidence;
  bool explicitlyInactive=false;
};
struct ComFixedFomInput {
  ComFomCaseIdentity identity;
  std::string cursorEvidence, txNoiseOwner, txNoiseEvidence, neInactiveEvidence;
  double cursorV=std::numeric_limits<double>::quiet_NaN();
  double rlm=std::numeric_limits<double>::quiet_NaN();
  double snrTxDb=std::numeric_limits<double>::quiet_NaN();
  unsigned levels=0;
  bool independentEquiprobablePam4Declared=false;
  bool c2cNonMmseNoRxFfeNoCalibrationNoC0Declared=false;
  // Ordered ISI, jitter, crosstalk, RX. TX is derived; ne must be inactive.
  std::array<std::optional<ComFomNoise>,4> noise;
};
struct ComFixedFomData {
  ComFomCaseIdentity identity;
  double signalV=0., totalRmsV=0., fomDb=0.;
  std::array<double,6> rmsV{};
  std::string model="SYNTHETIC_FIXED_EQ_FOM_NOT_IEEE_COM";
};
Result<ComFixedFomData> calculateComFixedFomSynthetic(
    const ComFixedFomInput& input, Control* control=nullptr);
// The 012_36 result is finite-span/local/uncalibrated. Reject rather than
// copying statistics to full-reference fields, even when all are present.
Result<ComFixedFomData> assessComFixedFomSource(
    const Result<ComFixedNoiseData>& replay, std::string_view testCase,
    Control* control=nullptr);
} // namespace si::channel
