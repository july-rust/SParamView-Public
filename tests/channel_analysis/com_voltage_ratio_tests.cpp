#include "si/channel_analysis/com_voltage_ratio.hpp"
#include "si/channel_analysis/com.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok, const char* m) { if (!ok) throw std::runtime_error(m); }
void close(double a, double b, double tol, const char* m) {
  demand(std::isfinite(a) && std::abs(a-b) <= tol, m);
}
template<class F> void rejects(F f, const char* why) {
  bool failed = false;
  try { f(); } catch (const Error&) { failed = true; }
  demand(failed, why);
}
Provenance identity(const std::string& c="TEST1") {
  Provenance p;
  p.sourceSha256 = std::string(64, 'a');
  p.revision = "rev-19";
  p.mappingIdentity = "physical-mapping";
  p.settingsIdentity = "120F8|case="+c+"|settings=019|";
  p.workspaceGeneration=12;
  p.mappingGeneration=19;
  return p;
}
Result<ComVoltageEvidence> evidence(double v, const Provenance& p,
                                    const std::string& c,
                                    const std::string& alg) {
  Result<ComVoltageEvidence> r;
  r.status=Status::Valid;
  r.value=ComVoltageEvidence{v,c,"receiver-decision","voltage-calibration"};
  r.provenance=p;
  r.provenance.algorithm=alg;
  r.provenance.version="1";
  return r;
}
ComVoltageRatioInput inputs(double s,double n,
                            const Provenance& p,const std::string& c) {
  return {evidence(s,p,c,"golden-signal"),
          evidence(n,p,c,"golden-noise")};
}
}
int main() {
  try {
    const auto p1=identity(),p2=identity("TEST2");
    auto t1=inputs(0.4,0.2,p1,"TEST1");
    const double oracle=6.020599913279624;
    auto r=calculateComDiagnosticVoltageRatio(t1,"TEST1",p1);
    demand(r.usable() && r.status==Status::Warning &&
           r.diagnostics.size()==1 &&
           r.diagnostics[0].code=="NOT_IEEE_COM" &&
           r.value->model=="EXPLORATORY_VOLTAGE_RATIO_NOT_IEEE_COM",
           "Ratio must never be misrepresented as official IEEE COM");
    close(r.value->ratioDb,oracle,1e-12,"independent 2:1 golden");
    close(calculateComDiagnosticVoltageRatio(
        inputs(1.,1.,p1,"TEST1"),"TEST1",p1).value->ratioDb,
        0.,1e-12,"1:1 golden");
    close(calculateComDiagnosticVoltageRatio(
        inputs(.1,.2,p1,"TEST1"),"TEST1",p1).value->ratioDb,
        -oracle,1e-12,"1:2 golden");
    for(double scale:{1e-100,1e100})
      close(calculateComDiagnosticVoltageRatio(
          inputs(.4*scale,.2*scale,p1,"TEST1"),"TEST1",p1)
          .value->ratioDb,oracle,1e-12,"voltage scale invariance");
    auto extreme=calculateComDiagnosticVoltageRatio(
        inputs(std::numeric_limits<double>::max(),
               std::numeric_limits<double>::denorm_min(),p1,"TEST1"),
        "TEST1",p1);
    demand(std::isfinite(extreme.value->ratioDb) &&
           extreme.value->ratioDb > 12000.,
           "valid extreme ratio must remain representable in dB");
    auto t2=inputs(.2,.2,p2,"TEST2");
    auto pair=calculateComDiagnosticVoltageRatioCases(t1,t2,p1,p2);
    demand(pair.usable() && pair.status==Status::Warning &&
           pair.diagnostics[0].code=="NOT_IEEE_COM", "pair warning");
    close(pair.value->test1.ratioDb,oracle,1e-12,"Test1 case");
    close(pair.value->test2.ratioDb,0.,1e-12,"Test2 case");
    close(pair.value->minimumRatioDb,0.,1e-12,"minimum of cases");
    t2.signal.status=Status::Stale;
    rejects([&]{(void)calculateComDiagnosticVoltageRatioCases(
        t1,t2,p1,p2);},"a bad case must reject the pair");

    for(double bad:{0.,-0.3,std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
      rejects([&]{(void)calculateComDiagnosticVoltageRatio(
          inputs(bad,.2,p1,"TEST1"),"TEST1",p1);},"bad signal");
      rejects([&]{(void)calculateComDiagnosticVoltageRatio(
          inputs(.4,bad,p1,"TEST1"),"TEST1",p1);},"bad noise");
    }
    auto corrupted=t1;
    corrupted.signal.provenance.sourceSha256[0]='b';
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"source sha");
    corrupted=t1;
    ++corrupted.noiseAtTargetDer.provenance.mappingGeneration;
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"mapping generation");
    corrupted=t1;
    corrupted.signal.provenance.version.clear();
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"algorithm version");
    corrupted=t1;
    corrupted.signal.provenance.settingsIdentity=p2.settingsIdentity;
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"settings cross-case");
    corrupted=t1;
    corrupted.signal.value->referencePlane="other-plane";
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"plane mismatch");
    corrupted=t1;
    corrupted.noiseAtTargetDer.value->calibrationIdentity="other";
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"calibration mismatch");
    corrupted=t1;
    corrupted.signal.status=Status::NotEvaluated;
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"upstream status");
    corrupted=t1;
    corrupted.signal.value.reset();
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        corrupted,"TEST1",p1);},"missing input value");
    auto malformed=p1;
    malformed.settingsIdentity+="|case=TEST1|";
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        t1,"TEST1",malformed);},"duplicate case");
    malformed=p1;
    malformed.sourceSha256="invalid";
    rejects([&]{(void)calculateComDiagnosticVoltageRatio(
        t1,"TEST1",malformed);},"invalid sha");
    malformed=p2;
    ++malformed.workspaceGeneration;
    rejects([&]{(void)calculateComDiagnosticVoltageRatioCases(
        t1,inputs(.2,.2,p2,"TEST2"),p1,malformed);},
        "mixed source generations");
    std::atomic_bool cancelled{true};
    bool didCancel=false;
    try {(void)calculateComDiagnosticVoltageRatio(t1,"TEST1",p1,&cancelled);}
    catch(const Cancelled&) {didCancel=true;}
    demand(didCancel,"cancellation");
    const auto production=calculateCom(ComInput{},ComSettings{});
    demand(production.status==Status::NotEvaluated &&
           !production.value.has_value(),"normative gate must remain closed");
    std::cout<<"012_19 voltage ratio / identity / independent goldens: PASS\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"012_19 failure: "<<e.what()<<'\n';
    return 1;
  }
}
