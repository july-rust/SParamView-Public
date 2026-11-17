#pragma once
// 012_15: bounded, non-normative band-limited Gaussian receiver-noise diagnostic.
// The caller must identify the one-sided PSD reference plane and the transfer
// for that noise source; never infer an IEEE 93A noise path from a THRU trace.
#include "si/channel_analysis/com_statistics.hpp"
#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

struct ComReceiverNoiseInput {
  double oneSidedPsdV2PerHz = std::numeric_limits<double>::quiet_NaN();
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> frequencyHz; // starts at DC; strictly increasing
  std::vector<Complex> noiseTransfer; // transfer from noise injection to receiver
  std::vector<double> queryVoltageV; // explicitly requested PDF/CDF sample points
};

struct ComReceiverNoiseData {
  double varianceV2 = 0;
  double rmsV = 0;
  double integrationStopHz = 0;
  double oneSidedQuantileV = std::numeric_limits<double>::quiet_NaN();
  double verifiedTailProbability = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> pdfPerV;
  std::vector<double> cdf;
  bool hasContinuousDensity = false;
  std::string model = "EXPLORATORY_GAUSSIAN_RECEIVER_NOISE";
};

// Integrates the finite measured band with a trapezoid rule, applies a
// zero-mean Gaussian distribution, and reuses the existing Gaussian-only
// quantile solver. Max 8193 frequencies and 256 requested voltages.
// No extrapolation, normative IEEE COM, jitter, crosstalk PDF or PHY verdict.
// When variance is exactly zero, a degenerate diagnostic is returned without
// a continuous PDF or positive-tail quantile.
Result<ComReceiverNoiseData> calculateComReceiverNoise(
    const ComReceiverNoiseInput& input,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
