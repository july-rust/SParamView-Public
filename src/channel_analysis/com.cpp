#include "si/channel_analysis/com.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr std::string_view lockedProfile =
    "IEEE-802.3ck-2022-100GAUI-1-C2C-120F-REFERENCE-CANDIDATE";
void require(bool ok, const char *message) {
  if (!ok) throw Error(message);
}
void check(const std::atomic_bool *flag) {
  if (flag && flag->load(std::memory_order_relaxed)) throw Cancelled();
}
bool finite(Complex x) {
  return std::isfinite(x.real()) && std::isfinite(x.imag());
}
void aligned(const ComplexTrace &source, const ComplexTrace &reference,
             double referenceOhm, const char *context) {
  validateTrace(source);
  require(source.frequencyHz.size() == reference.frequencyHz.size(),
          "COM traces must use the identical frequency grid");
  require(source.parameter.starts_with("Sdd"),
          "COM requires explicitly confirmed differential-mode S-parameters");
  require(source.stimulusOhm == source.responseOhm &&
          source.stimulusOhm == referenceOhm,
          "COM matched-wave preparation requires equal, verified Zref");
  for (size_t i = 0; i < reference.frequencyHz.size(); ++i) {
    require(source.frequencyHz[i] == reference.frequencyHz[i],
            "COM trace grid mismatch: no implicit extrapolation/resampling");
  }
  (void)context;
}
}
Result<ComPreparedData> prepareComSpectra(const ComInput &input,
    const ComSettings &settings, Provenance provenance,
    const std::atomic_bool *cancelled) {
  check(cancelled);
  require(settings.profileId == lockedProfile,
          "Unsupported or unverified COM profile identity");
  require(std::isfinite(settings.symbolRateHz) && settings.symbolRateHz > 0,
          "COM symbol rate must be explicitly supplied");
  require(std::isfinite(settings.referenceOhm) && settings.referenceOhm > 0,
          "COM differential reference impedance is required");
  require(!settings.txFfeTaps.empty() && settings.txFfeTaps.size() <= 64,
          "COM explicit TX FFE taps missing or excessive");
  for (double tap : settings.txFfeTaps)
    require(std::isfinite(tap), "Nonfinite COM FFE coefficient");
  validateTrace(input.thru);
  require(input.thru.parameter == "Sdd21" ||
          input.thru.parameter == "Sdd12",
          "COM thru must be Sdd21 or Sdd12 with confirmed direction");
  require(input.thru.stimulusOhm == settings.referenceOhm &&
          input.thru.responseOhm == settings.referenceOhm,
          "COM thru/nominal reference impedance mismatch");
  validateFrequency(input.thru.frequencyHz, true);
  require(input.thru.frequencyHz.back() >= settings.symbolRateHz / 2.0,
          "COM source band is below symbol Nyquist");
  aligned(input.rxFilter, input.thru, settings.referenceOhm, "RX");
  require(input.aggressors.size() <= 64, "Excessive COM aggressor count");
  std::unordered_set<std::string> names;
  for (const auto &aggressor : input.aggressors) {
    require(!aggressor.id.empty() && names.insert(aggressor.id).second,
            "Duplicate or empty COM aggressor ID");
    aligned(aggressor.transfer, input.thru, settings.referenceOhm, "aggressor");
  }
  ComPreparedData data;
  data.frequencyHz = input.thru.frequencyHz;
  const size_t n = data.frequencyHz.size();
  // Bound *output* complex spectral cells independently of the shared
  // validateTrace per-input limit; multi-aggressor arrays can be enormous.
  require(n <= (size_t{1} << 22) / (size_t{2} + input.aggressors.size()),
          "COM spectral preparation exceeds bounded memory budget");
  data.txFfeResponse.reserve(n);
  data.conditionedThru.reserve(n);
  for (const auto &aggressor : input.aggressors) {
    ComPreparedAggressor prepared;
    prepared.id = aggressor.id;
    prepared.coupling = aggressor.coupling;
    prepared.conditionedTransfer.reserve(n);
    data.aggressors.push_back(std::move(prepared));
  }
  for (size_t i = 0; i < n; ++i) {
    if (!(i & 1023u)) check(cancelled);
    const double f = data.frequencyHz[i];
    Complex ffe{0, 0};
    for (size_t k = 0; k < settings.txFfeTaps.size(); ++k) {
      const double angle = -2.0 * std::numbers::pi *
          (f / settings.symbolRateHz) * static_cast<double>(k);
      ffe += settings.txFfeTaps[k] * std::polar(1.0, angle);
    }
    require(finite(ffe), "COM FFE spectrum overflow");
    data.txFfeResponse.push_back(ffe);
    const Complex thru = input.thru.transfer[i] * ffe *
                         input.rxFilter.transfer[i];
    require(finite(thru), "COM conditioned THRU overflow");
    data.conditionedThru.push_back(thru);
    for (size_t j = 0; j < input.aggressors.size(); ++j) {
      // Each input is a validated complex transfer at identical matched planes.
      const Complex coupling = input.aggressors[j].transfer.transfer[i] *
                               ffe * input.rxFilter.transfer[i];
      require(finite(coupling), "COM aggressor spectrum overflow");
      data.aggressors[j].conditionedTransfer.push_back(coupling);
    }
  }
  check(cancelled);
  Result<ComPreparedData> result;
  result.status = Status::Warning;
  result.value = std::move(data);
  result.provenance = std::move(provenance);
  result.provenance.algorithm = "com-spectral-preparation";
  result.provenance.version = "012_09-prequalification";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Linear matched-wave spectral preparation only; not package/ISI/noise "
    "optimization, IEEE COM dB, or PHY conformance."});
  return result;
}
Result<ComMarginData> calculateCom(const ComInput &input,
    const ComSettings &settings, Provenance provenance,
    const std::atomic_bool *cancelled) {
  (void)input;
  // A full COM request must stop BEFORE allocating/preparing large spectra
  // while the normative package/ISI/noise/EQ/oracle qualification is absent.
  // Clients can separately request explicitly non-normative diagnostics
  // through prepareComSpectra(), never mistake those for COM dB.
  check(cancelled);
  Result<ComMarginData> result;
  result.provenance = std::move(provenance);
  result.provenance.algorithm = "com-ieee8023ck-unqualified";
  result.provenance.version = "012_09-prequalification";
  if (settings.profileId != lockedProfile) {
    result.status = Status::Unsupported;
    result.diagnostics.push_back({Severity::Error, "COM_PROFILE_UNSUPPORTED",
      "Requested PHY profile has no qualified IEEE COM implementation."});
  } else {
    result.status = Status::NotEvaluated;
    result.diagnostics.push_back({Severity::Error, "COM_PROFILE_NOT_QUALIFIED",
      "Final IEEE 802.3ck-2022 Table 120F-8, Test1/Test2 package/noise/ISI "
      "implementation, and at least three independent same-PHY golden channels "
      "are not yet qualified; no COM margin or PASS/FAIL may be returned."});
  }
  return result;
}
} // namespace si::channel
