#include "si/channel_analysis/com_victim_isi.hpp"
#include "si/channel_analysis/com.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace si;
using namespace si::channel;
namespace {
void need(bool yes, const char* why) {
  if (!yes) throw std::runtime_error(why);
}
void near(double actual, double golden, double tolerance, const char* why) {
  need(std::isfinite(actual) && std::abs(actual - golden) <= tolerance, why);
}
template<class Func>
void rejects(Func&& run, const char* why) {
  bool found = false;
  try { (void)run(); } catch (const si::Error&) { found = true; }
  need(found, why);
}
Provenance identity() {
  Provenance p;
  p.sourceSha256 = std::string(64, 'a');
  p.revision = "revision-22";
  p.mappingIdentity = "confirmed-map-22";
  p.settingsIdentity = "120F8|tx=ffe22|rx=ctle22|";
  p.workspaceGeneration = 22;
  p.mappingGeneration = 5;
  return p;
}
Com120F8RectangularPulseData pulse(double cursor, double precursor,
                                   double postcursor) {
  Com120F8RectangularPulseData data;
  data.symbolPeriodSeconds = 1. / final8023ck120F8().symbolRateHz;
  for (int k = -3; k <= 9; ++k) {
    data.timeSeconds.push_back(5e-9 + k * data.symbolPeriodSeconds);
    const double v = k == 0 ? cursor : k == -1 ? precursor :
                     k == 1 ? postcursor : 0.;
    data.unitSymbolPulse.push_back(v);
    data.maximumAbsolutePulse =
        std::max(data.maximumAbsolutePulse, std::abs(v));
  }
  return data;
}
ComVoltageScalingInput prepareInput(const Provenance& p) {
  ComVoltageScalingInput input;
  input.pulses.status = Status::Warning;
  input.pulses.provenance = p;
  input.pulses.provenance.algorithm =
      "com-120f8-two-package-symbol-pulse-exploratory";
  input.pulses.provenance.version = "012_09_03-pulse-1";
  input.pulses.value = Com120F8RectangularPulseCases{
      pulse(.8, -.1, .2), pulse(.5, .25, -.2)};
  auto makeScale = [&](double v, const char* which, const char* source) {
    si::channel::Result<ComVoltageScaleEvidence> result;
    result.status = Status::Valid;
    result.provenance = p;
    result.provenance.algorithm = "caller-explicit-voltage-scale";
    result.provenance.version = "v1";
    ComVoltageScaleEvidence evidence;
    evidence.voltsPerUnitSymbol = v;
    evidence.testCase = which;
    evidence.transmitterReferencePlane = "TX-unit-symbol";
    evidence.receiverDecisionPlane = "RX-decision";
    evidence.normalizationIdentity =
        "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
    evidence.calibrationIdentity = "caller-calibration-contract-22";
    evidence.calibrationSourceIdentity = source;
    evidence.txEqualizerIdentity = "ffe22";
    evidence.rxEqualizerIdentity = "ctle22";
    result.value = std::move(evidence);
    return result;
  };
  input.test1Scale = makeScale(.2, "TEST1", "external-cal-A");
  input.test2Scale = makeScale(.4, "TEST2", "external-cal-B");
  return input;
}
}

int main() {
  try {
    const Provenance p = identity();
    auto original = prepareInput(p);
    const auto scaled = scaleCom120F8ReceiverEquivalentVoltage(original, p);
    const auto result = calculateComVictimIsi(scaled, p);
    need(result.status == Status::Warning && result.usable() &&
         result.diagnostics.size() == 1 &&
         result.diagnostics.front().code == "NOT_IEEE_COM" &&
         result.value->model ==
             "EXPLORATORY_TWO_CASE_RECEIVER_VOLTAGE_VICTIM_ISI" &&
         result.provenance.algorithm == "com-victim-self-isi-exploratory" &&
         result.provenance.version == "012_22-1",
         "012_22 output must be explicitly exploratory");
    const auto& a = result.value->test1;
    const auto& b = result.value->test2;
    // Independently derived from E[L]=0, E[L^2]=5/9 for equiprobable
    // PAM4 levels (-1,-1/3,+1/3,+1). The two ISI taps are -0.02,+0.04 V
    // in Test1 and +0.10,-0.08 V in Test2.
    near(a.cursorSignedV, .16, 1e-15, "Test1 cursor");
    near(a.cursorMagnitudeV, .16, 1e-15, "Test1 magnitude");
    near(a.precursorRmsV, std::sqrt(5./9.) * .02, 1e-15, "Test1 pre RMS");
    near(a.postcursorRmsV, std::sqrt(5./9.) * .04, 1e-15, "Test1 post RMS");
    near(a.totalIsiRmsV, .03333333333333333, 1e-15, "Test1 total RMS");
    near(a.worstCaseIsiBoundV, .06, 1e-15, "Test1 deterministic bound");
    near(a.nominalPam4HalfSpacingV, .16/3., 1e-15,
         "Test1 nominal half spacing");
    near(b.cursorSignedV, .20, 1e-15, "Test2 cursor");
    near(b.precursorRmsV, std::sqrt(5./9.) * .10, 1e-15, "Test2 pre RMS");
    near(b.postcursorRmsV, std::sqrt(5./9.) * .08, 1e-15, "Test2 post RMS");
    near(b.totalIsiRmsV, .09545214042184236, 1e-15, "Test2 total RMS");
    near(b.worstCaseIsiBoundV, .18, 1e-15, "Test2 deterministic bound");
    near(b.nominalPam4HalfSpacingV, .20/3., 1e-15,
         "Test2 nominal half spacing");
    need(a.componentRole == "VICTIM_SELF_ISI" &&
         b.componentRole == "VICTIM_SELF_ISI" &&
         a.testCase == "TEST1" && b.testCase == "TEST2" &&
         a.calibrationSourceIdentity != b.calibrationSourceIdentity,
         "role, case isolation or per-case calibration source lost");

    // No mutation of upstream result: all samples and provenance remain.
    for (size_t i = 0; i < 13; ++i)
      near(scaled.value->test1.equivalentPulseV[i],
           original.pulses.value->test1.unitSymbolPulse[i] * .2,
           1e-15, "012_21 source must remain intact");

    auto bad = scaled;
    bad.value->test1.unitSymbolPulse[2] = -.2;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "independent signed unit-symbol reconciliation");
    bad = scaled;
    bad.value->test1.equivalentPulseV[2] = -.01;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "tampered voltage must fail");
    bad = scaled;
    bad.value->test1.cursorEquivalentV = .8;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "cursor report mismatch");
    bad = scaled;
    bad.value->test1.maximumAbsoluteEquivalentV = 0.;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "invalid reported voltage maximum");
    bad = scaled;
    bad.value->test1.testCase = "TEST2";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "mixed test identity");
    bad = scaled;
    bad.value->test2.receiverDecisionPlane = "other-RX";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "different receiver decision plane");
    bad = scaled;
    bad.value->test2.calibrationIdentity = "other-cal";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "different calibration");
    bad = scaled;
    bad.value->test2.txEqualizerIdentity = "wrong-TX";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "different TX EQ");
    bad = scaled;
    bad.value->test2.rxEqualizerIdentity = "wrong-RX";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "different RX EQ");
    bad = scaled;
    bad.value->test1.normalizationIdentity = "Vout/Vin";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "unknown normalization identity");
    bad = scaled;
    bad.value->test2.voltsPerUnitSymbol = -1.;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "negative scale");
    bad = scaled;
    bad.value->test1.voltsPerUnitSymbol =
        std::numeric_limits<double>::quiet_NaN();
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "NaN scale");
    bad = scaled;
    bad.value->test1.timeSeconds[2] += 2e-12;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "invalid absolute UI grid");
    bad = scaled;
    bad.value->test2.timeSeconds[3] += 1e-12;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "different test sampling time");
    bad = scaled;
    bad.value->test1.equivalentPulseV[4] =
        std::numeric_limits<double>::infinity();
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "nonfinite sample");
    bad = scaled;
    bad.value->test1.equivalentPulseV[2] =
        std::numeric_limits<double>::denorm_min();
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "subnormal tampering");
    bad = scaled;
    bad.value->test1.unitSymbolPulse[3] = 0.;
    bad.value->test1.equivalentPulseV[3] = 0.;
    bad.value->test1.cursorEquivalentV = 0.;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "zero cursor must fail closed");

    bad = scaled;
    bad.status = Status::Stale;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "stale upstream");
    bad = scaled;
    bad.status = Status::Valid;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "unqualified upstream status");
    bad = scaled;
    bad.diagnostics.clear();
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "upstream missing NOT_IEEE_COM warning");
    bad = scaled;
    bad.provenance.algorithm = "unqualified-physical-voltage";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "unqualified algorithm");
    bad = scaled;
    bad.provenance.version = "unqualified";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "unqualified version");
    bad = scaled;
    bad.value->model = "IEEE_COM";
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "unqualified upstream model");
    bad = scaled;
    bad.provenance.sourceSha256[0] = 'b';
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "SHA mismatch");
    bad = scaled;
    ++bad.provenance.mappingGeneration;
    rejects([&] {return calculateComVictimIsi(bad, p);},
            "stale generation");
    auto expected = p;
    ++expected.workspaceGeneration;
    rejects([&] {return calculateComVictimIsi(scaled, expected);},
            "expected live source generation mismatch");
    expected = p;
    expected.settingsIdentity = "120F8|case=TEST1";
    rejects([&] {return calculateComVictimIsi(scaled, expected);},
            "pair must not claim a single test case");

    auto zero = scaled;
    for (size_t i = 0; i < 13; ++i) {
      if (i == 3) continue;
      zero.value->test1.unitSymbolPulse[i] = 0.;
      zero.value->test1.equivalentPulseV[i] = 0.;
    }
    const auto noIsi = calculateComVictimIsi(zero, p);
    near(noIsi.value->test1.totalIsiRmsV, 0., 0., "zero ISI RMS");
    near(noIsi.value->test1.worstCaseIsiBoundV, 0., 0.,
         "zero ISI deterministic bound");

    auto reversed = scaled;
    for (size_t i = 0; i < 13; ++i) {
      reversed.value->test1.unitSymbolPulse[i] *= -1.;
      reversed.value->test1.equivalentPulseV[i] *= -1.;
    }
    reversed.value->test1.cursorEquivalentV *= -1.;
    const auto invert = calculateComVictimIsi(reversed, p);
    near(invert.value->test1.cursorSignedV, -.16, 1e-15,
         "cursor retains sign");
    near(invert.value->test1.totalIsiRmsV, .03333333333333333,
         1e-15, "RMS invariant to polarity");
    near(invert.value->test1.nominalPam4HalfSpacingV,
         .16/3., 1e-15, "half spacing invariant to polarity");

    auto tiny = prepareInput(p);
    tiny.test1Scale.value->voltsPerUnitSymbol = 1e-200;
    tiny.test2Scale.value->voltsPerUnitSymbol = 1e-200;
    const auto tinyScaled =
        scaleCom120F8ReceiverEquivalentVoltage(tiny, p);
    const auto tinyIsi = calculateComVictimIsi(tinyScaled, p);
    need(tinyIsi.value->test1.totalIsiRmsV > 0. &&
         std::isfinite(tinyIsi.value->test1.totalIsiRmsV) &&
         tinyIsi.value->test1.totalIsiRmsV < 1e-198,
         "very small representable voltage must not be rounded to zero");

    // Explicit enormous representable sample values must not overflow bounds
    // into an apparently valid answer.
    auto enormous = prepareInput(p);
    enormous.test1Scale.value->voltsPerUnitSymbol = 1e308;
    for (size_t i = 0; i < 13; ++i) {
      enormous.pulses.value->test1.unitSymbolPulse[i] = 1.;
    }
    enormous.pulses.value->test1.maximumAbsolutePulse = 1.;
    rejects([&] {
      auto hugeScaled = scaleCom120F8ReceiverEquivalentVoltage(enormous, p);
      return calculateComVictimIsi(hugeScaled, p);
    }, "large ISI bound must fail closed");

    std::atomic_bool cancelled{true};
    bool caughtCancel = false;
    try {(void)calculateComVictimIsi(scaled, p, &cancelled);}
    catch (const si::Cancelled&) {caughtCancel = true;}
    need(caughtCancel, "cancellation must not be converted into success");
    const auto gate = calculateCom(ComInput{}, ComSettings{});
    need(gate.status == Status::NotEvaluated && !gate.value,
         "production COM must remain gated");
    std::cout << "012_22 independent victim ISI goldens / fail-closed: PASS\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "012_22 FAIL: " << ex.what() << "\n";
    return 1;
  }
}
