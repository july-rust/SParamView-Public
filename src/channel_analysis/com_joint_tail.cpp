#include "si/channel_analysis/com_joint_tail.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr double kSqrt2 = 1.4142135623730950488;
constexpr double kLevels[4] = {-1., -1./3., 1./3., 1.};
constexpr size_t kMaxPam4Taps = 6;
constexpr size_t kMaxStates = 8192;

void require(bool condition, const char* why) {
  if (!condition) throw Error(why);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
bool bounded(const std::string& value) {
  return !value.empty() && value.size() <= 256;
}
bool validSha(const std::string& sha) {
  if (sha.size() != 64) return false;
  for (unsigned char c : sha)
    if (!std::isxdigit(c)) return false;
  return true;
}
void validateExpected(const Provenance& p, const std::string& test) {
  require(test == "TEST1" || test == "TEST2", "Invalid COM Test identity");
  require(validSha(p.sourceSha256) && bounded(p.revision) &&
          bounded(p.mappingIdentity) && bounded(p.settingsIdentity) &&
          p.workspaceGeneration > 0 && p.mappingGeneration > 0,
          "Missing joint-tail source identity");
  // Split actual fields, not a substring search: adjacent duplicate case
  // fields must not be hidden by their shared '|' delimiter.
  size_t count = 0, start = 0;
  while (start <= p.settingsIdentity.size()) {
    const size_t end = p.settingsIdentity.find('|', start);
    const auto field = p.settingsIdentity.substr(start,
        end == std::string::npos ? end : end - start);
    if (field.rfind("case=", 0) == 0) {
      require(field == "case=" + test, "Mixed Test case settings");
      ++count;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  require(count == 1, "Missing or duplicated Test case settings");
}
void validateComponent(const Result<ComJointVoltageEvidence>& r,
                       const Provenance& expected, const std::string& test,
                       ComJointComponentKind kind, bool receiver,
                       std::unordered_set<std::string>& ids,
                       std::string& plane, std::string& calibration) {
  require(r.usable(), "Missing or unusable voltage component");
  const auto& p = r.provenance;
  require(p.sourceSha256 == expected.sourceSha256 &&
          p.revision == expected.revision &&
          p.mappingIdentity == expected.mappingIdentity &&
          p.settingsIdentity == expected.settingsIdentity &&
          p.workspaceGeneration == expected.workspaceGeneration &&
          p.mappingGeneration == expected.mappingGeneration &&
          bounded(p.algorithm) && bounded(p.version),
          "Voltage component provenance mismatch");
  const auto& v = *r.value;
  require(v.kind == kind && v.testCase == test && bounded(v.componentId) &&
          bounded(v.referencePlane) && bounded(v.calibrationIdentity) &&
          v.assumedIndependent &&
          v.includesReceiverGaussianNoise == receiver,
          "Component type, identity, independence or RX-noise ownership invalid");
  require(ids.insert(v.componentId).second, "Duplicate component ID");
  require(std::isfinite(v.amplitudeV) &&
          (receiver ? v.amplitudeV > 0. : v.amplitudeV >= 0. ||
           kind == ComJointComponentKind::IndependentPam4Tap),
          "Nonfinite or invalid component voltage");
  if (plane.empty()) {
    plane = v.referencePlane;
    calibration = v.calibrationIdentity;
  } else {
    require(plane == v.referencePlane &&
            calibration == v.calibrationIdentity,
            "Voltage calibration or receiver plane mismatch");
  }
}
void samePhysicalSource(const Provenance& a, const Provenance& b) {
  require(a.sourceSha256 == b.sourceSha256 && a.revision == b.revision &&
          a.mappingIdentity == b.mappingIdentity &&
          a.workspaceGeneration == b.workspaceGeneration &&
          a.mappingGeneration == b.mappingGeneration,
          "Test1/Test2 physical source mismatch");
}
} // namespace

Result<ComJointTailData> calculateComJointTail(
    const ComJointTailInput& input, const std::string& test,
    const Provenance& expected, const std::atomic_bool* cancelled) {
  check(cancelled);
  validateExpected(expected, test);
  require(input.pam4Taps.size() <= kMaxPam4Taps, "PAM4 state budget exceeded");
  const double p = input.oneSidedTailProbability;
  require(std::isfinite(p) && p >= 1e-12 && p <= 0.1,
          "Unsupported one-sided tail probability");
  std::unordered_set<std::string> ids;
  std::string plane, calibration;
  validateComponent(input.receiverGaussian, expected, test,
                    ComJointComponentKind::ReceiverGaussian, true,
                    ids, plane, calibration);
  validateComponent(input.randomJitterGaussian, expected, test,
                    ComJointComponentKind::RandomJitterGaussian, false,
                    ids, plane, calibration);
  validateComponent(input.dualDiracPeak, expected, test,
                    ComJointComponentKind::DualDiracPeak, false,
                    ids, plane, calibration);
  for (const auto& tap : input.pam4Taps)
    validateComponent(tap, expected, test,
                      ComJointComponentKind::IndependentPam4Tap, false,
                      ids, plane, calibration);
  const double rx = input.receiverGaussian.value->amplitudeV;
  const double rj = input.randomJitterGaussian.value->amplitudeV;
  const double sigma = std::hypot(rx, rj);
  require(std::isfinite(sigma) && sigma > 0.,
          "Invalid combined independent Gaussian RMS");
  const double sigma2 = sigma * sigma;
  require(std::isfinite(sigma2) && sigma2 > 0.,
          "Gaussian variance cannot be represented");
  const double peak = input.dualDiracPeak.value->amplitudeV;
  const double normalizedPeak = peak / sigma;
  require(std::isfinite(normalizedPeak) && normalizedPeak < 1e10,
          "Unresolvable Dual-Dirac/Gaussian dynamic range");

  // Work in normalized Gaussian sigma units to retain nV/pV precision.
  std::vector<double> shifts{0.};
  double upperShift = normalizedPeak, normalizedVariance = 1.;
  for (const auto& tap : input.pam4Taps) {
    check(cancelled);
    const double normalizedTap = tap.value->amplitudeV / sigma;
    require(std::isfinite(normalizedTap) &&
            std::abs(normalizedTap) < 1e10 &&
            upperShift + std::abs(normalizedTap) < 1e10,
            "Unresolvable PAM4/Gaussian dynamic range");
    upperShift += std::abs(normalizedTap);
    normalizedVariance += (5. / 9.) * normalizedTap * normalizedTap;
    require(shifts.size() <= kMaxStates / 4, "PAM4 state count exceeded");
    std::vector<double> expanded;
    expanded.reserve(shifts.size() * 4);
    for (double old : shifts)
      for (double level : kLevels)
        expanded.push_back(old + normalizedTap * level);
    shifts = std::move(expanded);
  }
  if (normalizedPeak > 0.) {
    check(cancelled);
    require(shifts.size() <= kMaxStates / 2, "Dual-Dirac state budget exceeded");
    std::vector<double> expanded;
    expanded.reserve(shifts.size() * 2);
    for (double old : shifts) {
      expanded.push_back(old - normalizedPeak);
      expanded.push_back(old + normalizedPeak);
    }
    shifts = std::move(expanded);
  }
  normalizedVariance += normalizedPeak * normalizedPeak;
  require(shifts.size() <= kMaxStates &&
          std::isfinite(normalizedVariance) &&
          std::isfinite(upperShift) && upperShift < 1e10,
          "Invalid statistical state or variance");
  const double totalVariance = sigma2 * normalizedVariance;
  require(std::isfinite(totalVariance) && totalVariance > 0.,
          "Total voltage variance cannot be represented");

  const double invStates = 1. / static_cast<double>(shifts.size());
  auto tail = [&](double threshold) {
    double sum = 0., compensation = 0.;
    for (size_t i = 0; i < shifts.size(); ++i) {
      if (!(i & 1023u)) check(cancelled);
      const double term = .5 * std::erfc(
          (threshold - shifts[i]) / kSqrt2);
      require(std::isfinite(term) && term >= 0. && term <= 1.,
              "Nonfinite Gaussian tail probability");
      const double y = term - compensation;
      const double accum = sum + y;
      compensation = (accum - sum) - y;
      sum = accum;
    }
    const double value = sum * invStates;
    require(std::isfinite(value) && value >= 0. && value <= 1.,
            "Invalid mixture tail");
    return value;
  };
  double low = 0., high = upperShift + 12.;
  require(std::isfinite(high) && tail(low) > p && tail(high) < p,
          "Unbracketed joint-tail quantile");
  for (int iter = 0; iter < 120; ++iter) {
    check(cancelled);
    const double midpoint = low + (high - low) / 2.;
    if (midpoint == low || midpoint == high) break;
    if (tail(midpoint) > p) low = midpoint;
    else high = midpoint;
  }
  const double q = (low + high) / 2.;
  const double verified = tail(q), voltage = q * sigma;
  require(std::isfinite(voltage) && voltage >= 0. &&
          std::abs(verified - p) <= p * 1e-6,
          "Joint-tail probability accuracy not qualified");
  check(cancelled);

  Result<ComJointTailData> result;
  result.status = Status::Warning;
  result.value = ComJointTailData{voltage, verified, sigma2, totalVariance,
      shifts.size(), test, plane, calibration};
  result.provenance = expected;
  result.provenance.algorithm = "com-joint-tail-exploratory";
  result.provenance.version = "012_20-1";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Independent calibrated voltage-mixture diagnostic only; not Annex 93A "
      "physical noise/DER qualification, COM dB, FOM, or PHY PASS/FAIL."});
  return result;
}

Result<ComJointTailCasesData> calculateComJointTailCases(
    const ComJointTailInput& test1, const ComJointTailInput& test2,
    const Provenance& p1, const Provenance& p2,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  validateExpected(p1, "TEST1");
  validateExpected(p2, "TEST2");
  samePhysicalSource(p1, p2);
  require(test1.oneSidedTailProbability == test2.oneSidedTailProbability,
          "Test1/Test2 tail targets are not comparable");
  const auto a = calculateComJointTail(test1, "TEST1", p1, cancelled);
  const auto b = calculateComJointTail(test2, "TEST2", p2, cancelled);
  require(a.value->referencePlane == b.value->referencePlane &&
          a.value->calibrationIdentity == b.value->calibrationIdentity,
          "Test1/Test2 receiver voltage planes or calibrations differ");
  check(cancelled);
  Result<ComJointTailCasesData> out;
  out.status = Status::Warning;
  out.value = ComJointTailCasesData{*a.value, *b.value,
      std::max(a.value->oneSidedNoiseQuantileV,
               b.value->oneSidedNoiseQuantileV)};
  out.provenance = p1;
  out.provenance.settingsIdentity = "EXPLORATORY_TEST1_TEST2_JOINT_TAIL";
  out.provenance.algorithm = "com-joint-tail-exploratory";
  out.provenance.version = "012_20-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Maximum of two exploratory noise quantiles, not IEEE COM worst case."});
  return out;
}
} // namespace si::channel
