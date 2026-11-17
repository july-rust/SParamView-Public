#include "si/channel_analysis/com_jitter.hpp"
#include "si/channel_analysis/com_statistics.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t maxWaveSamples = 4097;
constexpr size_t maxQuerySamples = 128;
constexpr double minTail = 1e-12, maxTail = 0.1;
constexpr double sqrtTwo = 1.4142135623730950488;

void require(bool ok, const char* reason) { if (!ok) throw Error(reason); }
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
void finiteNonnegative(double x, const char* reason) {
  require(std::isfinite(x) && x >= 0, reason);
}
void finiteNormalOrZero(double x, const char* reason) {
  finiteNonnegative(x, reason);
  require(x == 0 || x >= std::numeric_limits<double>::min(), reason);
}
double product(double a, double b, const char* reason) {
  const double value = a*b;
  require(std::isfinite(value) && (a == 0 || b == 0 || value > 0), reason);
  finiteNormalOrZero(value, reason);
  return value;
}
double gaussianPdf(double z, double sigma) {
  if (!std::isfinite(z)) return 0;
  const double logPdf = -0.5*z*z - std::log(sigma)
                        - 0.5*std::log(2*std::numbers::pi);
  return std::exp(logPdf);
}
double gaussianCdf(double z) { return 0.5*std::erfc(-z/sqrtTwo); }
double gaussianUpper(double z) { return 0.5*std::erfc(z/sqrtTwo); }
double mixtureTailScaled(double q, double shift) {
  return 0.5*(gaussianUpper(q-shift) + gaussianUpper(q+shift));
}
double mixturePdf(double voltage, double shift, double sigma) {
  const double zPlus = (voltage-shift)/sigma;
  const double zMinus = (voltage+shift)/sigma;
  return 0.5*(gaussianPdf(zPlus,sigma) + gaussianPdf(zMinus,sigma));
}
double mixtureCdf(double voltage, double shift, double sigma) {
  const double zPlus = (voltage-shift)/sigma;
  const double zMinus = (voltage+shift)/sigma;
  return 0.5*(gaussianCdf(zPlus) + gaussianCdf(zMinus));
}
void validateMatchingSource(const Provenance& a, const Provenance& b) {
  require(!a.sourceSha256.empty() && !a.mappingIdentity.empty() &&
              a.sourceSha256 == b.sourceSha256 &&
              a.revision == b.revision &&
              a.mappingIdentity == b.mappingIdentity &&
              a.workspaceGeneration == b.workspaceGeneration &&
              a.mappingGeneration == b.mappingGeneration &&
              a.settingsIdentity == b.settingsIdentity,
          "Jitter waveform and receiver noise source/provenance mismatch");
}
} // namespace

Result<ComJitterData> calculateComJitter(
    const ComJitterInput& input, Provenance provenance,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  require(std::isfinite(input.symbolRateBd) && input.symbolRateBd > 0,
          "Jitter requires positive finite symbol rate in baud");
  finiteNonnegative(input.randomJitterRmsUi, "Invalid RJ RMS in UI");
  finiteNonnegative(input.dualDiracPeakUi, "Invalid dual-Dirac peak in UI");
  // These are numerical guardrails, NOT an IEEE-specified jitter range.
  require(input.randomJitterRmsUi <= 1 && input.dualDiracPeakUi <= 1,
          "Jitter magnitude exceeds this diagnostic's 1 UI budget");
  require(std::isfinite(input.oneSidedTailProbability) &&
              input.oneSidedTailProbability >= minTail &&
              input.oneSidedTailProbability <= maxTail,
          "Unqualified one-sided probability");
  require(input.timeSeconds.size() >= 3 &&
              input.timeSeconds.size() <= maxWaveSamples &&
              input.receiverVoltageV.size() == input.timeSeconds.size(),
          "Invalid calibrated waveform sample count/budget");
  require(input.queryVoltageV.size() <= maxQuerySamples,
          "Jitter PDF/CDF voltage query budget exceeded");
  require(std::isfinite(input.decisionTimeSeconds) &&
              !input.voltageReferencePlane.empty() &&
              !input.voltageCalibrationIdentity.empty(),
          "Explicit calibrated voltage plane, calibration and decision time required");
  require(input.receiverNoise.usable() &&
              input.receiverNoise.status == Status::Warning &&
              input.receiverNoise.value.has_value() &&
              input.receiverNoise.value->model ==
                  "EXPLORATORY_GAUSSIAN_RECEIVER_NOISE" &&
              input.receiverNoise.provenance.algorithm ==
                  "com-gaussian-receiver-noise-exploratory",
          "Unqualified 012_15 receiver noise result");
  validateMatchingSource(provenance, input.receiverNoise.provenance);
  const auto& receiver = *input.receiverNoise.value;
  finiteNormalOrZero(receiver.rmsV, "Invalid receiver RMS voltage");
  finiteNormalOrZero(receiver.varianceV2, "Invalid receiver variance");
  require(std::isfinite(receiver.integrationStopHz) &&
              receiver.integrationStopHz > 0 &&
              receiver.hasContinuousDensity == (receiver.rmsV > 0),
          "Receiver noise is invalid or internally inconsistent");
  const double recomputedReceiverVariance = product(
      receiver.rmsV, receiver.rmsV, "Receiver variance underflow/overflow");
  require(std::abs(recomputedReceiverVariance - receiver.varianceV2) <=
              1e-12*std::max(recomputedReceiverVariance,receiver.varianceV2),
          "Receiver RMS/variance inconsistent");
  for (double query: input.queryVoltageV)
    require(std::isfinite(query), "Nonfinite jitter voltage query");

  for (size_t i = 0; i < input.timeSeconds.size(); ++i) {
    if (!(i & 255u)) check(cancelled);
    require(std::isfinite(input.timeSeconds[i]) &&
                std::isfinite(input.receiverVoltageV[i]),
            "Nonfinite calibrated waveform sample");
    if (i) {
      const double width = input.timeSeconds[i]-input.timeSeconds[i-1];
      require(std::isfinite(width) &&
                  width >= std::numeric_limits<double>::min(),
              "Non-increasing or subnormal waveform time interval");
    }
  }
  require(input.decisionTimeSeconds > input.timeSeconds.front() &&
              input.decisionTimeSeconds < input.timeSeconds.back(),
          "Decision time must be strictly inside the measured waveform");

  const auto it = std::lower_bound(
      input.timeSeconds.begin(), input.timeSeconds.end(),
      input.decisionTimeSeconds);
  const size_t hiIndex = static_cast<size_t>(it-input.timeSeconds.begin());
  const bool exact = input.timeSeconds[hiIndex] == input.decisionTimeSeconds;
  const size_t lo = hiIndex-1;
  const size_t hi = exact ? hiIndex+1 : hiIndex;
  const double dt = input.timeSeconds[hi]-input.timeSeconds[lo];
  const double dv = input.receiverVoltageV[hi]-input.receiverVoltageV[lo];
  require(std::isfinite(dt) && dt > 0 && std::isfinite(dv),
          "Unresolvable voltage slope inputs");
  const double slope = dv/dt;
  require(std::isfinite(slope) && (slope != 0 || dv == 0),
          "Nonzero receiver voltage slope underflow");
  require(slope == 0 ||
              std::abs(slope) >= std::numeric_limits<double>::min(),
          "Subnormal receiver voltage slope unsupported");

  ComJitterData data;
  data.unitIntervalSeconds = 1.0/input.symbolRateBd;
  require(std::isfinite(data.unitIntervalSeconds) &&
              data.unitIntervalSeconds >= std::numeric_limits<double>::min(),
          "Unrepresentable unit interval");
  data.randomJitterRmsSeconds = product(
      data.unitIntervalSeconds, input.randomJitterRmsUi,
      "RJ time RMS underflow/overflow");
  data.dualDiracPeakSeconds = product(
      data.unitIntervalSeconds, input.dualDiracPeakUi,
      "Dual-Dirac peak time underflow/overflow");
  data.decisionSlopeVPerSecond = slope;
  data.jitterGaussianRmsV = product(
      std::abs(slope), data.randomJitterRmsSeconds,
      "Jitter voltage RMS underflow/overflow");
  data.dualDiracVoltagePeakV = product(
      std::abs(slope), data.dualDiracPeakSeconds,
      "Dual-Dirac voltage peak underflow/overflow");
  data.receiverNoiseRmsV = receiver.rmsV;
  data.combinedGaussianRmsV = std::hypot(
      data.jitterGaussianRmsV, data.receiverNoiseRmsV);
  finiteNormalOrZero(data.combinedGaussianRmsV,
                     "Combined Gaussian noise RMS invalid");
  const double gaussianVariance = product(
      data.combinedGaussianRmsV, data.combinedGaussianRmsV,
      "Combined Gaussian variance underflow/overflow");
  const double discreteVariance = product(
      data.dualDiracVoltagePeakV, data.dualDiracVoltagePeakV,
      "Dual-Dirac voltage variance underflow/overflow");
  data.totalVoltageVarianceV2 = gaussianVariance+discreteVariance;
  finiteNormalOrZero(data.totalVoltageVarianceV2,
                     "Total voltage variance underflow/overflow");

  Result<ComJitterData> out;
  out.status = Status::Warning;
  out.provenance = std::move(provenance);
  out.provenance.algorithm = "com-local-linear-rj-dual-dirac-exploratory";
  out.provenance.version = "012_16-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Local-linear jitter and receiver-noise mixture is non-normative; "
      "not IEEE 93A COM/DER, full ISI/XTALK PDF, BER or PHY PASS/FAIL."});
  if (data.combinedGaussianRmsV == 0) {
    out.diagnostics.push_back({Severity::Warning, "NO_CONTINUOUS_DENSITY",
      "Zero Gaussian variance produces a discrete one/two-atom distribution: "
      "no continuous PDF/CDF or positive-tail quantile is asserted."});
    out.value = std::move(data);
    return out;
  }

  const double ratio = data.dualDiracVoltagePeakV/data.combinedGaussianRmsV;
  require(std::isfinite(ratio) && ratio < 1e10,
          "Unresolvable dual-Dirac/Gaussian scale ratio");
  if (data.dualDiracVoltagePeakV == 0) {
    // Reuse the qualified exact Gaussian-only quantile from 012_09/012_15.
    ComStatisticalNoiseInput gaussian;
    gaussian.gaussianRmsV = data.combinedGaussianRmsV;
    gaussian.oneSidedTailProbability = input.oneSidedTailProbability;
    const auto quantile = calculatePam4StatisticalNoise(
        gaussian, out.provenance, cancelled);
    require(quantile.usable() && quantile.value.has_value() &&
                quantile.value->statesEvaluated == 1,
            "Gaussian-only jitter quantile failed");
    data.oneSidedQuantileV = quantile.value->oneSidedNoiseQuantileV;
    data.verifiedTailProbability = quantile.value->verifiedTailProbability;
  } else {
    const double p = input.oneSidedTailProbability;
    double low = 0, high = ratio+12;
    require(std::isfinite(high) && mixtureTailScaled(low,ratio)>p &&
                mixtureTailScaled(high,ratio)<p,
            "Dual-Dirac mixture quantile cannot be bracketed");
    for (int iter = 0; iter < 120; ++iter) {
      check(cancelled);
      const double mid = low+(high-low)*0.5;
      if (mid == low || mid == high) break;
      if (mixtureTailScaled(mid,ratio)>p) low=mid;
      else high=mid;
    }
    const double qScaled = low+(high-low)*0.5;
    data.verifiedTailProbability = mixtureTailScaled(qScaled,ratio);
    data.oneSidedQuantileV = product(
        qScaled, data.combinedGaussianRmsV,
        "Dual-Dirac quantile underflow/overflow");
    require(std::isfinite(data.verifiedTailProbability) &&
                std::abs(data.verifiedTailProbability-p) <= p*1e-6,
            "Dual-Dirac quantile failed reverse-tail verification");
  }
  require(std::isfinite(data.oneSidedQuantileV) &&
              data.oneSidedQuantileV > 0,
          "Invalid positive one-sided noise quantile");
  data.hasContinuousDensity = true;
  data.pdfPerV.reserve(input.queryVoltageV.size());
  data.cdf.reserve(input.queryVoltageV.size());
  for (size_t i = 0; i < input.queryVoltageV.size(); ++i) {
    if (!(i & 63u)) check(cancelled);
    const double x = input.queryVoltageV[i];
    const double pdf = mixturePdf(
        x,data.dualDiracVoltagePeakV,data.combinedGaussianRmsV);
    const double cdf = mixtureCdf(
        x,data.dualDiracVoltagePeakV,data.combinedGaussianRmsV);
    require(std::isfinite(pdf) && pdf >= 0 &&
                std::isfinite(cdf) && cdf >= 0 && cdf <= 1,
            "Nonfinite jitter mixture PDF/CDF");
    data.pdfPerV.push_back(pdf);
    data.cdf.push_back(cdf);
  }
  out.value = std::move(data);
  return out;
}

} // namespace si::channel
