#include "si/channel_analysis/com_receiver_noise.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t maxFrequencySamples = 8193;
constexpr size_t maxVoltageSamples = 256;
constexpr double minTail = 1e-12;
constexpr double maxTail = 0.1;
constexpr double sqrtTwo = 1.4142135623730950488;

void require(bool valid, const char* reason) {
  if (!valid) throw Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
double densityAt(double voltage, double rms) {
  const double z = voltage/rms;
  if (!std::isfinite(z)) return 0.0; // far outside representable tail
  // Evaluating exp(-z^2/2) first loses representable densities when RMS
  // is very small. Evaluate the complete PDF in log space instead.
  const double logPdf = -0.5*z*z - std::log(rms) -
                        0.5*std::log(2.0*std::numbers::pi);
  return std::exp(logPdf); // caller rejects overflow; genuine underflow -> 0
}
} // namespace

Result<ComReceiverNoiseData> calculateComReceiverNoise(
    const ComReceiverNoiseInput& input, Provenance provenance,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  require(std::isfinite(input.oneSidedPsdV2PerHz) &&
              input.oneSidedPsdV2PerHz >= 0,
          "A finite non-negative one-sided PSD (V^2/Hz) is required");
  require(std::isfinite(input.oneSidedTailProbability) &&
              input.oneSidedTailProbability >= minTail &&
              input.oneSidedTailProbability <= maxTail,
          "One-sided tail probability is outside the qualified range");
  const auto n = input.frequencyHz.size();
  require(n >= 2 && n <= maxFrequencySamples &&
              input.noiseTransfer.size() == n,
          "Invalid receiver-noise frequency/transfer array or budget");
  require(input.queryVoltageV.size() <= maxVoltageSamples,
          "Receiver-noise PDF/CDF evaluation budget exceeded");
  require(input.frequencyHz.front() == 0.0,
          "Receiver-noise integration must start at DC");
  for (double v: input.queryVoltageV)
    require(std::isfinite(v), "Nonfinite PDF/CDF query voltage");

  // Apply the specified one-sided PSD exactly once; no extra factor of 2.
  auto weightedPsd = [&](size_t index) {
    const auto h = input.noiseTransfer[index];
    require(std::isfinite(h.real()) && std::isfinite(h.imag()),
            "Nonfinite receiver noise transfer");
    // Exact zero PSD has zero noise regardless of filter gain. Otherwise,
    // reject silent underflow at either stage instead of inventing a
    // physically false zero-variance distribution.
    if (input.oneSidedPsdV2PerHz == 0) return 0.0;
    const double powerGain = std::norm(h);
    require(std::isfinite(powerGain), "Receiver-noise gain overflow");
    require(powerGain != 0.0 ||
                (h.real() == 0.0 && h.imag() == 0.0),
            "Receiver-noise transfer power underflow");
    const double spectral = input.oneSidedPsdV2PerHz*powerGain;
    require(std::isfinite(spectral), "Receiver-noise PSD overflow");
    require(spectral != 0.0 || powerGain == 0.0,
            "Receiver-noise weighted PSD underflow");
    return spectral;
  };
  double sum = 0.0, correction = 0.0;
  double previousWeighted = weightedPsd(0);
  bool nonzeroWeighted = previousWeighted > 0;
  for (size_t i = 1; i < n; ++i) {
    if (!(i & 255u)) check(cancelled);
    const double previousF = input.frequencyHz[i-1];
    const double currentF = input.frequencyHz[i];
    require(std::isfinite(previousF) && std::isfinite(currentF) &&
                previousF >= 0 && currentF > previousF,
            "Receiver-noise frequency axis is not finite and increasing");
    const double width = currentF - previousF;
    require(std::isfinite(width) && width > 0,
            "Receiver-noise frequency increment overflow");
    const double currentWeighted = weightedPsd(i);
    nonzeroWeighted |= currentWeighted > 0;
    // A subnormal step loses a nonzero fraction when halved; reject such
    // physically unresolvable frequency grids rather than bias variance.
    require(width >= std::numeric_limits<double>::min(),
            "Subnormal receiver-noise frequency interval");
    const double halfWidth = width*0.5;
    const double left = previousWeighted*halfWidth;
    const double right = currentWeighted*halfWidth;
    require((previousWeighted == 0.0 || left > 0.0) &&
                (currentWeighted == 0.0 || right > 0.0),
            "Receiver-noise interval variance underflow");
    const double term = left + right;
    require(std::isfinite(term) && term >= 0,
            "Receiver-noise variance integration overflow");
    // Compensated non-negative trapezoid accumulation.
    const double y = term - correction;
    const double next = sum + y;
    correction = (next - sum) - y;
    sum = next;
    require(std::isfinite(sum) && sum >= 0,
            "Receiver-noise variance accumulation overflow");
    previousWeighted = currentWeighted;
  }
  require(!nonzeroWeighted || sum > 0,
          "Receiver-noise variance underflow");
  const double rms = std::sqrt(sum);
  require(std::isfinite(rms), "Receiver-noise RMS overflow");

  ComReceiverNoiseData data;
  data.varianceV2 = sum;
  data.rmsV = rms;
  data.integrationStopHz = input.frequencyHz.back();

  Result<ComReceiverNoiseData> out;
  out.status = Status::Warning; // never a normative IEEE COM pass
  out.provenance = std::move(provenance);
  out.provenance.algorithm = "com-gaussian-receiver-noise-exploratory";
  out.provenance.version = "012_15-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Finite-band Gaussian noise is an exploratory mathematical result, "
      "not the complete IEEE Annex 93A receiver noise, COM or PHY verdict."});

  if (sum == 0.0) {
    out.diagnostics.push_back({Severity::Warning, "NO_CONTINUOUS_DENSITY",
        "Zero variance is a degenerate distribution: no continuous Gaussian "
        "PDF or positive-tail quantile is reported."});
    out.value = std::move(data);
    return out;
  }

  ComStatisticalNoiseInput gaussian;
  gaussian.gaussianRmsV = rms;
  gaussian.oneSidedTailProbability = input.oneSidedTailProbability;
  // Explicitly empty PAM4 taps: the existing exact finite-state solver
  // reduces to the pure Gaussian Q-function and 1-state quantile.
  const auto quantile = calculatePam4StatisticalNoise(
      gaussian, out.provenance, cancelled);
  require(quantile.usable() && quantile.value.has_value() &&
              quantile.value->statesEvaluated == 1,
          "Gaussian-only quantile was not qualified");
  data.oneSidedQuantileV = quantile.value->oneSidedNoiseQuantileV;
  data.verifiedTailProbability = quantile.value->verifiedTailProbability;
  data.hasContinuousDensity = true;
  data.pdfPerV.reserve(input.queryVoltageV.size());
  data.cdf.reserve(input.queryVoltageV.size());
  for (size_t i = 0; i < input.queryVoltageV.size(); ++i) {
    if (!(i & 63u)) check(cancelled);
    const double voltage = input.queryVoltageV[i];
    const double pdf = densityAt(voltage, rms);
    const double z = voltage/rms;
    const double cdf = 0.5*std::erfc(-z/sqrtTwo);
    require(std::isfinite(pdf) && pdf >= 0 &&
                std::isfinite(cdf) && cdf >= 0 && cdf <= 1,
            "Receiver-noise PDF/CDF numerical overflow");
    data.pdfPerV.push_back(pdf);
    data.cdf.push_back(cdf);
  }
  out.value = std::move(data);
  return out;
}

} // namespace si::channel
