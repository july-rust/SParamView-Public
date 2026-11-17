#include "si/channel_analysis/com_dfe.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>

namespace si::channel {
namespace {
void require(bool ok, const char* message) {
  if (!ok) throw si::Error(message);
}
void checkCancelled(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool sourceMatches(const Provenance& a, const Provenance& b) {
  return a.sourceSha256 == b.sourceSha256 &&
         a.revision == b.revision &&
         a.mappingIdentity == b.mappingIdentity &&
         a.settingsIdentity == b.settingsIdentity &&
         a.workspaceGeneration == b.workspaceGeneration &&
         a.mappingGeneration == b.mappingGeneration;
}
bool validSource(const Provenance& p) {
  return p.sourceSha256.size() == 64 &&
         std::all_of(p.sourceSha256.begin(), p.sourceSha256.end(),
                     [](unsigned char ch){ return std::isxdigit(ch) != 0; }) &&
         !p.revision.empty() && !p.mappingIdentity.empty() &&
         !p.settingsIdentity.empty() && p.workspaceGeneration != 0 &&
         p.mappingGeneration != 0;
}
} // namespace

void validateCom120F8DfeTaps(const std::array<double,6>& taps) {
  const auto profile = final8023ck120F8();
  require(profile.dfeTaps == static_cast<int>(taps.size()),
          "COM DFE: unsupported final-profile tap count");
  for (size_t k = 0; k < taps.size(); ++k)
    require(std::isfinite(taps[k]) && taps[k] >= profile.dfeMin[k] &&
            taps[k] <= profile.dfeMax[k],
            "COM DFE: nonfinite or out-of-range final 120F-8 tap");
}

Result<ComDfeData> calculateComDfe(const ComDfeInput& input,
    Provenance provenance, const std::atomic_bool* cancelled) {
  checkCancelled(cancelled);
  validateCom120F8DfeTaps(input.taps);
  require(input.testCase == "TEST1" || input.testCase == "TEST2",
          "COM DFE: unsupported package case");
  require(validSource(provenance),
          "COM DFE: incomplete or invalid source identity");
  const std::string marker = "|case=" + input.testCase + "|";
  require(provenance.settingsIdentity.find(marker) != std::string::npos,
          "COM DFE: package case not bound to source settings identity");
  require(input.symbolPulse.usable() &&
          input.symbolPulse.status == Status::Warning &&
          input.symbolPulse.value.has_value(),
          "COM DFE: unavailable upstream symbol response");
  const auto& upstream = input.symbolPulse.provenance;
  require(validSource(upstream) && sourceMatches(upstream, provenance) &&
          upstream.algorithm == "com-120f8-rectangular-symbol-band-quadrature" &&
          upstream.version == "012_09_03-pulse-1",
          "COM DFE: incompatible source, mapping, settings or algorithm");
  const auto& pulse = *input.symbolPulse.value;
  require(pulse.model ==
           "EXPLORATORY_MEASURED_BAND_RECTANGULAR_SYMBOL_CONVOLUTION",
          "COM DFE: cannot substitute an impulse for a symbol response");
  constexpr size_t count = 13;
  require(pulse.timeSeconds.size() == count &&
          pulse.unitSymbolPulse.size() == count,
          "COM DFE: requires exactly -3..+9 UI observed symbol samples");
  const double ui = 1.0 / final8023ck120F8().symbolRateHz;
  require(std::isfinite(ui) && ui > 0 &&
          std::isfinite(pulse.symbolPeriodSeconds) &&
          std::abs(pulse.symbolPeriodSeconds-ui) <= 1e-12*ui,
          "COM DFE: inconsistent published symbol interval");
  const double cursorTime = pulse.timeSeconds[3];
  require(std::isfinite(cursorTime) && cursorTime >= 0,
          "COM DFE: invalid explicit decision cursor");
  double maxAmplitude = 0;
  for (size_t i = 0; i < count; ++i) {
    checkCancelled(cancelled);
    const double t = pulse.timeSeconds[i], v = pulse.unitSymbolPulse[i];
    const double expected =
      cursorTime + static_cast<double>(static_cast<int>(i)-3)*ui;
    require(std::isfinite(t) && t >= 0 && std::isfinite(v) &&
            std::isfinite(expected) &&
            std::abs(t-expected) <= 1e-8*ui,
            "COM DFE: non-UI-spaced or nonfinite symbol sample");
    if (i) require(t > pulse.timeSeconds[i-1],
                   "COM DFE: non-increasing sample times");
    maxAmplitude = std::max(maxAmplitude, std::abs(v));
  }
  require(std::isnormal(maxAmplitude) &&
          std::isfinite(pulse.maximumAbsolutePulse) &&
          std::abs(pulse.maximumAbsolutePulse-maxAmplitude) <=
             1e-8*maxAmplitude,
          "COM DFE: invalid or inconsistent source maximum amplitude");
  const double cursor = pulse.unitSymbolPulse[3];
  // Relative conditioning guard, NOT a PHY cursor requirement.
  require(std::isnormal(cursor) &&
          std::abs(cursor) > 1e-10*maxAmplitude,
          "COM DFE: absent or ill-conditioned main cursor");

  ComDfeData out;
  out.cursorAmplitudeUnitSymbol = cursor;
  out.symbolPeriodSeconds = ui;
  out.decisionTimeSeconds = cursorTime;
  out.taps = input.taps;
  out.testCase = input.testCase;
  for (size_t i = 0; i < count; ++i) {
    checkCancelled(cancelled);
    const double v = pulse.unitSymbolPulse[i] / cursor;
    require(std::isfinite(v), "COM DFE: invalid normalized symbol sample");
    out.normalizedBefore[i] = v;
    out.normalizedAfter[i] = v;
    if (i >= 4 && i <= 9)
      out.normalizedAfter[i] -= input.taps[i-4];
    require(std::isfinite(out.normalizedAfter[i]),
            "COM DFE: nonfinite compensated symbol sample");
    if (i < 3) {
      out.precursorRssBefore = std::hypot(out.precursorRssBefore, v);
      out.precursorRssAfter =
        std::hypot(out.precursorRssAfter, out.normalizedAfter[i]);
    } else if (i > 3) {
      out.postcursorRssBefore = std::hypot(out.postcursorRssBefore, v);
      out.postcursorRssAfter =
        std::hypot(out.postcursorRssAfter, out.normalizedAfter[i]);
    }
  }
  require(std::isfinite(out.precursorRssBefore) &&
          std::isfinite(out.precursorRssAfter) &&
          std::isfinite(out.postcursorRssBefore) &&
          std::isfinite(out.postcursorRssAfter) &&
          out.normalizedBefore[3] == 1.0 &&
          out.normalizedAfter[3] == 1.0,
          "COM DFE: nonfinite RSS or inconsistent main cursor");

  Result<ComDfeData> result;
  result.status = Status::Warning;
  result.value = std::move(out);
  result.provenance = std::move(provenance);
  result.provenance.algorithm = "com-120f8-fixed-6tap-symbol-dfe-exploratory";
  result.provenance.version = "012_17-component-1";
  result.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Fixed ideal-decision six-tap DFE on observed rectangular-symbol "
    "response only. No unobserved tail, decision-error propagation, "
    "Annex 93A optimum, statistical COM/DER or PHY PASS/FAIL."});
  return result;
}
} // namespace si::channel
