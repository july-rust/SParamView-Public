#include "si/channel_analysis/com_voltage_scaling.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <string_view>
#include <utility>

namespace si::channel {
namespace {
void require(bool condition, const char* why) {
  if (!condition) throw si::Error(why);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool qualifiedIdentity(const Provenance& p) {
  return p.sourceSha256.size() == 64 &&
      std::all_of(p.sourceSha256.begin(), p.sourceSha256.end(),
        [](unsigned char c){return std::isxdigit(c) != 0;}) &&
      !p.revision.empty() && !p.mappingIdentity.empty() &&
      !p.settingsIdentity.empty() && p.workspaceGeneration != 0 &&
      p.mappingGeneration != 0;
}
bool sameChannel(const Provenance& a, const Provenance& b) {
  return a.sourceSha256 == b.sourceSha256 &&
      a.revision == b.revision &&
      a.mappingIdentity == b.mappingIdentity &&
      a.settingsIdentity == b.settingsIdentity &&
      a.workspaceGeneration == b.workspaceGeneration &&
      a.mappingGeneration == b.mappingGeneration;
}
void validateResult(const Provenance& actual, const Provenance& expected,
                    const char* why) {
  require(sameChannel(actual, expected) &&
      !actual.algorithm.empty() && !actual.version.empty(), why);
}
void validatePulse(const Com120F8RectangularPulseData& p) {
  constexpr size_t kTapCount=13;
  require(p.timeSeconds.size()==kTapCount &&
          p.unitSymbolPulse.size()==kTapCount &&
          std::isfinite(p.symbolPeriodSeconds) &&
          p.symbolPeriodSeconds > 0. &&
          std::isfinite(p.maximumAbsolutePulse) &&
          p.maximumAbsolutePulse >= 0.,
          "012_21: malformed 13-symbol pulse");
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  require(std::abs(p.symbolPeriodSeconds-ui)<=1e-10*ui,
          "012_21: symbol rate mismatch");
  const double cursorTime=p.timeSeconds[3];
  require(std::isfinite(cursorTime) && cursorTime>=0.,
          "012_21: invalid decision time");
  // Keep absolute-time floating-point granularity and spacing errors
  // bounded by the UI, not by the potentially huge cursor timestamp.
  const double timingTolerance=1e-6*ui;
  const double nextTime=std::nextafter(
      cursorTime,std::numeric_limits<double>::infinity());
  require(std::isfinite(nextTime) &&
          nextTime-cursorTime<=timingTolerance*0.5,
          "012_21: absolute decision time has inadequate UI resolution");
  double observedMax=0.;
  for(size_t i=0; i<kTapCount; ++i) {
    const double t=p.timeSeconds[i], v=p.unitSymbolPulse[i];
    require(std::isfinite(t) && t>=0. && std::isfinite(v),
            "012_21: nonfinite pulse sample");
    if(i) require(t>p.timeSeconds[i-1],
                  "012_21: pulse time axis not increasing");
    const double relative=t-cursorTime;
    const double nominal=(static_cast<double>(i)-3.)*ui;
    require(std::abs(relative-nominal)<=timingTolerance,
            "012_21: pulse is not sampled at -3..+9 UI");
    observedMax=std::max(observedMax,std::abs(v));
  }
  // A unit-symbol pulse may be arbitrarily small: an absolute 1e-10
  // floor would admit a false zero maximum for a nonzero waveform.
  if(std::fpclassify(observedMax)==FP_ZERO) {
    require(std::fpclassify(p.maximumAbsolutePulse)==FP_ZERO,
            "012_21: all-zero pulse reports nonzero maximum");
  } else {
    require(p.maximumAbsolutePulse>0. &&
            std::abs(observedMax-p.maximumAbsolutePulse)/
                std::max(observedMax,p.maximumAbsolutePulse)<=1e-10,
            "012_21: pulse maximum differs by relative magnitude");
  }
}
void validateScale(const Result<ComVoltageScaleEvidence>& r,
                   const Provenance& expected, const char* which) {
  require(r.usable(), "012_21: missing or stale scale evidence");
  validateResult(r.provenance,expected,
                 "012_21: voltage scale channel/settings provenance mismatch");
  const auto& s=*r.value;
  require(s.testCase==which,
          "012_21: mixed or incorrect Test1/Test2 scale");
  require(std::isnormal(s.voltsPerUnitSymbol) &&
          s.voltsPerUnitSymbol>0.,
          "012_21: voltage scale must be positive finite normal volts");
  require(!s.transmitterReferencePlane.empty() &&
          !s.receiverDecisionPlane.empty() &&
          !s.normalizationIdentity.empty() &&
          !s.calibrationIdentity.empty() &&
          !s.calibrationSourceIdentity.empty() &&
          !s.txEqualizerIdentity.empty() && !s.rxEqualizerIdentity.empty(),
          "012_21: scale normalization/plane/calibration/EQ is undefined");
  // The only supported mathematical unit contract in this bounded step.
  require(s.normalizationIdentity==
          "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL",
          "012_21: unknown voltage normalization contract");
}
ComScaledVoltageCase scale(const Com120F8RectangularPulseData& p,
                           const ComVoltageScaleEvidence& s,
                           const std::atomic_bool* cancelled) {
  ComScaledVoltageCase out;
  out.testCase=s.testCase;
  out.voltsPerUnitSymbol=s.voltsPerUnitSymbol;
  out.transmitterReferencePlane=s.transmitterReferencePlane;
  out.receiverDecisionPlane=s.receiverDecisionPlane;
  out.normalizationIdentity=s.normalizationIdentity;
  out.calibrationIdentity=s.calibrationIdentity;
  out.calibrationSourceIdentity=s.calibrationSourceIdentity;
  out.txEqualizerIdentity=s.txEqualizerIdentity;
  out.rxEqualizerIdentity=s.rxEqualizerIdentity;
  for(size_t i=0;i<out.unitSymbolPulse.size();++i) {
    check(cancelled);
    out.timeSeconds[i]=p.timeSeconds[i];
    out.unitSymbolPulse[i]=p.unitSymbolPulse[i];
    const double scaled=s.voltsPerUnitSymbol*p.unitSymbolPulse[i];
    // Reject both complete underflow to zero and partial underflow to
    // subnormal results, whose relative rounding error may be enormous.
    require(std::isfinite(scaled) &&
            (std::fpclassify(p.unitSymbolPulse[i])==FP_ZERO ||
             std::isnormal(scaled)),
            "012_21: receiver equivalent voltage overflow or underflow");
    out.equivalentPulseV[i]=scaled;
    out.maximumAbsoluteEquivalentV=
        std::max(out.maximumAbsoluteEquivalentV,std::abs(scaled));
  }
  out.cursorEquivalentV=out.equivalentPulseV[3];
  return out;
}
} // namespace

Result<ComScaledVoltageCases> scaleCom120F8ReceiverEquivalentVoltage(
    const ComVoltageScalingInput& input, const Provenance& expected,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  require(qualifiedIdentity(expected),
          "012_21: missing valid channel SHA/revision/mapping/settings/generation");
  // A paired result is a shared-source aggregate, not a single case token.
  require(expected.settingsIdentity.find("|case=")==std::string::npos,
          "012_21: pair settings must not claim one test case");
  require(input.pulses.usable(),
          "012_21: upstream paired pulses are unavailable");
  validateResult(input.pulses.provenance,expected,
                 "012_21: pulse source identity mismatch");
  require(input.pulses.provenance.algorithm==
            "com-120f8-two-package-symbol-pulse-exploratory" &&
          input.pulses.provenance.version=="012_09_03-pulse-1",
          "012_21: unqualified pulse algorithm/version");
  const auto& pulses=*input.pulses.value;
  validatePulse(pulses.test1);
  validatePulse(pulses.test2);
  const double pairTimeTolerance=
      1e-6/final8023ck120F8().symbolRateHz;
  for(size_t i=0;i<13;++i)
    require(std::abs(pulses.test1.timeSeconds[i]-
                     pulses.test2.timeSeconds[i])<=pairTimeTolerance,
            "012_21: Test1/Test2 decision-time grids disagree");
  validateScale(input.test1Scale,expected,"TEST1");
  validateScale(input.test2Scale,expected,"TEST2");
  const auto& a=*input.test1Scale.value;
  const auto& b=*input.test2Scale.value;
  require(a.transmitterReferencePlane==b.transmitterReferencePlane &&
          a.receiverDecisionPlane==b.receiverDecisionPlane &&
          a.normalizationIdentity==b.normalizationIdentity &&
          a.calibrationIdentity==b.calibrationIdentity &&
          a.txEqualizerIdentity==b.txEqualizerIdentity &&
          a.rxEqualizerIdentity==b.rxEqualizerIdentity,
          "012_21: incompatible Test1/Test2 voltage calibration/EQ planes");
  check(cancelled);
  ComScaledVoltageCases data;
  data.test1=scale(pulses.test1,a,cancelled);
  data.test2=scale(pulses.test2,b,cancelled);
  check(cancelled);
  Result<ComScaledVoltageCases> result;
  result.status=Status::Warning;
  result.value=std::move(data);
  result.provenance=expected;
  result.provenance.algorithm="com-receiver-equivalent-unit-symbol-scaling";
  result.provenance.version="012_21-1";
  result.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Signed unit-symbol pulses are scaled into EXPLORATORY receiver-equivalent "
    "volts using caller-declared evidence. Calibration identity is not physical "
    "proof, Sdd21 is not assumed to equal Vout/Vin. No IEEE COM/PHY verdict."});
  return result;
}
} // namespace si::channel
