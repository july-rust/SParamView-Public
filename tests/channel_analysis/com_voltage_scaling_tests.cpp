#include "si/channel_analysis/com_voltage_scaling.hpp"
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
void need(bool ok,const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* message) {
  need(std::isfinite(a) && std::abs(a-b)<=tol,message);
}
template<class F> void rejects(F fn,const char* message) {
  bool caught=false;
  try {(void)fn();} catch(const Error&) {caught=true;}
  need(caught,message);
}
Provenance identity() {
  Provenance p;
  p.sourceSha256=std::string(64,'a');
  p.revision="rev-21";
  p.mappingIdentity="confirmed-four-port-map";
  p.settingsIdentity="120F8|tx=eq21|rx=ctle21|";
  p.workspaceGeneration=21;
  p.mappingGeneration=4;
  return p;
}
Com120F8RectangularPulseData pulse(double cursor,double pre,double post) {
  Com120F8RectangularPulseData p;
  p.symbolPeriodSeconds=1./final8023ck120F8().symbolRateHz;
  const double decision=5e-9;
  for(int k=-3;k<=9;++k) {
    p.timeSeconds.push_back(decision+k*p.symbolPeriodSeconds);
    const double v=k==-1?pre:k==0?cursor:k==1?post:0.;
    p.unitSymbolPulse.push_back(v);
    p.maximumAbsolutePulse=std::max(p.maximumAbsolutePulse,std::abs(v));
  }
  return p;
}
ComVoltageScalingInput golden(const Provenance& p) {
  ComVoltageScalingInput in;
  in.pulses.status=Status::Warning;
  in.pulses.provenance=p;
  in.pulses.provenance.algorithm=
      "com-120f8-two-package-symbol-pulse-exploratory";
  in.pulses.provenance.version="012_09_03-pulse-1";
  in.pulses.value=Com120F8RectangularPulseCases{
      pulse(.8,-.1,.2),pulse(.5,.25,-.2)};
  auto evidence=[&](double scale,const char* which,const char* source) {
    si::channel::Result<ComVoltageScaleEvidence> r;
    r.status=Status::Valid;
    r.provenance=p;
    r.provenance.algorithm="caller-explicit-voltage-scale";
    r.provenance.version="v1";
    ComVoltageScaleEvidence s;
    s.voltsPerUnitSymbol=scale;
    s.testCase=which;
    s.transmitterReferencePlane="TX-equivalent-unit-symbol";
    s.receiverDecisionPlane="RX-decision-equivalent";
    s.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
    s.calibrationIdentity="calibration-contract-21";
    s.calibrationSourceIdentity=source;
    s.txEqualizerIdentity="tx-ffe-21";
    s.rxEqualizerIdentity="rx-ctle-21";
    r.value=std::move(s);
    return r;
  };
  // Separate calibration-source identities are legal: they are NOT
  // cryptographically verified channel source hashes.
  in.test1Scale=evidence(.2,"TEST1","external-cal-A");
  in.test2Scale=evidence(.4,"TEST2","external-cal-B");
  return in;
}
}
int main() {
 try {
  const auto p=identity();
  const auto input=golden(p);
  const auto result=scaleCom120F8ReceiverEquivalentVoltage(input,p);
  need(result.usable() && result.status==Status::Warning &&
       result.diagnostics.size()==1 &&
       result.diagnostics.front().code=="NOT_IEEE_COM" &&
       result.value->model==
         "EXPLORATORY_RECEIVER_EQUIVALENT_UNIT_SYMBOL_VOLTAGE",
       "scale output must remain exploratory NOT_IEEE_COM");
  const auto& t1=result.value->test1;
  const auto& t2=result.value->test2;
  near(t1.cursorEquivalentV,.16,1e-15,"independent Test1 cursor");
  near(t1.equivalentPulseV[2],-.02,1e-15,"signed precursor");
  near(t1.equivalentPulseV[4],.04,1e-15,"postcursor");
  near(t2.cursorEquivalentV,.20,1e-15,"independent Test2 cursor");
  near(t2.equivalentPulseV[2],.10,1e-15,"Test2 pre");
  near(t2.equivalentPulseV[4],-.08,1e-15,"Test2 post");
  for(size_t k=0;k<13;++k) {
    near(t1.unitSymbolPulse[k],input.pulses.value->test1.unitSymbolPulse[k],
         0.,"unchanged signed pulse");
    near(t1.timeSeconds[k],input.pulses.value->test1.timeSeconds[k],
         0.,"unchanged sample order/time");
  }
  auto doubled=input;
  doubled.test1Scale.value->voltsPerUnitSymbol*=2.;
  near(scaleCom120F8ReceiverEquivalentVoltage(doubled,p)
          .value->test1.cursorEquivalentV,.32,1e-15,
       "linear voltage scale factor");
  near(scaleCom120F8ReceiverEquivalentVoltage(input,p)
          .value->test1.cursorEquivalentV,.16,1e-15,
       "reproducible and no input mutation");

  auto bad=input;
  bad.test2Scale.value->testCase="TEST1";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "mixed Test case");
  bad=input; bad.test1Scale.provenance.sourceSha256[0]='b';
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "channel SHA mismatch");
  bad=input; ++bad.test1Scale.provenance.mappingGeneration;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "mapping generation mismatch");
  bad=input; bad.pulses.provenance.revision="stale";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "revision mismatch");
  bad=input; bad.test1Scale.provenance.settingsIdentity="other-EQ";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "settings mismatch");
  bad=input; bad.pulses.provenance.version.clear();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "missing pulse version");
  bad=input; bad.pulses.provenance.algorithm="other-pulse";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "unqualified pulse algorithm");
  bad=input; bad.test1Scale.provenance.version.clear();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "missing calibration algorithm version");
  bad=input; bad.test1Scale.status=Status::Stale;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "stale evidence");
  bad=input; bad.pulses.value.reset();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "missing pair result");
  bad=input; bad.test2Scale.value->receiverDecisionPlane="different-RX";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "cross-case receiver plane mismatch");
  bad=input; bad.test2Scale.value->rxEqualizerIdentity="wrong-CTLE";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "cross-case RX equalizer mismatch");
  bad=input; bad.test2Scale.value->calibrationIdentity="other-calibration";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "cross-case calibration mismatch");
  bad=input; bad.test1Scale.value->calibrationSourceIdentity.clear();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "missing independent calibration source");
  bad=input; bad.test1Scale.value->normalizationIdentity="UNVERIFIED";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "unknown unit normalization");
  bad=input; bad.test1Scale.value->voltsPerUnitSymbol=0.;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "zero scale");
  bad=input; bad.test1Scale.value->voltsPerUnitSymbol=-.2;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "negative scale");
  bad=input; bad.test1Scale.value->voltsPerUnitSymbol=
      std::numeric_limits<double>::quiet_NaN();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "NaN scale");
  bad=input; bad.test2Scale.value->voltsPerUnitSymbol=
      std::numeric_limits<double>::infinity();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "infinite scale");
  bad=input; bad.pulses.value->test1.unitSymbolPulse[3]=
      std::numeric_limits<double>::max();
  bad.pulses.value->test1.maximumAbsolutePulse=
      std::numeric_limits<double>::max();
  bad.test1Scale.value->voltsPerUnitSymbol=2.;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "overflow in scale multiplication");
  bad=input;
  bad.pulses.value->test1.unitSymbolPulse[5]=
      3.*std::numeric_limits<double>::denorm_min();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "subnormal partially underflowed multiplication");
  bad=input;
  bad.pulses.value->test1.unitSymbolPulse[5]=
      std::numeric_limits<double>::denorm_min();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "underflowed multiplication rounded to zero");
  bad=input; bad.pulses.value->test1.timeSeconds[2]+=5e-12;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "non-UI-spaced time axis");
  bad=input;
  // Large absolute cursor time must not make a two-UI grid pass.
  for(auto* sample : {&bad.pulses.value->test1,
                      &bad.pulses.value->test2}) {
    for(int k=-3;k<=9;++k)
      sample->timeSeconds[static_cast<size_t>(k+3)]=
          1. + k*2.*sample->symbolPeriodSeconds;
  }
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "large absolute cursor weakens UI spacing tolerance");
  bad=input; bad.pulses.value->test1.maximumAbsolutePulse=4.;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "invalid pulse max");
  bad=input;
  for(double& v:bad.pulses.value->test1.unitSymbolPulse) v*=1e-12;
  bad.test1Scale.value->voltsPerUnitSymbol=1e12;
  bad.pulses.value->test1.maximumAbsolutePulse=0.;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "tiny nonzero pulse with claimed zero maximum must reject");
  bad.pulses.value->test1.maximumAbsolutePulse=.8e-12;
  near(scaleCom120F8ReceiverEquivalentVoltage(bad,p)
           .value->test1.cursorEquivalentV,.8,1e-14,
       "tiny consistent pulse with large scale is valid");
  bad=input;
  for(double& v:bad.pulses.value->test1.unitSymbolPulse) v=0.;
  bad.pulses.value->test1.maximumAbsolutePulse=.1;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "all-zero pulse cannot report nonzero maximum");
  bad=input; bad.pulses.value->test1.unitSymbolPulse.pop_back();
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "invalid pulse tap count");
  bad=input; bad.pulses.value->test2.timeSeconds[3]+=1e-12;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(bad,p);},
          "case timing mismatch");
  auto wrong=p; ++wrong.workspaceGeneration;
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(input,wrong);},
          "live source generation changed");
  wrong=p; wrong.settingsIdentity="120F8|case=TEST1|";
  rejects([&]{return scaleCom120F8ReceiverEquivalentVoltage(input,wrong);},
          "pair must not masquerade as single test");
  std::atomic_bool cancel{true};
  bool cancelled=false;
  try {(void)scaleCom120F8ReceiverEquivalentVoltage(input,p,&cancel);}
  catch(const Cancelled&) {cancelled=true;}
  need(cancelled,"cancel requested before numerical work");
  const auto gate=calculateCom(ComInput{},ComSettings{});
  need(gate.status==Status::NotEvaluated && !gate.value,
       "production COM gate must stay closed");
  std::cout<<"012_21 signed unit-symbol voltage scaling goldens / fail-closed: PASS\n";
  return 0;
 } catch (const std::exception& e) {
  std::cerr<<"012_21 FAIL: "<<e.what()<<"\n";
  return 1;
 }
}
