#include "si/channel_analysis/com_joint_tail.hpp"
#include "si/channel_analysis/com_statistics.hpp"
#include "si/channel_analysis/com.hpp"
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
void require(bool condition, const char* why) {
  if (!condition) throw std::runtime_error(why);
}
void near(double x, double y, double tolerance, const char* why) {
  require(std::isfinite(x) && std::abs(x-y)<=tolerance, why);
}
template<class Fn> void rejects(Fn fn, const char* why) {
  bool hit=false;
  try { (void)fn(); } catch(const Error&) { hit=true; }
  require(hit,why);
}
Provenance identity(const std::string& test="TEST1") {
  Provenance p;
  p.sourceSha256=std::string(64,'a');
  p.revision="rev-20";
  p.mappingIdentity="confirmed-physical-mapping";
  p.settingsIdentity="120F8|case="+test+"|settings=020|";
  p.workspaceGeneration=12;
  p.mappingGeneration=20;
  return p;
}
si::channel::Result<ComJointVoltageEvidence> component(double v,
    ComJointComponentKind kind, const std::string& id,
    const Provenance& p, const std::string& test) {
  si::channel::Result<ComJointVoltageEvidence> r;
  r.status=Status::Valid;
  r.value=ComJointVoltageEvidence{v,kind,id,test,
      "rx-decision-plane","voltage-calibration-020",
      true,kind==ComJointComponentKind::ReceiverGaussian};
  r.provenance=p;
  r.provenance.algorithm="independent-calibrated-"+id;
  r.provenance.version="1";
  return r;
}
ComJointTailInput inputs(double rx, double rj, double dd,
                        std::vector<double> taps, double p,
                        const Provenance& source,
                        const std::string& test) {
  ComJointTailInput out;
  out.receiverGaussian=component(rx,ComJointComponentKind::ReceiverGaussian,
                                 "rx",source,test);
  out.randomJitterGaussian=component(rj,
      ComJointComponentKind::RandomJitterGaussian,"rj",source,test);
  out.dualDiracPeak=component(dd,ComJointComponentKind::DualDiracPeak,
                             "dd",source,test);
  for(size_t k=0;k<taps.size();++k)
    out.pam4Taps.push_back(component(taps[k],
      ComJointComponentKind::IndependentPam4Tap,
      "pam4-"+std::to_string(k),source,test));
  out.oneSidedTailProbability=p;
  return out;
}
}
int main() {
  try {
    const auto p1=identity(), p2=identity("TEST2");
    const auto pure=calculateComJointTail(
        inputs(.02,0,0,{},1e-5,p1,"TEST1"),"TEST1",p1);
    require(pure.usable() && pure.status==Status::Warning &&
      pure.value->model=="EXPLORATORY_INDEPENDENT_PAM4_DUAL_DIRAC_GAUSSIAN" &&
      pure.diagnostics.size()==1 &&
      pure.diagnostics[0].code=="NOT_IEEE_COM", "non-normative contract");
    near(pure.value->oneSidedNoiseQuantileV,.0852978158784565,1e-12,
        "independent analytic standard Gaussian quantile");
    require(pure.value->statesEvaluated==1,"Gaussian one state");
    near(pure.value->gaussianVarianceV2,.0004,1e-16,"Gaussian variance");

    // Source-free independent Python math.erfc + bracketed bisection goldens:
    // P(X>q)=mean_{state} 0.5*erfc((q-state)/(sigma*sqrt(2))).
    const auto one=calculateComJointTail(
        inputs(.02,0,0,{.03},1e-5,p1,"TEST1"),"TEST1",p1);
    near(one.value->oneSidedNoiseQuantileV,.10893359953178733,1e-12,
         "independent four-state golden");
    require(one.value->statesEvaluated==4,"PAM4 must have four states");
    near(one.value->totalVarianceV2,.0004+(.03*.03)*5./9.,1e-15,
         "PAM4 level variance");

    const auto joint=calculateComJointTail(
        inputs(.012,.016,.015,{.03},1e-5,p1,"TEST1"),"TEST1",p1);
    near(joint.value->oneSidedNoiseQuantileV,.12056005802997405,1e-12,
         "independent eight-state Gaussian RJ/receiver + DD/PAM4 golden");
    require(joint.value->statesEvaluated==8,"eight mixture states");
    near(joint.value->gaussianVarianceV2,.0004,1e-16,"independent Gaussian sum");
    near(joint.value->totalVarianceV2,
         .0004+(.03*.03)*5./9.+.015*.015,1e-15,
         "mixture analytical variance");
    near(joint.value->verifiedTailProbability,1e-5,1e-11,
         "verified mixture tail probability");
    const auto harder=calculateComJointTail(
        inputs(.012,.016,.015,{.03,.01},1e-8,p1,"TEST1"),"TEST1",p1);
    near(harder.value->oneSidedNoiseQuantileV,.15527837162253227,1e-11,
         "independent 32-state 1e-8 tail golden");
    require(harder.value->statesEvaluated==32,"32 mixture states");

    const auto old=calculatePam4StatisticalNoise(
        ComStatisticalNoiseInput{.02,1e-5,{.03}});
    near(one.value->oneSidedNoiseQuantileV,
         old.value->oneSidedNoiseQuantileV,1e-12,
         "zero-Dirac legacy PAM4/Gaussian correlation");
    auto scaled=inputs(.012e-10,.016e-10,.015e-10,
                       {.03e-10},1e-5,p1,"TEST1");
    const auto nano=calculateComJointTail(scaled,"TEST1",p1);
    near(nano.value->oneSidedNoiseQuantileV/1e-10,
         joint.value->oneSidedNoiseQuantileV,1e-12,
         "small-voltage scale invariance");
    auto signedTap=inputs(.012,.016,.015,{-.03},1e-5,p1,"TEST1");
    near(calculateComJointTail(signedTap,"TEST1",p1)
             .value->oneSidedNoiseQuantileV,
         joint.value->oneSidedNoiseQuantileV,1e-12,
         "PAM4 sign symmetry");

    const auto t1=inputs(.012,.016,.015,{.03},1e-5,p1,"TEST1");
    const auto t2=inputs(.02,0,0,{},1e-5,p2,"TEST2");
    const auto pair=calculateComJointTailCases(t1,t2,p1,p2);
    require(pair.usable() && pair.status==Status::Warning &&
            pair.diagnostics[0].code=="NOT_IEEE_COM",
            "dual case must remain non-normative");
    near(pair.value->test1.oneSidedNoiseQuantileV,
         joint.value->oneSidedNoiseQuantileV,1e-12,"Test1 preserved");
    near(pair.value->test2.oneSidedNoiseQuantileV,
         pure.value->oneSidedNoiseQuantileV,1e-12,"Test2 preserved");
    near(pair.value->worstCaseNoiseQuantileV,
         joint.value->oneSidedNoiseQuantileV,1e-12,
         "worst exploratory noise max, NOT COM margin");
    auto differentTail=t2;
    differentTail.oneSidedTailProbability=1e-8;
    rejects([&]{return calculateComJointTailCases(t1,differentTail,p1,p2);},
            "different tail targets cannot be compared");
    auto differentCalibration=t2;
    differentCalibration.receiverGaussian.value->calibrationIdentity="other-cal";
    differentCalibration.randomJitterGaussian.value->calibrationIdentity="other-cal";
    differentCalibration.dualDiracPeak.value->calibrationIdentity="other-cal";
    rejects([&]{return calculateComJointTailCases(t1,differentCalibration,p1,p2);},
            "different case calibrations cannot be compared");
    auto full=inputs(.02,0,.001,
        {0.001,0.002,0.003,0.004,0.005,0.006},1e-5,p1,"TEST1");
    require(calculateComJointTail(full,"TEST1",p1).value->statesEvaluated==8192,
            "bounded 8192-state maximum");
    full.pam4Taps.push_back(component(.001,
        ComJointComponentKind::IndependentPam4Tap,"pam4-6",p1,"TEST1"));
    rejects([&]{return calculateComJointTail(full,"TEST1",p1);},
            "seven PAM4 taps must reject");

    auto bad=t1; bad.randomJitterGaussian.value->includesReceiverGaussianNoise=true;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "RJ containing receiver noise would double-count");
    bad=t1; bad.randomJitterGaussian.value->assumedIndependent=false;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "unqualified independence");
    bad=t1; bad.dualDiracPeak.value->calibrationIdentity="different";
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "calibration mismatch");
    bad=t1; bad.pam4Taps[0].value->referencePlane="different";
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "receiver plane mismatch");
    bad=t1; bad.pam4Taps[0].provenance.sourceSha256[0]='b';
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "source SHA mismatch");
    bad=t1; ++bad.receiverGaussian.provenance.mappingGeneration;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "mapping generation mismatch");
    bad=t1; bad.randomJitterGaussian.provenance.version.clear();
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "missing upstream algorithm version");
    bad=t1; bad.dualDiracPeak.status=Status::Stale;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "Stale must fail");
    bad=t1; bad.randomJitterGaussian.value.reset();
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "missing component value");
    bad=t1; bad.pam4Taps[0].value->componentId="rx";
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "duplicate physical component id");
    bad=t1; bad.randomJitterGaussian.value->amplitudeV=-.001;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "negative jitter RMS");
    bad=t1; bad.dualDiracPeak.value->amplitudeV=INFINITY;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "nonfinite Dual-Dirac");
    bad=t1; bad.oneSidedTailProbability=1e-15;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "unsupported tail probability");
    bad=t1; bad.receiverGaussian.value->amplitudeV=0;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "zero Gaussian receiver RMS");
    bad=t1; bad.pam4Taps[0].value->amplitudeV=1e308;
    rejects([&]{return calculateComJointTail(bad,"TEST1",p1);},
            "overwide dynamic range");
    bad=t2;
    bad.receiverGaussian.value->testCase="TEST1";
    rejects([&]{return calculateComJointTailCases(t1,bad,p1,p2);},
            "mixed case identity");
    auto malformed=p1; malformed.settingsIdentity="120F8|case=TEST1|case=TEST1|";
    rejects([&]{return calculateComJointTail(
        inputs(.02,0,0,{},1e-5,malformed,"TEST1"),"TEST1",malformed);},
        "adjacent duplicate case fields");
    malformed=p2; ++malformed.workspaceGeneration;
    rejects([&]{return calculateComJointTailCases(t1,t2,p1,malformed);},
            "cross-case source generations");
    std::atomic_bool cancelled{true};
    bool didCancel=false;
    try {(void)calculateComJointTail(t1,"TEST1",p1,&cancelled);}
    catch(const Cancelled&) {didCancel=true;}
    require(didCancel,"cancellation");
    const auto production=calculateCom(ComInput{},ComSettings{});
    require(production.status==Status::NotEvaluated &&
            !production.value.has_value(),"production COM must remain disabled");

    std::cout<<"012_20 independent joint statistical goldens PASS; "
                "NOT IEEE COM/PHY\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr<<"012_20 failure: "<<e.what()<<'\n';
    return 1;
  }
}
