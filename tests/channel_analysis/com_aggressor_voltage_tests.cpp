#include "si/channel_analysis/com_aggressor_voltage.hpp"
#include "si/channel_analysis/com.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace si;
using namespace si::channel;
namespace {
void need(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
void near(double actual, double expected, double tolerance, const char* why) {
  need(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, why);
}
template<class F> void rejects(F f, const char* why) {
  bool caught = false;
  try { (void)f(); } catch (const si::Error&) { caught = true; }
  need(caught, why);
}
Provenance source() {
  Provenance p;
  p.sourceSha256 = std::string(64,'a');
  p.revision = "012-24-test";
  p.mappingIdentity = "confirmed-mapping";
  p.settingsIdentity = "120F8|tx=ffe|rx=ctle|";
  p.workspaceGeneration = 24;
  p.mappingGeneration = 8;
  return p;
}
SourceIdentity liveFrom(const Provenance& p) {
  return {p.sourceSha256,p.revision,p.mappingIdentity,
          p.workspaceGeneration,p.mappingGeneration};
}
Com120F8RectangularPulseData victimPulse(double cursor) {
  Com120F8RectangularPulseData x;
  x.symbolPeriodSeconds = 1. / final8023ck120F8().symbolRateHz;
  for (int k=-3; k<=9; ++k) {
    x.timeSeconds.push_back(5e-9 + k * x.symbolPeriodSeconds);
    const double u = k == 0 ? cursor : 0.;
    x.unitSymbolPulse.push_back(u);
    x.maximumAbsolutePulse = std::max(x.maximumAbsolutePulse,std::abs(u));
  }
  return x;
}
Result<ComVoltageScaleEvidence> scale(const Provenance& p,
                                     const char* test, double gain) {
  Result<ComVoltageScaleEvidence> r;
  r.status = Status::Valid;
  r.provenance = p;
  r.provenance.algorithm = "independent-fixture-scale";
  r.provenance.version = "1";
  ComVoltageScaleEvidence v;
  v.testCase=test;
  v.voltsPerUnitSymbol=gain;
  v.transmitterReferencePlane="TX-Victim";
  v.receiverDecisionPlane="RX-Decision";
  v.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  v.calibrationIdentity="RX-Cal-24";
  v.calibrationSourceIdentity=std::string("Victim-Cal-")+test;
  v.txEqualizerIdentity="FFE-Victim";
  v.rxEqualizerIdentity="CTLE-RX";
  r.value=v;
  return r;
}
ComInterferencePulse aggressor(const std::string& id, ComCoupling c,
                               double u1, double u2) {
  ComInterferencePulse x;
  x.id=id;
  x.coupling=c;
  x.unitSymbolPulse.assign(13, 0.);
  x.unitSymbolPulse[2]=u1;
  x.unitSymbolPulse[4]=u2;
  x.maximumAbsolutePulse=std::max(std::abs(u1),std::abs(u2));
  return x;
}
Result<ComAggressorVoltageEvidence> calibration(
    const Provenance& p, const char* test, const std::string& id,
    ComCoupling coupling, double gain) {
  Result<ComAggressorVoltageEvidence> r;
  r.status=Status::Valid;
  r.provenance=p;
  r.provenance.settingsIdentity += "case=";
  r.provenance.settingsIdentity += test;
  r.provenance.algorithm="independent-fixture-aggressor-cal";
  r.provenance.version="1";
  ComAggressorVoltageEvidence v;
  v.aggressorId=id;
  v.testCase=test;
  v.coupling=coupling;
  v.voltsPerUnitSymbol=gain;
  v.transmitterReferencePlane="TX-"+id;
  v.receiverDecisionPlane="RX-Decision";
  v.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  v.calibrationIdentity="RX-Cal-24";
  v.calibrationSourceIdentity="independent-"+id+"-"+test;
  v.txEqualizerIdentity="TxEQ-"+id;
  v.rxEqualizerIdentity="CTLE-RX";
  r.value=v;
  return r;
}
ComAggressorVoltageInput fixture(const Provenance& p, size_t n=2) {
  ComVoltageScalingInput v;
  v.pulses.status=Status::Warning;
  v.pulses.provenance=p;
  v.pulses.provenance.algorithm =
      "com-120f8-two-package-symbol-pulse-exploratory";
  v.pulses.provenance.version="012_09_03-pulse-1";
  v.pulses.value=Com120F8RectangularPulseCases{
      victimPulse(.8),victimPulse(.5)};
  v.test1Scale=scale(p,"TEST1",.2);
  v.test2Scale=scale(p,"TEST2",.4);
  ComAggressorVoltageInput input;
  input.victim=scaleCom120F8ReceiverEquivalentVoltage(v,p);
  input.interference.status=Status::Warning;
  input.interference.provenance=p;
  input.interference.provenance.algorithm=
      "com-120f8-separated-test1-test2-interference";
  input.interference.provenance.version="012_14-1";
  input.interference.diagnostics.push_back(
      {Severity::Warning,"NOT_IEEE_COM","Synthetic 012_14 contract fixture"});
  ComInterferenceTestCases x;
  for (auto* c : {&x.test1,&x.test2}) {
    c->symbolPeriodSeconds = 1. / final8023ck120F8().symbolRateHz;
    c->timeSeconds=input.victim.value->test1.timeSeconds.empty() ?
       std::vector<double>{} : std::vector<double>(
       input.victim.value->test1.timeSeconds.begin(),
       input.victim.value->test1.timeSeconds.end());
  }
  for (size_t i=0;i<n;++i) {
    const std::string id="aggressor-"+std::to_string(i);
    const ComCoupling c=i%2==0 ? ComCoupling::Next : ComCoupling::Fext;
    const double a=i==0 ? -.25 : .3;
    const double b=i==0 ? .5 : -.2;
    x.test1.aggressors.push_back(aggressor(id,c,a,b));
    x.test2.aggressors.push_back(aggressor(id,c,-a,-b));
    const double gain1=i==0 ? .4 : .6;
    const double gain2=i==0 ? .8 : .3;
    input.test1.calibrations.push_back(calibration(p,"TEST1",id,c,gain1));
    input.test2.calibrations.push_back(calibration(p,"TEST2",id,c,gain2));
  }
  input.interference.value=std::move(x);
  return input;
}
} // namespace
int main() {
 try {
  const auto p=source(), live=liveFrom(p);
  const auto result=calculateComAggressorReceiverVoltage(fixture(p),p,live);
  need(result.usable() && result.status==Status::Warning &&
       result.value->model==
         "EXPLORATORY_TWO_CASE_AGGRESSOR_RECEIVER_VOLTAGE" &&
       result.provenance.version=="012_24-1" &&
       result.diagnostics.size()==1 &&
       result.diagnostics.front().code=="NOT_IEEE_COM",
       "explicit exploratory contract");
  const auto& a=result.value->test1.aggressors;
  const auto& b=result.value->test2.aggressors;
  need(a.size()==2 && b.size()==2 && a[0].id=="aggressor-0" &&
       a[1].coupling==ComCoupling::Fext &&
       b[0].testCase=="TEST2", "deterministic identity and coupling");
  // Independent multiplication golden; no production scaling function used.
  near(a[0].equivalentPulseV[2],-.1,1e-15,"Test1 negative NEXT");
  near(a[0].equivalentPulseV[4], .2,1e-15,"Test1 positive NEXT");
  near(a[1].equivalentPulseV[2], .18,1e-15,"Test1 FEXT");
  near(a[1].equivalentPulseV[4],-.12,1e-15,"Test1 FEXT polarity");
  near(b[0].equivalentPulseV[2], .2,1e-15,"Test2 NEXT");
  near(b[0].equivalentPulseV[4],-.4,1e-15,"Test2 NEXT");
  near(b[1].equivalentPulseV[2],-.09,1e-15,"Test2 FEXT");
  near(a[0].maximumAbsoluteEquivalentV,.2,1e-15,"output peak");
  const auto zero=calculateComAggressorReceiverVoltage(fixture(p,0),p,live);
  need(zero.usable() && zero.value->test1.aggressors.empty() &&
       zero.value->test2.aggressors.empty(), "zero aggressors");
  auto max32=calculateComAggressorReceiverVoltage(fixture(p,32),p,live);
  need(max32.value->test1.aggressors.size()==32, "32 aggressor boundary");
  rejects([&]{return calculateComAggressorReceiverVoltage(fixture(p,33),p,live);},
          "33 aggressors must reject");
  auto reordered=fixture(p);
  std::reverse(reordered.interference.value->test1.aggressors.begin(),
               reordered.interference.value->test1.aggressors.end());
  std::reverse(reordered.test1.calibrations.begin(),
               reordered.test1.calibrations.end());
  const auto sorted=calculateComAggressorReceiverVoltage(reordered,p,live);
  need(sorted.value->test1.aggressors[0].id==a[0].id &&
       sorted.value->test1.aggressors[0].equivalentPulseV==a[0].equivalentPulseV,
       "order independent output");
  auto bad=fixture(p);
  bad.test1.calibrations[0].value->voltsPerUnitSymbol=
      std::numeric_limits<double>::quiet_NaN();
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "NaN scale");
  bad=fixture(p);
  bad.test2.calibrations[1].value->voltsPerUnitSymbol=0.;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "zero scale");
  bad=fixture(p);
  bad.test1.calibrations[0].value->voltsPerUnitSymbol=
      std::numeric_limits<double>::max();
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "overflow scale");
  bad=fixture(p);
  bad.test1.calibrations[0].value->voltsPerUnitSymbol=
      std::numeric_limits<double>::min();
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "underflow scale");
  bad=fixture(p);
  bad.test1.calibrations[0].value->coupling=ComCoupling::Fext;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "coupling mismatch");
  bad=fixture(p);
  bad.test1.calibrations[0].value->aggressorId="aggressor-1";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "duplicate calibration id");
  bad=fixture(p);
  bad.test1.calibrations.pop_back();
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "missing calibration");
  bad=fixture(p);
  bad.test1.calibrations[0].value->rxEqualizerIdentity="wrong";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "RX EQ mismatch");
  bad=fixture(p);
  bad.test1.calibrations[0].value->receiverDecisionPlane="wrong";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "RX plane mismatch");
  bad=fixture(p);
  bad.test1.calibrations[0].value->normalizationIdentity="wrong";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "normalization mismatch");
  bad=fixture(p);
  bad.test1.calibrations[0].value->calibrationSourceIdentity="";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "missing calibration source");
  bad=fixture(p);
  bad.test1.calibrations[0].value->txEqualizerIdentity="";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "missing TX condition");
  bad=fixture(p);
  bad.test2.calibrations[0].provenance.settingsIdentity += "case=TEST1";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "cross-case provenance");
  bad=fixture(p);
  bad.test1.calibrations[0].provenance.workspaceGeneration++;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "calibration stale generation");
  bad=fixture(p);
  bad.interference.provenance.sourceSha256=std::string(64,'b');
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "interference mismatched source");
  bad=fixture(p);
  bad.interference.status=Status::Stale;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "interference stale status");
  bad=fixture(p);
  bad.interference.value->test1.timeSeconds[4]+=1e-12;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "misaligned time grid");
  bad=fixture(p);
  bad.interference.value->test1.aggressors[0].maximumAbsolutePulse=.1;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "tampered upstream peak");
  bad=fixture(p);
  bad.interference.value->test2.aggressors[0].id="different";
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "Test1/Test2 mismatched aggressors");
  bad=fixture(p);
  bad.victim.value->test1.equivalentPulseV[3]+=.01;
  rejects([&]{return calculateComAggressorReceiverVoltage(bad,p,live);},
          "tampered 012_21 receiver pulse");
  auto staleLive=live;
  ++staleLive.mappingGeneration;
  rejects([&]{return calculateComAggressorReceiverVoltage(fixture(p),p,staleLive);},
          "live SourceSnapshot freshness check");
  std::atomic_bool cancelled{true};
  bool wasCancelled=false;
  try { (void)calculateComAggressorReceiverVoltage(
          fixture(p),p,live,&cancelled);
  } catch (const si::Cancelled&) { wasCancelled=true; }
  need(wasCancelled,"cancellation must propagate");
  ComInput unused;
  (void)unused;
  std::cout<<"012_24 independent voltage multiplication and fail-closed PASS\n";
  return 0;
 } catch (const std::exception& e) {
  std::cerr<<"012_24 FAILURE: "<<e.what()<<"\n";
  return 1;
 }
}
