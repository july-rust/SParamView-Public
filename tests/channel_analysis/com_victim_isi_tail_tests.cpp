#include "si/channel_analysis/com_victim_isi_tail.hpp"
#include "si/channel_analysis/com.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace si;
using namespace si::channel;
namespace {
void need(bool yes, const char* what) {
  if (!yes) throw std::runtime_error(what);
}
void near(double actual, double expected, double tol, const char* what) {
  need(std::isfinite(actual) && std::abs(actual - expected) <= tol, what);
}
template<class Func> void rejects(Func run, const char* what) {
  bool rejected = false;
  try { (void)run(); } catch (const Error&) { rejected = true; }
  need(rejected, what);
}
Provenance identity() {
  Provenance p;
  p.sourceSha256 = std::string(64, 'a');
  p.revision = "revision-23";
  p.mappingIdentity = "confirmed-map-23";
  p.settingsIdentity = "120F8|tx=ffe23|rx=ctle23|";
  p.workspaceGeneration = 23;
  p.mappingGeneration = 7;
  return p;
}
Com120F8RectangularPulseData pulse(
    double cursor, const std::array<double,12>& other) {
  Com120F8RectangularPulseData out;
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  out.symbolPeriodSeconds = ui;
  for (int k = -3; k <= 9; ++k) {
    const double v = k == 0 ? cursor :
         (k < 0 ? other[static_cast<size_t>(k + 3)] :
                  other[static_cast<size_t>(k + 2)]);
    out.timeSeconds.push_back(5e-9 + static_cast<double>(k)*ui);
    out.unitSymbolPulse.push_back(v);
    out.maximumAbsolutePulse = std::max(out.maximumAbsolutePulse,std::abs(v));
  }
  return out;
}
ComVoltageScalingInput preparePulses(
    const Provenance& p, const std::array<double,12>& t1,
    const std::array<double,12>& t2, double cursor1=.8, double cursor2=.5) {
  ComVoltageScalingInput x;
  x.pulses.status = Status::Warning;
  x.pulses.provenance = p;
  x.pulses.provenance.algorithm =
      "com-120f8-two-package-symbol-pulse-exploratory";
  x.pulses.provenance.version = "012_09_03-pulse-1";
  x.pulses.value = Com120F8RectangularPulseCases{
      pulse(cursor1,t1), pulse(cursor2,t2)};
  auto scale = [&](double volts, const char* which, const char* source) {
    si::channel::Result<ComVoltageScaleEvidence> r;
    r.status = Status::Valid;
    r.provenance = p;
    r.provenance.algorithm = "caller-explicit-012-23-scaling";
    r.provenance.version = "1";
    ComVoltageScaleEvidence e;
    e.voltsPerUnitSymbol = volts;
    e.testCase = which;
    e.transmitterReferencePlane = "TX-unit-symbol";
    e.receiverDecisionPlane = "RX-decision";
    e.normalizationIdentity = "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
    e.calibrationIdentity = "test-calibration-contract-23";
    e.calibrationSourceIdentity = source;
    e.txEqualizerIdentity = "ffe23";
    e.rxEqualizerIdentity = "ctle23";
    r.value = e;
    return r;
  };
  x.test1Scale = scale(.2,"TEST1","external-cal-A");
  x.test2Scale = scale(.4,"TEST2","external-cal-B");
  return x;
}
si::channel::Result<ComJointVoltageEvidence> noiseComponent(
    const Provenance& expected, const ComScaledVoltageCase& scaled,
    const char* which, ComJointComponentKind kind, const char* componentId,
    double amplitude) {
  si::channel::Result<ComJointVoltageEvidence> r;
  r.status = Status::Valid;
  r.provenance = expected;
  r.provenance.settingsIdentity += "case=";
  r.provenance.settingsIdentity += which;
  r.provenance.algorithm = "independent-caller-calibrated-noise";
  r.provenance.version = "1";
  ComJointVoltageEvidence e;
  e.amplitudeV = amplitude;
  e.kind = kind;
  e.componentId = componentId;
  e.testCase = which;
  e.referencePlane = scaled.receiverDecisionPlane;
  e.calibrationIdentity = scaled.calibrationIdentity;
  e.assumedIndependent = true;
  e.includesReceiverGaussianNoise =
      kind == ComJointComponentKind::ReceiverGaussian;
  r.value = e;
  return r;
}
ComVictimIsiTailNoiseCase noiseCase(
    const Provenance& p, const ComScaledVoltageCase& scaled,
    const char* which, double rx=.02, double rj=0., double dd=0.) {
  ComVictimIsiTailNoiseCase n;
  n.normalizationIdentity = scaled.normalizationIdentity;
  n.calibrationSourceIdentity = scaled.calibrationSourceIdentity;
  n.txEqualizerIdentity = scaled.txEqualizerIdentity;
  n.rxEqualizerIdentity = scaled.rxEqualizerIdentity;
  n.receiverGaussian = noiseComponent(
      p,scaled,which,ComJointComponentKind::ReceiverGaussian,"rx",rx);
  n.randomJitterGaussian = noiseComponent(
      p,scaled,which,ComJointComponentKind::RandomJitterGaussian,"rj",rj);
  n.dualDiracPeak = noiseComponent(
      p,scaled,which,ComJointComponentKind::DualDiracPeak,"dd",dd);
  return n;
}
ComVictimIsiTailInput inputFrom(
    const Provenance& p, const std::array<double,12>& t1,
    const std::array<double,12>& t2, double cursor1=.8) {
  auto prepared = preparePulses(p,t1,t2,cursor1);
  ComVictimIsiTailInput input;
  input.scaled = scaleCom120F8ReceiverEquivalentVoltage(prepared,p);
  input.test1 = noiseCase(p,input.scaled.value->test1,"TEST1");
  input.test2 = noiseCase(p,input.scaled.value->test2,"TEST2");
  input.oneSidedTailProbability = 1e-5;
  return input;
}
} // namespace

int main() {
 try {
  const auto p = identity();
  // Source-free SciPy erfc/Brent enumeration of 4^n PAM4 states.
  // A: non-cursor signed voltage [-.02, +.04] V; p=1e-5, RX=.02 V.
  std::array<double,12> a{}; a[2]=-.1; a[3]=.2;
  std::array<double,12> b{}; b[2]=.25; b[3]=-.2;
  auto input = inputFrom(p,a,b);
  const auto basic = calculateComVictimIsiTailBridge(input,p);
  need(basic.usable() && basic.status == Status::Warning &&
       basic.diagnostics.size()==1 &&
       basic.diagnostics[0].code=="NOT_IEEE_COM" &&
       basic.provenance.algorithm==
         "com-victim-self-isi-joint-tail-bridge-exploratory" &&
       basic.provenance.version=="012_23-1", "non-normative contract");
  const auto& t1=basic.value->test1;
  const auto& t2=basic.value->test2;
  near(t1.selectedTailQuantileV,.1323069531212122,1e-11,
       "Test1 independent two-tap oracle");
  near(t2.selectedTailQuantileV,.2519709465346396,1e-11,
       "Test2 independent two-tap oracle");
  need(t1.selectedTapCount==2 && t2.selectedTapCount==2 &&
       t1.omittedTapCount==0 && t2.omittedTapCount==0 &&
       t1.statesEvaluated==16 && t2.statesEvaluated==16 &&
       t1.boundMode=="EXACT_ASSUMED_MIXTURE" &&
       !(t1.lowerTailBoundV < t1.selectedTailQuantileV) &&
       !(t1.upperTailBoundV > t1.selectedTailQuantileV) &&
       t1.componentRole=="VICTIM_SELF_ISI" &&
       t1.selectedUiOffsets[0]==1 && t1.selectedUiOffsets[1]==-1 &&
       t2.selectedUiOffsets[0]==-1 && t2.selectedUiOffsets[1]==1 &&
       input.test1.calibrationSourceIdentity !=
          input.test2.calibrationSourceIdentity,
       "two-case source, signed tap selection, state count or role lost");
  near(basic.value->worstCaseExploratoryUpperTailV,
       .2519709465346396,1e-11,"worst exploratory Test upper");

  // C: 12 self-ISI taps; top 6 are enumerated and the remaining 6 are
  // bounded by a triangle-inequality envelope, never quietly discarded.
  std::array<double,12> volts{
    .04,.01,.006,.005,.004,.003,.002,.001,.0005,.0004,.0003,.0002};
  std::array<double,12> twelve{};
  for (size_t i=0;i<12;++i) twelve[i]=volts[i]/.2;
  auto many = inputFrom(p,twelve,b);
  const auto bounded = calculateComVictimIsiTailBridge(many,p);
  const auto& c = bounded.value->test1;
  near(c.selectedTailQuantileV,.1276790914271497,1e-11,
       "six-tap independent 4096-state oracle");
  near(c.omittedTapBoundV,.0044,1e-14,"six omitted taps upper bound");
  near(c.lowerTailBoundV,.1232790914271497,1e-11,
       "conservative lower envelope");
  near(c.upperTailBoundV,.1320790914271497,1e-11,
       "conservative upper envelope");
  need(c.selectedTapCount==6 && c.omittedTapCount==6 &&
       c.statesEvaluated==4096 &&
       c.boundMode=="CONSERVATIVE_BOUNDED_MIXTURE" &&
       c.selectedUiOffsets.size()==6 && c.omittedUiOffsets.size()==6,
       "bounded 12 tap selection/metadata");
  // Statistical and deterministic ISI bounds remain different entities.
  near(c.totalIsiRmsV,
       std::sqrt(5./9.)*std::sqrt(
         .04*.04+.01*.01+.006*.006+.005*.005+.004*.004+.003*.003+
         .002*.002+.001*.001+.0005*.0005+.0004*.0004+
         .0003*.0003+.0002*.0002),1e-13,"full 12-tap RMS preserved");
  // Seven nonzero taps: exactly one omitted; no exponential expansion.
  std::array<double,12> seven{};
  for (size_t i=0;i<7;++i) seven[i]=volts[i]/.2;
  const auto s7=calculateComVictimIsiTailBridge(inputFrom(p,seven,b),p);
  need(s7.value->test1.selectedTapCount==6 &&
       s7.value->test1.omittedTapCount==1 &&
       s7.value->test1.statesEvaluated==4096 &&
       s7.value->test1.omittedTapBoundV>=.002,
       "seven tap boundary");
  // Six nonzero taps: no artificial uncertainty envelope.
  std::array<double,12> six{};
  for (size_t i=0;i<6;++i) six[i]=volts[i]/.2;
  const auto s6=calculateComVictimIsiTailBridge(inputFrom(p,six,b),p);
  need(s6.value->test1.omittedTapCount==0 &&
       s6.value->test1.boundMode=="EXACT_ASSUMED_MIXTURE" &&
       s6.value->test1.statesEvaluated==4096,"six exact tap budget");
  // With no ISI, the joint-tail engine must reduce to Gaussian-only.
  std::array<double,12> zero{};
  const auto z=calculateComVictimIsiTailBridge(inputFrom(p,zero,zero),p);
  near(z.value->test1.selectedTailQuantileV,.0852978158784565,1e-11,
       "Gaussian-only independent oracle");
  need(z.value->test1.selectedTapCount==0 &&
       z.value->test1.statesEvaluated==1,"zero tap state count");
  // Dual-Dirac doubles selected-state count but keeps the 8192-state cap.
  auto dd=inputFrom(p,six,b);
  dd.test1.dualDiracPeak.value->amplitudeV=.015;
  const auto d=calculateComVictimIsiTailBridge(dd,p);
  need(d.value->test1.statesEvaluated==8192,"max 8192 state budget");
  // Global sign reversal leaves the assumed symmetric PAM4 distribution.
  std::array<double,12> reverse{};
  for(size_t i=0;i<12;++i) reverse[i]=-a[i];
  auto rev = inputFrom(p,reverse,b,-.8);
  const auto r=calculateComVictimIsiTailBridge(rev,p);
  near(r.value->test1.selectedTailQuantileV,t1.selectedTailQuantileV,
       1e-12,"sign reversal symmetry");
  // Equal-magnitude taps are selected by increasing UI offset.
  std::array<double,12> tie{};
  tie[0]=.05;tie[1]=-.05;tie[2]=.05;tie[3]=-.05;
  tie[4]=.05;tie[5]=-.05;tie[6]=.05;
  const auto tied=calculateComVictimIsiTailBridge(inputFrom(p,tie,b),p);
  need(tied.value->test1.selectedUiOffsets==
           std::vector<int>({-3,-2,-1,1,2,3}) &&
       tied.value->test1.omittedUiOffsets==std::vector<int>({4}),
       "deterministic tie break");

  auto bad=input;
  bad.scaled.status=Status::Stale;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "stale 012_21 result");
  bad=input; bad.scaled.provenance.sourceSha256[0]='b';
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "source SHA mismatch");
  bad=input; ++bad.scaled.provenance.mappingGeneration;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "stale mapping generation");
  bad=input; bad.scaled.value->test1.equivalentPulseV[2]=.01;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "tampered signed scaled voltage");
  bad=input; bad.scaled.value->test1.testCase="TEST2";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "wrong scaled case");
  bad=input; bad.test1.receiverGaussian.value->referencePlane="other";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "noise plane mismatch");
  bad=input; bad.test2.receiverGaussian.value->calibrationIdentity="other";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "noise calibration mismatch");
  bad=input; bad.test1.calibrationSourceIdentity="not-the-scale-source";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "noise calibration source mismatch");
  bad=input; bad.test2.txEqualizerIdentity="unrelated-TX";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "TX EQ mismatch");
  bad=input; bad.test1.rxEqualizerIdentity="unrelated-RX";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "RX EQ mismatch");
  bad=input; bad.test1.normalizationIdentity="Sdd21=Vout/Vin";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "invalid normalization");
  bad=input; bad.test1.receiverGaussian.provenance.settingsIdentity +=
             "|case=TEST1";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "duplicate case token");
  bad=input; bad.test1.receiverGaussian.provenance.settingsIdentity =
             p.settingsIdentity+"case=TEST2";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "cross-case noise");
  bad=input; bad.test2.randomJitterGaussian.provenance.sourceSha256[0]='f';
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "noise source mismatch");
  bad=input; bad.test1.randomJitterGaussian.value->
                  includesReceiverGaussianNoise=true;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "RX Gaussian double count");
  bad=input; bad.test1.receiverGaussian.value->
                  includesReceiverGaussianNoise=false;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "missing RX noise ownership");
  bad=input; bad.test1.randomJitterGaussian.value->assumedIndependent=false;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "unqualified independence assumption");
  bad=input; bad.test1.dualDiracPeak.value->componentId="rx";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "duplicate noise component ID");
  bad=input; bad.test1.randomJitterGaussian.value->componentId=
             "victim-self-isi-ui--1";
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "noise component must not impersonate ISI tap");
  bad=input; bad.oneSidedTailProbability=0.;
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "unsupported probability");
  bad=input; bad.oneSidedTailProbability=
             std::numeric_limits<double>::quiet_NaN();
  rejects([&]{return calculateComVictimIsiTailBridge(bad,p);},
          "nonfinite probability");
  auto live=p; ++live.workspaceGeneration;
  rejects([&]{return calculateComVictimIsiTailBridge(input,live);},
          "expected live source changed");
  live=p; live.settingsIdentity="120F8|case=TEST1";
  rejects([&]{return calculateComVictimIsiTailBridge(input,live);},
          "pair cannot masquerade as Test1");
  std::atomic_bool cancelled{true};
  bool sawCancel=false;
  try {(void)calculateComVictimIsiTailBridge(input,p,&cancelled);}
  catch(const Cancelled&) {sawCancel=true;}
  need(sawCancel,"explicit cancellation");
  // Keep official COM production API disabled.
  ComSettings settings;
  settings.profileId="IEEE-802.3ck-2022-100GAUI-1-C2C-120F-REFERENCE-CANDIDATE";
  const auto official=calculateCom(ComInput{},settings,p);
  need(official.status==Status::NotEvaluated && !official.value,
       "production COM must remain fail-closed");

  std::cout << "012_23 bounded victim ISI joint tail PASS\n";
  return 0;
 } catch (const std::exception& e) {
  std::cerr << "012_23 bounded victim ISI tail FAIL: " << e.what() << '\n';
  return 1;
 }
}
