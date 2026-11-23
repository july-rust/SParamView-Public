#include "si/channel_analysis/com_cursor_tail_ratio.hpp"
#include "si/channel_analysis/com_voltage_ratio.hpp"
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

} // namespace

int main() {
  try {
    const auto p=source();
    const auto live=liveFrom(p);
    const auto baseline=calculateComCursorTailRatioBridge(fixture(p,0,true),p,live);
    need(baseline.usable() && baseline.status==Status::Warning &&
         baseline.provenance.version=="012_29-1" &&
         baseline.provenance.algorithm=="com-cursor-tail-ratio-exploratory" &&
         baseline.diagnostics.size()==1 &&
         baseline.diagnostics.front().code=="NOT_IEEE_COM" &&
         baseline.value->model=="EXPLORATORY_CURSOR_TAIL_RATIO_NOT_IEEE_COM",
         "012_28: fail-closed exploratory output/provenance");
    const double qRef=.08529781587845649257; // independent 80-dps normal-tail oracle
    const auto& a=baseline.value->test1;
    const auto& b=baseline.value->test2;
    near(a.cursorSignedV,.16,1e-15,"TEST1 signed cursor");
    near(b.cursorSignedV,.20,1e-15,"TEST2 signed cursor");
    near(a.nominalPam4HalfSpacingV,.16/3.,1e-15,"TEST1 half spacing");
    near(b.nominalPam4HalfSpacingV,.20/3.,1e-15,"TEST2 half spacing");
    near(a.noiseUpperTailV,qRef,1e-6,"independent Gaussian tail upper bound");
    near(b.noiseUpperTailV,qRef,1e-6,"TEST2 independent Gaussian tail upper");
    near(a.exploratoryLowerRatioDb,
         20.*std::log10((.16/3.)/qRef),1e-4,"TEST1 independent dB oracle");
    near(b.exploratoryLowerRatioDb,
         20.*std::log10((.20/3.)/qRef),1e-4,"TEST2 independent dB oracle");
    near(baseline.value->minimumExploratoryRatioDb,
         a.exploratoryLowerRatioDb,1e-12,"minimum two-case ratio");
    need(a.numericalQualification=="NUMERICALLY_BRACKETED_NOT_CERTIFIED" &&
         a.boundMode=="NUMERICALLY_BRACKETED_ASSUMED_MIXTURE" &&
         a.oneSidedTailProbability==1e-5,
         "012_28: probability/qualification metadata");
    // Independent Gaussian oracle checks both images of the finite-precision
    // noise interval, without borrowing the engine's estimated quantile.
    const double ideal1 = 20.*std::log10((.16/3.)/qRef);
    const double ideal2 = 20.*std::log10((.20/3.)/qRef);
    need(a.noiseLowerTailBoundV > 0. &&
         b.noiseLowerTailBoundV > 0. &&
         a.noiseLowerTailBoundV <= a.noiseUpperTailV &&
         b.noiseLowerTailBoundV <= b.noiseUpperTailV &&
         a.exploratoryUpperRatioDb.has_value() &&
         b.exploratoryUpperRatioDb.has_value() &&
         a.exploratoryLowerRatioDb <= *a.exploratoryUpperRatioDb &&
         b.exploratoryLowerRatioDb <= *b.exploratoryUpperRatioDb,
         "012_29: missing or inverted Gaussian diagnostic interval");
    near(*a.exploratoryUpperRatioDb,ideal1,1e-4,
         "012_29: TEST1 independent upper dB golden");
    near(*b.exploratoryUpperRatioDb,ideal2,1e-4,
         "012_29: TEST2 independent upper dB golden");
    need(baseline.value->minimumExploratoryRatioUpperDb.has_value(),
         "012_29: Gaussian pair upper must exist");
    near(*baseline.value->minimumExploratoryRatioUpperDb,
         std::min(*a.exploratoryUpperRatioDb,
                  *b.exploratoryUpperRatioDb),1e-12,
         "012_29: exact two-case minimum of finite upper endpoints");

    // Adding victim self-ISI and NEXT/FEXT cannot increase the diagnostic
    // ratio in these independently fixed synthetic examples.
    const auto victim=calculateComCursorTailRatioBridge(fixture(p,0),p,live);
    const auto next=calculateComCursorTailRatioBridge(fixture(p,1),p,live);
    const auto fext=calculateComCursorTailRatioBridge(fixture(p,2),p,live);
    const auto omitted=calculateComCursorTailRatioBridge(fixture(p,3),p,live);
    need(victim.value->test1.noiseUpperTailV > a.noiseUpperTailV &&
         next.value->test1.noiseUpperTailV >
             victim.value->test1.noiseUpperTailV &&
         fext.value->test1.noiseUpperTailV >
             next.value->test1.noiseUpperTailV &&
         omitted.value->test1.noiseUpperTailV >=
             fext.value->test1.noiseUpperTailV,
         "012_28: expected synthetic noise ordering");
    need(victim.value->test1.exploratoryLowerRatioDb <
             a.exploratoryLowerRatioDb &&
         next.value->test1.exploratoryLowerRatioDb <
             victim.value->test1.exploratoryLowerRatioDb &&
         omitted.value->test1.exploratoryLowerRatioDb <=
             fext.value->test1.exploratoryLowerRatioDb &&
         omitted.value->test1.boundMode=="CONSERVATIVE_NUMERICAL_BOUND" &&
         omitted.value->test1.noiseUpperTailV >
             fext.value->test1.noiseUpperTailV,
         "012_28: omitted tap not silently discarded");
    need(std::abs(victim.value->test1.exploratoryLowerRatioDb -
                  victim.value->test2.exploratoryLowerRatioDb)>1e-6 &&
         victim.value->test1.testCase=="TEST1" &&
         victim.value->test2.testCase=="TEST2",
         "012_28: Test cases must be isolated");

    // Independent common-voltage scaling retains a dimensionless ratio.
    auto scaled=fixture(p,0,true);
    for(auto* v : {&scaled.voltage.victim.value->test1,
                   &scaled.voltage.victim.value->test2}) {
      v->voltsPerUnitSymbol *= 2.;
      for(auto& sample:v->equivalentPulseV) sample*=2.;
      v->cursorEquivalentV*=2.;
      v->maximumAbsoluteEquivalentV*=2.;
    }
    scaled.test1.receiverGaussian.value->amplitudeV*=2.;
    scaled.test2.receiverGaussian.value->amplitudeV*=2.;
    const auto doubled=calculateComCursorTailRatioBridge(scaled,p,live);
    near(doubled.value->test1.exploratoryLowerRatioDb,
         a.exploratoryLowerRatioDb,1e-8,"TEST1 common scaling invariance");
    near(doubled.value->test2.exploratoryLowerRatioDb,
         b.exploratoryLowerRatioDb,1e-8,"TEST2 common scaling invariance");
    need(doubled.value->test1.exploratoryUpperRatioDb &&
         doubled.value->test2.exploratoryUpperRatioDb,
         "012_29: voltage scaling lost upper bound");
    near(*doubled.value->test1.exploratoryUpperRatioDb,
         *a.exploratoryUpperRatioDb,1e-8,
         "012_29: TEST1 upper common scaling invariance");
    near(*doubled.value->test2.exploratoryUpperRatioDb,
         *b.exploratoryUpperRatioDb,1e-8,
         "012_29: TEST2 upper common scaling invariance");

    // A validated signed negative cursor must preserve both absolute
    // half-spacing and voltage-ratio endpoints.
    auto signedCursor=fixture(p,0,true);
    auto& signedTest1=signedCursor.voltage.victim.value->test1;
    signedTest1.unitSymbolPulse[3]=-signedTest1.unitSymbolPulse[3];
    signedTest1.equivalentPulseV[3]=-signedTest1.equivalentPulseV[3];
    signedTest1.cursorEquivalentV=signedTest1.equivalentPulseV[3];
    const auto negative=calculateComCursorTailRatioBridge(signedCursor,p,live);
    need(negative.value->test1.cursorSignedV < 0. &&
         negative.value->test1.exploratoryUpperRatioDb.has_value(),
         "012_29: negative signed cursor not accepted");
    near(negative.value->test1.exploratoryLowerRatioDb,
         a.exploratoryLowerRatioDb,1e-12,
         "012_29: negative cursor lower dB invariance");
    near(*negative.value->test1.exploratoryUpperRatioDb,
         *a.exploratoryUpperRatioDb,1e-12,
         "012_29: negative cursor upper dB invariance");

    // 20 independently sourced synthetic aggressors (within the 32 limit)
    // create an omitted-tap envelope larger than one or both lower quantiles.
    // Missing upper ends must be absent, not NaN, infinity, zero or DBL_MAX.
    auto oneSided=fixture(p,20);
    for(size_t i=2;i<oneSided.voltage.test1.calibrations.size();++i)
      oneSided.voltage.test1.calibrations[i].value->voltsPerUnitSymbol=40.;
    const auto one=calculateComCursorTailRatioBridge(oneSided,p,live);
    need(one.value->test1.boundMode=="CONSERVATIVE_NUMERICAL_BOUND" &&
         one.value->test1.noiseLowerTailBoundV <= 0. &&
         !one.value->test1.exploratoryUpperRatioDb.has_value() &&
         std::isfinite(one.value->test1.exploratoryLowerRatioDb) &&
         one.value->test2.noiseLowerTailBoundV > 0. &&
         one.value->test2.exploratoryUpperRatioDb.has_value() &&
         one.value->minimumExploratoryRatioUpperDb.has_value(),
         "012_29: one-sided TEST1 and finite TEST2 upper contract");
    near(*one.value->minimumExploratoryRatioUpperDb,
         *one.value->test2.exploratoryUpperRatioDb,1e-12,
         "012_29: one known Test upper bounds the minimum");
    auto bothOneSided=oneSided;
    for(size_t i=2;i<bothOneSided.voltage.test2.calibrations.size();++i)
      bothOneSided.voltage.test2.calibrations[i].value->voltsPerUnitSymbol=40.;
    const auto both=calculateComCursorTailRatioBridge(bothOneSided,p,live);
    need(both.value->test1.noiseLowerTailBoundV <= 0. &&
         both.value->test2.noiseLowerTailBoundV <= 0. &&
         !both.value->test1.exploratoryUpperRatioDb &&
         !both.value->test2.exploratoryUpperRatioDb &&
         !both.value->minimumExploratoryRatioUpperDb &&
         std::isfinite(both.value->minimumExploratoryRatioDb),
         "012_29: both nonpositive noise lower bounds must remain one-sided");

    // The existing pure 012_19 dB kernel must survive extreme positive
    // representable voltages and near-unity ratios without arithmetic
    // overflow or premature division underflow.
    const double small=std::numeric_limits<double>::min();
    const double huge=std::numeric_limits<double>::max();
    const long double independentExtreme=
        (20.L/std::log(10.L))*
        (std::log(static_cast<long double>(small))-
         std::log(static_cast<long double>(huge)));
    near(calculateDiagnosticVoltageRatioDb(small,huge),
         static_cast<double>(independentExtreme),1e-7,
         "012_29: extreme safe-log voltage ratio");
    const double next=std::nextafter(1.,2.);
    near(calculateDiagnosticVoltageRatioDb(next,1.),
         20.*std::log10(next),1e-12,
         "012_29: near-unity stable voltage ratio");

    // Deliberately bad inputs must fail during upstream replay, not yield
    // attractive but unqualified dB values.
    auto stale=live; stale.workspaceGeneration++;
    rejects([&] {return calculateComCursorTailRatioBridge(
                    fixture(p,0,true),p,stale);},"stale live SourceIdentity");
    auto badNoise=fixture(p,0,true);
    badNoise.test1.receiverGaussian.value->amplitudeV =
        std::numeric_limits<double>::quiet_NaN();
    rejects([&] {return calculateComCursorTailRatioBridge(
                    badNoise,p,live);},"nonfinite Gaussian evidence");
    auto mixed=fixture(p,0,true);
    mixed.voltage.victim.value->test1.testCase="TEST2";
    rejects([&] {return calculateComCursorTailRatioBridge(
                    mixed,p,live);},"mixed victim Test cases");
    auto plane=fixture(p,0,true);
    plane.voltage.victim.value->test2.receiverDecisionPlane="RX-OTHER";
    rejects([&] {return calculateComCursorTailRatioBridge(
                    plane,p,live);},"different receiver planes");
    auto noCursor=fixture(p,0,true);
    noCursor.voltage.victim.value->test1.cursorEquivalentV=0.;
    rejects([&] {return calculateComCursorTailRatioBridge(
                    noCursor,p,live);},"invalid signal cursor");
    auto calibration=fixture(p,0,true);
    calibration.test1.calibrationSourceIdentity="STALE-CALIBRATION";
    rejects([&] {return calculateComCursorTailRatioBridge(
                    calibration,p,live);},"calibration lineage mismatch");
    auto unknownIndependence=fixture(p,0,true);
    unknownIndependence.independenceModel=
        ComCombinedIndependenceModel::NotDeclared;
    rejects([&] {return calculateComCursorTailRatioBridge(
                    unknownIndependence,p,live);},"undeclared independence");
    auto wrongP=fixture(p,0,true);
    wrongP.oneSidedTailProbability=0.;
    rejects([&] {return calculateComCursorTailRatioBridge(
                    wrongP,p,live);},"unsupported probability");
    std::atomic_bool cancelled{true};
    try {
      (void)calculateComCursorTailRatioBridge(fixture(p,0,true),p,live,&cancelled);
      need(false,"cancellation not honored");
    } catch(const si::Cancelled&) {}

    // The real IEEE COM evaluator stays fail closed.
    ComSettings settings;
    settings.profileId=
       "IEEE-802.3ck-2022-100GAUI-1-C2C-120F-REFERENCE-CANDIDATE";
    const auto production=calculateCom(ComInput{},settings,p);
    need(production.status==Status::NotEvaluated &&
         !production.value.has_value(),
         "012_28 cannot activate official calculateCom");
    std::cout<<"012_29 exploratory cursor/tail ratio interval PASS\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"012_28 FAILED: "<<e.what()<<'\n';
    return 1;
  }
}
