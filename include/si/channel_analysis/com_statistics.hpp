#pragma once
// 012_09: exact finite-state PAM4 + Gaussian one-sided noise tail solver.
// A self-contained mathematical primitive for future COM receiver statistics;
// NOT the IEEE 802.3ck Annex 93A probability model / numerical COM solver.
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <limits>
#include <vector>

namespace si::channel {

// A PAM4 interferer tap uses independently equiprobable normalized levels
// {-1, -1/3, +1/3, +1}. The coefficient is the voltage magnitude at level +1.
// This simplified discrete+Gaussian model is NOT final IEEE 93A noise PDF.
struct ComStatisticalNoiseInput {
  double gaussianRmsV = std::numeric_limits<double>::quiet_NaN();
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> pam4TapV;
};

struct ComStatisticalNoiseData {
  double oneSidedNoiseQuantileV = 0;
  double analyticGaussianVarianceV2 = 0;
  double analyticTotalVarianceV2 = 0;
  double verifiedTailProbability = 0;
  double worstCaseDiscreteShiftV = 0;
  size_t statesEvaluated = 0;
  bool exactFiniteState = false;
  // This cannot be used as IEEE COM noise without qualified model mapping.
  const char *model = "EXPLORATORY_INDEPENDENT_PAM4_GAUSSIAN_MIXTURE";
};

// Deterministic no-random-number exact sum of Gaussian tails over up to
// eight PAM4 taps (4^8 = 65536 states), bisection on the one-sided quantile.
// No truncation/clipping or Gaussian-RMS replacement of discrete ISI tails.
// Reports the full total variance for analytic consistency checks.
Result<ComStatisticalNoiseData> calculatePam4StatisticalNoise(
    const ComStatisticalNoiseInput &input,
    Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);

} // namespace si::channel
