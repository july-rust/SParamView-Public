#include "si/channel_analysis/com_combined_tail.hpp"
#include "si/channel_analysis/com.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace si;
using namespace si::channel;

namespace {
void need(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
void near(double actual, double expected, double tol, const char* why) {
  need(std::isfinite(actual) && std::abs(actual-expected) <= tol, why);
}
template <class F> void rejects(F f, const char* why) {
  bool caught = false;
  try { (void)f(); } catch (const si::Error&) { caught = true; }
  need(caught, why);
}
Provenance source() {
  Provenance p;
  p.sourceSha256 = std::string(64, 'a');
  p.revision = "revision-25";
  p.mappingIdentity = "map-25";
  p.settingsIdentity = "120F8|tx=ffe25|rx=ctle25|";
  p.workspaceGeneration = 25;
  p.mappingGeneration = 8;
  return p;
}
SourceIdentity liveFrom(const Provenance& p) {
  return {p.sourceSha256,p.revision,p.mappingIdentity,
          p.workspaceGeneration,p.mappingGeneration};
}
Com120F8RectangularPulseData victimPulse(
    double cursor, double before, double after) {
  Com120F8RectangularPulseData x;
  x.symbolPeriodSeconds=1. / final8023ck120F8().symbolRateHz;
  for(int k=-3;k<=9;++k) {
    x.timeSeconds.push_back(5e-9+k*x.symbolPeriodSeconds);
    const double u=k==0 ? cursor : (k==-1 ? before : (k==1 ? after : 0.));
    x.unitSymbolPulse.push_back(u);
    x.maximumAbsolutePulse=std::max(x.maximumAbsolutePulse,std::abs(u));
  }
  return x;
}
si::channel::Result<ComVoltageScaleEvidence> scale(
    const Provenance& p, const char* which, double gain) {
  si::channel::Result<ComVoltageScaleEvidence> r;
  r.status=Status::Valid;
  r.provenance=p;
  r.provenance.algorithm="independent-synthetic-victim-scale";
  r.provenance.version="1";
  ComVoltageScaleEvidence e;
  e.testCase=which;
  e.voltsPerUnitSymbol=gain;
  e.transmitterReferencePlane="TX-Victim";
  e.receiverDecisionPlane="RX-Decision";
  e.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  e.calibrationIdentity="RX-Cal-25";
  e.calibrationSourceIdentity=std::string("Victim-Cal-")+which;
  e.txEqualizerIdentity="FFE-Victim";
  e.rxEqualizerIdentity="CTLE-RX";
  r.value=e;
  return r;
}
ComInterferencePulse pulse(const std::string& id, ComCoupling kind,
                           double before, double after) {
  ComInterferencePulse p;
  p.id=id;
  p.coupling=kind;
  p.unitSymbolPulse.assign(13,0.);
  p.unitSymbolPulse[2]=before;
  p.unitSymbolPulse[4]=after;
  p.maximumAbsolutePulse=std::max(std::abs(before),std::abs(after));
  return p;
}
si::channel::Result<ComAggressorVoltageEvidence> calibration(
    const Provenance& p, const char* which, const std::string& id,
    ComCoupling coupling, double gain) {
  si::channel::Result<ComAggressorVoltageEvidence> r;
  r.status=Status::Valid;
  r.provenance=p;
  r.provenance.settingsIdentity += "case=";
  r.provenance.settingsIdentity += which;
  r.provenance.algorithm="independent-synthetic-aggressor-cal";
  r.provenance.version="1";
  ComAggressorVoltageEvidence e;
  e.aggressorId=id;
  e.testCase=which;
  e.coupling=coupling;
  e.voltsPerUnitSymbol=gain;
  e.transmitterReferencePlane="TX-"+id;
  e.receiverDecisionPlane="RX-Decision";
  e.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  e.calibrationIdentity="RX-Cal-25";
  e.calibrationSourceIdentity="Cal-"+id+"-"+which;
  e.txEqualizerIdentity="EQ-"+id;
  e.rxEqualizerIdentity="CTLE-RX";
  r.value=e;
  return r;
}
si::channel::Result<ComJointVoltageEvidence> noiseComponent(
    const Provenance& p, const char* which, ComJointComponentKind kind,
    const char* id, double v) {
  si::channel::Result<ComJointVoltageEvidence> r;
  r.status=Status::Valid;
  r.provenance=p;
  r.provenance.settingsIdentity += "case=";
  r.provenance.settingsIdentity += which;
  r.provenance.algorithm="independent-synthetic-noise";
  r.provenance.version="1";
  ComJointVoltageEvidence e;
  e.amplitudeV=v;
  e.kind=kind;
  e.componentId=id;
  e.testCase=which;
  e.referencePlane="RX-Decision";
  e.calibrationIdentity="RX-Cal-25";
  e.assumedIndependent=true;
  e.includesReceiverGaussianNoise=
      kind == ComJointComponentKind::ReceiverGaussian;
  r.value=e;
  return r;
}
ComVictimIsiTailNoiseCase noise(const Provenance& p, const char* which) {
  ComVictimIsiTailNoiseCase x;
  x.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  x.calibrationSourceIdentity=std::string("Victim-Cal-")+which;
  x.txEqualizerIdentity="FFE-Victim";
  x.rxEqualizerIdentity="CTLE-RX";
  x.receiverGaussian=noiseComponent(
      p,which,ComJointComponentKind::ReceiverGaussian,"rx",.02);
  x.randomJitterGaussian=noiseComponent(
      p,which,ComJointComponentKind::RandomJitterGaussian,"rj",0.);
  x.dualDiracPeak=noiseComponent(
      p,which,ComJointComponentKind::DualDiracPeak,"dd",0.);
  return x;
}
ComCombinedTailInput fixture(
    const Provenance& p, size_t n=2, bool zeroVictim=false) {
  ComVoltageScalingInput original;
  original.pulses.status=Status::Warning;
  original.pulses.provenance=p;
  original.pulses.provenance.algorithm=
      "com-120f8-two-package-symbol-pulse-exploratory";
  original.pulses.provenance.version="012_09_03-pulse-1";
  original.pulses.value=Com120F8RectangularPulseCases{
      victimPulse(.8,zeroVictim?0.:-.1,zeroVictim?0.:.2),
      victimPulse(.5,zeroVictim?0.:.25,zeroVictim?0.:-.2)};
  original.test1Scale=scale(p,"TEST1",.2);
  original.test2Scale=scale(p,"TEST2",.4);
  ComCombinedTailInput in;
  in.voltage.victim=scaleCom120F8ReceiverEquivalentVoltage(original,p);
  in.voltage.interference.status=Status::Warning;
  in.voltage.interference.provenance=p;
  in.voltage.interference.provenance.algorithm=
      "com-120f8-separated-test1-test2-interference";
  in.voltage.interference.provenance.version="012_14-1";
  in.voltage.interference.diagnostics.push_back(
      {Severity::Warning,"NOT_IEEE_COM","synthetic"});
  ComInterferenceTestCases pairs;
  for (auto* t : {&pairs.test1,&pairs.test2}) {
    t->symbolPeriodSeconds=1. / final8023ck120F8().symbolRateHz;
    t->timeSeconds.assign(in.voltage.victim.value->test1.timeSeconds.begin(),
                          in.voltage.victim.value->test1.timeSeconds.end());
  }
  for(size_t i=0;i<n;++i) {
    const auto id="aggressor-"+std::to_string(i);
    const auto kind=i%2==0 ? ComCoupling::Next : ComCoupling::Fext;
    const double before=i==0 ? -.25 : (i==1 ? .3 : .001);
    const double after=i==0 ? .5 : (i==1 ? -.2 : -.002);
    pairs.test1.aggressors.push_back(pulse(id,kind,before,after));
    pairs.test2.aggressors.push_back(pulse(id,kind,-before,-after));
    const double gain1=i==0 ? .4 : (i==1 ? .6 : .1);
    const double gain2=i==0 ? .8 : (i==1 ? .3 : .2);
    in.voltage.test1.calibrations.push_back(
        calibration(p,"TEST1",id,kind,gain1));
    in.voltage.test2.calibrations.push_back(
        calibration(p,"TEST2",id,kind,gain2));
  }
  in.voltage.interference.value=std::move(pairs);
  in.test1=noise(p,"TEST1");
  in.test2=noise(p,"TEST2");
  in.oneSidedTailProbability=1e-5;
  in.independenceModel=ComCombinedIndependenceModel::IndependentUniformPam4;
  return in;
}
// Independent direct 4^n state enumeration and long-double bisection,
// never invokes the production 012_20 solver. Only small synthetic cases.
long double independentQuantile(
    const std::vector<double>& signedTaps, long double sigma,
    long double oneSidedP) {
  const long double levels[] = {-1.L,-1.L/3.L,1.L/3.L,1.L};
  std::vector<long double> states{0.L};
  for(double tap:signedTaps) {
    need(states.size()<=65536/4,"independent reference state budget");
    std::vector<long double> expanded;
    expanded.reserve(states.size()*4);
    for(auto old:states)
      for(auto level:levels) expanded.push_back(old+tap*level);
    states=std::move(expanded);
  }
  auto probability=[&](long double threshold) {
    long double sum=0;
    for(auto s:states)
      sum += .5L*std::erfc((threshold-s)/(sigma*std::sqrt(2.L)));
    return sum/static_cast<long double>(states.size());
  };
  long double low=0., high=1.;
  for(int i=0;i<68;++i) {
    const auto mid=(low+high)/2;
    if(probability(mid)>oneSidedP) low=mid;
    else high=mid;
  }
  return (low+high)/2;
}
bool sameTaps(const std::vector<ComCombinedTapRecord>& a,
              const std::vector<ComCombinedTapRecord>& b) {
  if(a.size()!=b.size()) return false;
  for(size_t i=0;i<a.size();++i)
    if(a[i].role!=b[i].role || a[i].aggressorId!=b[i].aggressorId ||
       a[i].uiOffset!=b[i].uiOffset ||
       std::abs(a[i].signedVoltageV-b[i].signedVoltageV)>1e-15) return false;
  return true;
}
void eraseNext(ComCombinedTailInput& in) {
  for(auto* t : {&in.voltage.interference.value->test1,
                 &in.voltage.interference.value->test2})
    t->aggressors.erase(t->aggressors.begin());
  in.voltage.test1.calibrations.erase(in.voltage.test1.calibrations.begin());
  in.voltage.test2.calibrations.erase(in.voltage.test2.calibrations.begin());
}
} // namespace

int main() {
 try {
  const auto p=source();
  const auto live=liveFrom(p);
  const auto empty=calculateComCombinedTailBridge(fixture(p,0,true),p,live);
  need(empty.usable() && empty.status==Status::Warning &&
       empty.provenance.version=="012_25-1" &&
       empty.diagnostics.size()==1 &&
       empty.diagnostics.front().code=="NOT_IEEE_COM" &&
       empty.value->test1.selectedTapCount==0 &&
       empty.value->test1.statesEvaluated==1,
       "Gaussian-only and NOT_IEEE_COM contract");
  near(empty.value->test1.selectedTailQuantileV,.0852978158784565,1e-11,
       "Gaussian-only published independent mathematical oracle");

  const auto victim=calculateComCombinedTailBridge(fixture(p,0),p,live);
  const auto& v=victim.value->test1;
  near(v.selectedTailQuantileV,.1323069531212122,1e-11,
       "independent 2-tap self-ISI oracle");
  need(v.selectedTapCount==2 && v.omittedTapCount==0 &&
       v.statesEvaluated==16 && v.selectedTaps[0].role==
       ComCombinedTapRole::VictimSelfIsi &&
       v.selectedTaps[0].uiOffset==1 && v.selectedTaps[1].uiOffset==-1 &&
       v.boundMode=="EXACT_ASSUMED_MIXTURE",
       "victim-only cursor exclusion and roles");

  const auto one=calculateComCombinedTailBridge(fixture(p,1),p,live);
  near(one.value->test1.selectedTailQuantileV,.416686608710545,1e-11,
       "independent victim+NEXT 4-tap oracle");
  need(one.value->test1.selectedTapCount==4 &&
       one.value->test1.statesEvaluated==256,
       "victim+NEXT PAM4 state count");
  bool nextSeen=false;
  for (const auto& t:one.value->test1.selectedTaps)
    if(t.role==ComCombinedTapRole::Next && t.aggressorId=="aggressor-0" &&
       t.uiOffset==1 && std::abs(t.signedVoltageV-.2)<1e-14)
      nextSeen=true;
  need(nextSeen,"signed NEXT voltage metadata");

  const auto two=calculateComCombinedTailBridge(fixture(p,2),p,live);
  near(two.value->test1.selectedTailQuantileV,.6967286340771246,1e-10,
       "independent 6-tap victim+NEXT+FEXT oracle");
  need(two.value->test1.selectedTapCount==6 &&
       two.value->test1.omittedTapCount==0 &&
       two.value->test1.statesEvaluated==4096 &&
       std::abs(two.value->test1.lowerTailBoundV-
                two.value->test1.upperTailBoundV)<=0.,
       "combined exact six-tap budget");
  need(two.value->test2.testCase=="TEST2" &&
       std::abs(two.value->test2.selectedTailQuantileV-
                two.value->test1.selectedTailQuantileV)>1e-9 &&
       std::abs(two.value->worstCaseExploratoryUpperTailV-
                std::max(two.value->test1.upperTailBoundV,
                         two.value->test2.upperTailBoundV))<1e-15,
       "two test cases and worst exploratory upper bound");
  bool hasFext=false;
  for(const auto& t:two.value->test1.selectedTaps)
    if(t.role==ComCombinedTapRole::Fext && t.aggressorId=="aggressor-1")
      hasFext=true;
  need(hasFext,"FEXT identity must survive combined tail");

  auto fextOnly=fixture(p,2,true);
  eraseNext(fextOnly);
  const auto fext=calculateComCombinedTailBridge(fextOnly,p,live);
  need(fext.value->test1.selectedTapCount==2 &&
       fext.value->test1.selectedTaps[0].role==ComCombinedTapRole::Fext,
       "standalone FEXT cases");

  auto atCursor=fixture(p,1,true);
  auto& next=atCursor.voltage.interference.value->test1.aggressors[0];
  next.unitSymbolPulse[2]=0.;
  next.unitSymbolPulse[4]=0.;
  next.unitSymbolPulse[3]=.5;
  next.maximumAbsolutePulse=.5;
  auto& next2=atCursor.voltage.interference.value->test2.aggressors[0];
  next2.unitSymbolPulse[2]=0.;
  next2.unitSymbolPulse[4]=0.;
  next2.unitSymbolPulse[3]=-.5;
  next2.maximumAbsolutePulse=.5;
  const auto cursor=calculateComCombinedTailBridge(atCursor,p,live);
  need(cursor.value->test1.selectedTapCount==1 &&
       cursor.value->test1.selectedTaps[0].role==ComCombinedTapRole::Next &&
       cursor.value->test1.selectedTaps[0].uiOffset==0,
       "aggressor UI zero must NOT be excluded as victim cursor");

  // Eight nonzero voltage taps: 6 selected; two omitted with an
  // independently calculated conservative voltage sum.
  const auto seven=calculateComCombinedTailBridge(fixture(p,3),p,live);
  const auto& b=seven.value->test1;
  need(b.selectedTapCount==6 && b.omittedTapCount==2 &&
       b.statesEvaluated==4096 &&
       b.boundMode=="CONSERVATIVE_BOUNDED_MIXTURE" &&
       b.selectedTaps.size()==6 && b.omittedTaps.size()==2,
       "bounded eight-tap selection, no silently dropped terms");
  near(b.omittedTapBoundV,.0003,1e-12,
       "independent omitted-absolute-voltage sum"); // .001*.1 + .002*.1
  std::vector<double> selectedValues, allValues;
  for(const auto& t:b.selectedTaps) {
    selectedValues.push_back(t.signedVoltageV);
    allValues.push_back(t.signedVoltageV);
  }
  for(const auto& t:b.omittedTaps)
    allValues.push_back(t.signedVoltageV);
  near(b.selectedTailQuantileV,
       static_cast<double>(independentQuantile(selectedValues,.02L,1e-5L)),
       1e-10,"independent exact selected enumeration");
  const double fullOracle=static_cast<double>(
      independentQuantile(allValues,.02L,1e-5L));
  need(fullOracle>=b.lowerTailBoundV-1e-10 &&
       fullOracle<=b.upperTailBoundV+1e-10,
       "independent 8-tap full-tail quantile within omitted bound");

  auto high32=calculateComCombinedTailBridge(fixture(p,32),p,live);
  need(high32.value->test1.selectedTapCount==6 &&
       high32.value->test1.omittedTapCount==60 &&
       high32.value->test1.statesEvaluated==4096 &&
       high32.value->test1.omittedTapBoundV>0.,
       "32 aggressors without exponential enumeration");
  auto reordered=fixture(p,3);
  std::reverse(reordered.voltage.interference.value->test1.aggressors.begin(),
               reordered.voltage.interference.value->test1.aggressors.end());
  std::reverse(reordered.voltage.interference.value->test2.aggressors.begin(),
               reordered.voltage.interference.value->test2.aggressors.end());
  std::reverse(reordered.voltage.test1.calibrations.begin(),
               reordered.voltage.test1.calibrations.end());
  std::reverse(reordered.voltage.test2.calibrations.begin(),
               reordered.voltage.test2.calibrations.end());
  const auto permuted=calculateComCombinedTailBridge(reordered,p,live);
  need(sameTaps(permuted.value->test1.selectedTaps,b.selectedTaps) &&
       sameTaps(permuted.value->test1.omittedTaps,b.omittedTaps) &&
       std::abs(permuted.value->test1.selectedTailQuantileV-
                b.selectedTailQuantileV)<1e-15,
       "deterministic input permutation invariance");

  auto dd=fixture(p,2);
  dd.test1.dualDiracPeak.value->amplitudeV=.015;
  const auto withDd=calculateComCombinedTailBridge(dd,p,live);
  need(withDd.value->test1.statesEvaluated==8192,
       "dual-Dirac maximum 8192 state cap");

  auto bad=fixture(p,1);
  bad.independenceModel=ComCombinedIndependenceModel::NotDeclared;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "missing independence assumption must reject");
  bad=fixture(p,1);
  bad.oneSidedTailProbability=0.;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "invalid probability must reject");
  bad=fixture(p,1);
  bad.test1.receiverGaussian.value->referencePlane="wrong";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "noise RX decision plane mismatch");
  bad=fixture(p,1);
  bad.test1.calibrationSourceIdentity="wrong";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "victim noise calibration source mismatch");
  bad=fixture(p,1);
  bad.test2.txEqualizerIdentity="wrong";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "victim TX EQ mismatch");
  bad=fixture(p,1);
  bad.test1.randomJitterGaussian.value->includesReceiverGaussianNoise=true;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "Gaussian/RJ noise double ownership");
  bad=fixture(p,1);
  bad.test1.dualDiracPeak.value->componentId="rx";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "duplicate noise ID");
  bad=fixture(p,1);
  bad.voltage.victim.value->test1.equivalentPulseV[2]=.123;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "tampered original scaled victim");
  bad=fixture(p,1);
  bad.voltage.interference.value->test1.aggressors[0].maximumAbsolutePulse=.01;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "tampered input aggressor maximum");
  bad=fixture(p,1);
  bad.voltage.test1.calibrations[0].value->voltsPerUnitSymbol=
      std::numeric_limits<double>::quiet_NaN();
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "NaN aggressor volts-per-symbol");
  bad=fixture(p,1);
  bad.voltage.test1.calibrations[0].value->calibrationSourceIdentity="";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "missing aggressor TX calibration source");
  bad=fixture(p,1);
  bad.voltage.interference.value->test1.timeSeconds[3]+=.1;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "misaligned aggressor time grid");
  bad=fixture(p,1);
  bad.voltage.interference.provenance.sourceSha256[0]='b';
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "interference SHA mismatch");
  bad=fixture(p,1);
  bad.voltage.victim.provenance.mappingGeneration++;
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "stale mapping generation");
  bad=fixture(p,1);
  bad.test1.randomJitterGaussian.provenance.settingsIdentity += "case=TEST1";
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "duplicate case suffix in noise provenance");
  bad=fixture(p,33);
  rejects([&]{return calculateComCombinedTailBridge(bad,p,live);},
          "33 aggressors over budget");
  auto stale=live;
  ++stale.workspaceGeneration;
  rejects([&]{return calculateComCombinedTailBridge(fixture(p,1),p,stale);},
          "live snapshot changed");
  std::atomic_bool cancelled{true};
  bool didCancel=false;
  try { (void)calculateComCombinedTailBridge(fixture(p,1),p,live,&cancelled); }
  catch(const si::Cancelled&) { didCancel=true; }
  need(didCancel,"cancellation propagation");

  ComSettings settings;
  settings.profileId="IEEE-802.3ck-2022-100GAUI-1-C2C-120F-REFERENCE-CANDIDATE";
  const auto official=calculateCom(ComInput{},settings,p);
  need(official.status==Status::NotEvaluated && !official.value,
       "production COM remains NotEvaluated");
  std::cout<<"012_25 combined victim/xtalk bound independent goldens PASS\n";
  return 0;
 } catch (const std::exception& e) {
  std::cerr<<"012_25 combined tail FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
