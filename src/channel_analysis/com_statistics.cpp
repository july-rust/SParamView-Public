#include "si/channel_analysis/com_statistics.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

namespace si::channel {
namespace {
constexpr double sqrt2 = 1.4142135623730950488;
constexpr double maxReasonableTail = 0.1;
constexpr double minReasonableTail = 1e-12;
constexpr size_t maxTapCount = 8;
constexpr size_t maxStateCount = 1u << 16;
constexpr double normalizedPam4[4] = {-1., -1./3., +1./3., +1.};
void require(bool ok, const char *message) {
  if(!ok)throw Error(message);
}
void check(const std::atomic_bool *cancelled) {
  if(cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
double gaussianTail(double threshold, double sigma) {
  const double z=threshold/(sqrt2*sigma);
  return 0.5 * std::erfc(z);
}
}
Result<ComStatisticalNoiseData> calculatePam4StatisticalNoise(
    const ComStatisticalNoiseInput &input, Provenance provenance,
    const std::atomic_bool *cancelled) {
  check(cancelled);
  const double sigma=input.gaussianRmsV;
  const double p=input.oneSidedTailProbability;
  require(std::isfinite(sigma) && sigma>0,
          "Positive finite Gaussian noise RMS is required");
  require(std::isfinite(p) && p>=minReasonableTail &&
          p<=maxReasonableTail,
          "Target one-sided noise tail probability is outside qualified range");
  require(input.pam4TapV.size()<=maxTapCount,
          "Exact finite-state model allows no more than eight PAM4 taps");
  double shiftMax=0;
  double variance=sigma*sigma;
  require(std::isfinite(variance) && variance>0,
          "Nonfinite Gaussian noise variance");
  for(double coefficient:input.pam4TapV) {
    require(std::isfinite(coefficient),
            "Nonfinite discrete PAM4 interference coefficient");
    shiftMax+=std::abs(coefficient);
    // independent PAM4 level distribution: E[A]=0, E[A^2]=5/9.
    variance+=(5./9.)*coefficient*coefficient;
    require(std::isfinite(shiftMax) && std::isfinite(variance),
            "Overflow in PAM4 interference variance or bound");
  }
  require(std::isfinite(shiftMax+12*sigma),
          "Overflow in finite-state Gaussian tail search bound");
  std::vector<double> shifts{0.};
  // Exact enumeration with no approximation or statistical randomization.
  for(double tap:input.pam4TapV) {
    check(cancelled);
    require(shifts.size()<=maxStateCount/4,
            "Exact PAM4 state memory budget exceeded");
    std::vector<double> expanded;
    expanded.reserve(shifts.size()*4);
    for(double prior:shifts)
      for(double level:normalizedPam4) {
        const double next=prior+tap*level;
        require(std::isfinite(next), "Finite-state PAM4 shift overflow");
        expanded.push_back(next);
      }
    shifts=std::move(expanded);
  }
  require(!shifts.empty() && shifts.size()<=maxStateCount,
          "Invalid exact PAM4 probability state count");
  // Work in units of Gaussian RMS. A fixed absolute voltage convergence
  // floor wrongly rejects valid nV/pV-scale inputs and can accept a false
  // quantile at 1e-12 tails. Normalize *before* root finding.
  for(double &shift:shifts) {
    shift /= sigma;
    require(std::isfinite(shift) && std::abs(shift) < 1e10,
            "PAM4/Gaussian voltage dynamic range cannot be resolved");
  }
  const double scaledShiftMax=shiftMax/sigma;
  require(std::isfinite(scaledShiftMax) && scaledShiftMax < 1e10,
          "Unresolvable PAM4/Gaussian relative voltage scale");
  const double invStates=1.0/double(shifts.size());
  auto tail=[&](double normalizedQ) -> double {
    // Kahan compensated sum avoids large state-dependent rounding error,
    // especially when the one-sided tail is in the 1e-12..1e-5 range.
    double sum=0,correction=0;
    for(size_t i=0;i<shifts.size();++i) {
      if(!(i&4095u))check(cancelled);
      const double term=gaussianTail(normalizedQ-shifts[i],1.0);
      require(std::isfinite(term) && term>=0 && term<=1,
              "Nonfinite Gaussian tail contribution");
      const double adjusted=term-correction;
      const double accum=sum+adjusted;
      correction=(accum-sum)-adjusted;
      sum=accum;
    }
    const double value=sum*invStates;
    require(std::isfinite(value) && value>=0 && value<=1,
            "Invalid normalized noise tail probability");
    return value;
  };
  // Symmetric mixture and p<=0.1 means the positive one-sided quantile
  // is nonnegative; the high bound covers erfc tail at >=12 sigma past
  // the largest finite PAM4 interference shift.
  double low=0,high=scaledShiftMax+12.;
  require(std::isfinite(high) && tail(low)>p && tail(high)<p,
          "Noise quantile root is not bracketed");
  for(int iter=0;iter<120;++iter) {
    check(cancelled);
    const double center=low+(high-low)*0.5;
    if(center==low || center==high)break;
    const double prob=tail(center);
    if(prob>p)low=center;
    else high=center;
  }
  const double normalizedThreshold=low+(high-low)*0.5;
  const double actual=tail(normalizedThreshold);
  const double threshold=normalizedThreshold*sigma;
  require(std::isfinite(threshold) && threshold>=0 &&
          std::isfinite(actual) && std::abs(actual-p)<=p*1e-6,
          "Unreliable discrete Gaussian noise quantile root");
  ComStatisticalNoiseData data;
  data.oneSidedNoiseQuantileV=threshold;
  data.analyticGaussianVarianceV2=sigma*sigma;
  data.analyticTotalVarianceV2=variance;
  data.verifiedTailProbability=actual;
  data.worstCaseDiscreteShiftV=shiftMax;
  data.statesEvaluated=shifts.size();
  data.exactFiniteState=true;
  Result<ComStatisticalNoiseData> out;
  out.status=Status::Warning;
  out.value=std::move(data);
  out.provenance=std::move(provenance);
  out.provenance.algorithm="com-pam4-gaussian-mixture";
  out.provenance.version="012_09-exploratory-1";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "This exact finite PAM4 Gaussian-mixture quantile is exploratory. "
    "It is not the full IEEE 93A statistical COM/ISI/XTALK noise model "
    "or an IEEE 802.3ck noise/PASS/FAIL value."});
  return out;
}
} // namespace si::channel
