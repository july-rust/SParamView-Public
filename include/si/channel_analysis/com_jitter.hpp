#pragma once
// 012_16: bounded NON-NORMATIVE local-linear timing-jitter receiver diagnostic.
// No IEEE Annex 93A COM/PDF/DER, DFE, NEXT/FEXT statistical aggregation or PHY verdict.
#include "si/channel_analysis/com_receiver_noise.hpp"
#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

struct ComJitterInput {
  double symbolRateBd = std::numeric_limits<double>::quiet_NaN();
  double randomJitterRmsUi = std::numeric_limits<double>::quiet_NaN();
  // Dual-Dirac atoms at +/-peak UI; peak is NOT peak-to-peak.
  double dualDiracPeakUi = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> timeSeconds;
  std::vector<double> receiverVoltageV; // externally calibrated real V, NOT unitSymbolPulse
  double decisionTimeSeconds = std::numeric_limits<double>::quiet_NaN();
  std::string voltageReferencePlane;
  std::string voltageCalibrationIdentity;
  // Must originate from 012_15, with matching SourceSnapshot provenance.
  Result<ComReceiverNoiseData> receiverNoise;
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> queryVoltageV;
};

struct ComJitterData {
  double unitIntervalSeconds = 0;
  double randomJitterRmsSeconds = 0;
  double dualDiracPeakSeconds = 0;
  double decisionSlopeVPerSecond = 0;
  double jitterGaussianRmsV = 0;
  double receiverNoiseRmsV = 0;
  double combinedGaussianRmsV = 0;
  double dualDiracVoltagePeakV = 0;
  double totalVoltageVarianceV2 = 0;
  double oneSidedQuantileV = std::numeric_limits<double>::quiet_NaN();
  double verifiedTailProbability = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> pdfPerV, cdf;
  bool hasContinuousDensity = false;
  std::string model = "EXPLORATORY_LOCAL_LINEAR_RJ_DUAL_DIRAC_RX_GAUSSIAN";
};

// A two-component equal-weight Gaussian mixture: 0.5 N(-A,sigma^2) +
// 0.5 N(+A,sigma^2), A=abs(slope)*dualDiracPeakSeconds,
// sigma=hypot(abs(slope)*randomJitterRmsSeconds, receiverNoiseRmsV).
// Time response is locally linear at decisionTimeSeconds, NOT the full
// nonlinear IEEE 93A jitter/ISI/NEXT/FEXT probability engine.
// Requires explicit waveform calibration and matching receiver-noise provenance;
// max 4097 time samples and 128 PDF/CDF voltage queries, no extrapolation.
Result<ComJitterData> calculateComJitter(
    const ComJitterInput& input, Provenance provenance,
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
