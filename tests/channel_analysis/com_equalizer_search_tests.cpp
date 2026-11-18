#include "si/channel_analysis/com_equalizer_search.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <set>
#include <string>

using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void close(double actual, double expected, double tolerance,
           const char* message) {
  demand(std::isfinite(actual) &&
         std::abs(actual-expected) <= tolerance, message);
}
template<class F> void reject(F&& call, const char* message) {
  bool failed = false;
  try { call(); }
  catch (const si::Error&) { failed = true; }
  demand(failed,message);
}
Provenance source(const std::string& which = "TEST1") {
  Provenance p;
  p.sourceSha256=std::string(64,'c');
  p.revision="synthetic-012-18";
  p.mappingIdentity="confirmed-s4p-testcase";
  p.settingsIdentity="IEEE-120F8|case="+which+"|rx-profile|";
  p.workspaceGeneration=21;
  p.mappingGeneration=34;
  return p;
}
Result<ComPhysicalChannelData> physical() {
  const auto profile=final8023ck120F8();
  ComPhysicalChannelData p;
  p.model="EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P";
  p.referenceOhmDifferential=100.;
  p.maxObservedModeConversion=0.;
  p.test1.packageCase=ComPackageCase::Test1;
  p.test2.packageCase=ComPackageCase::Test2;
  p.test1.txPrimaryLineMm=profile.txPackageLengthMm[0];
  p.test1.rxPrimaryLineMm=profile.rxPackageLengthMm[0];
  p.test2.txPrimaryLineMm=profile.txPackageLengthMm[1];
  p.test2.rxPrimaryLineMm=profile.rxPackageLengthMm[1];
  // Finite-band, uniform complex 100-ohm differential networks.
  // These are synthetic fixtures, not vendor/IEEE approved packages.
  for (int k=0;k<=4000;++k) {
    const double f=8e6*k;
    p.frequencyHz.push_back(f);
    ComTwoPortS s1,s2;
    s1.s21=s1.s12=std::polar(0.7,-2.*std::numbers::pi*f*0.4e-9);
    s2.s21=s2.s12=std::polar(0.63,-2.*std::numbers::pi*f*0.405e-9);
    p.test1.loadedSdd.push_back(s1);
    p.test2.loadedSdd.push_back(s2);
  }
  Result<ComPhysicalChannelData> out;
  out.status=Status::Warning;
  out.value=std::move(p);
  out.provenance=source();
  out.provenance.settingsIdentity.clear(); // Physical source precedes EQ.
  out.provenance.algorithm="com-causal-c2c-historical-physical-network";
  out.provenance.version="012_09-causal-package-1";
  return out;
}
} // namespace
int main() {
  try {
    // Hand-computed, independent 13-symbol DFE least-squares oracle:
    // precursors (0.02,-0.03,0.01), postcursors
    // (0.4,0.1,0.02,0,0,0,0.04,0,0).
    const std::array<double,13> symbols{
      0.02,-0.03,0.01,1.,0.4,0.1,0.02,0,0,0,0.04,0,0};
    const auto taps=minimizeCom120F8IdealDfeResidual(symbols);
    const std::array<double,6> expected{0.4,0.1,0.02,0.,0.,0.};
    demand(taps==expected,"Independent exact six-tap DFE optimum");
    double beforeSquared=0,afterSquared=0,preSquared=0;
    for (size_t k=0;k<6;++k) {
      beforeSquared+=symbols[4+k]*symbols[4+k];
      const double residual=symbols[4+k]-taps[k];
      afterSquared+=residual*residual;
    }
    for (size_t k=10;k<13;++k) {
      beforeSquared+=symbols[k]*symbols[k];
      afterSquared+=symbols[k]*symbols[k];
    }
    for (size_t k=0;k<3;++k)
      preSquared+=symbols[k]*symbols[k];
    close(beforeSquared,0.172,1e-14,"Pre-DFE postcursor golden");
    close(afterSquared,0.0016,1e-14,"Post-DFE postcursor golden");
    close(preSquared,0.0014,1e-14,"Precursor golden");
    close(std::sqrt(preSquared+afterSquared),std::sqrt(0.003),1e-14,
          "Independent complete score oracle");
    auto clipped=symbols;
    clipped[4]=-0.8; clipped[5]=0.9; clipped[6]=-0.8;
    const auto bounded=minimizeCom120F8IdealDfeResidual(clipped);
    const auto profile=final8023ck120F8();
    demand(bounded[0]==profile.dfeMin[0] &&
           bounded[1]==profile.dfeMax[1] &&
           bounded[2]==profile.dfeMin[2],
           "DFE published continuous lower/upper limits");
    auto invalid=symbols;
    invalid[4]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)minimizeCom120F8IdealDfeResidual(invalid);},
           "DFE NaN must fail");
    invalid=symbols; invalid[3]=0.;
    reject([&]{(void)minimizeCom120F8IdealDfeResidual(invalid);},
           "DFE normalized cursor must be 1");

    // End-to-end real complex spectrum -> FFE/CTLE -> rectangular
    // SYMBOL convolution -> bounded optimized DFE at one fixed cursor.
    auto channel=physical();
    Com120F8EqualizerChoice start;
    const double cursor=0.4e-9+
                0.5/final8023ck120F8().symbolRateHz;
    auto p=source();
    const auto found=searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,p);
    demand(found.usable() && found.status==Status::Warning &&
           found.diagnostics.size()==1 &&
           found.diagnostics.front().code=="NOT_IEEE_COM",
           "Exploratory search cannot return IEEE COM verdict");
    const auto& d=*found.value;
    demand(d.attempted==13 && d.evaluated>=1 &&
           d.evaluated+d.rejected==13,
           "Exactly bounded one-step candidate accounting");
    demand(d.best.residualScore <= d.initial.residualScore+1e-12,
           "Chosen candidate cannot worsen baseline ISI objective");
    close(d.best.residualScore,
          std::hypot(d.best.precursorRss,d.best.postcursorRss),
          1e-12,"Chosen objective equals observed precursor/postcursor RSS");
    demand(d.evaluatedCandidates.size()==d.evaluated,
           "Every evaluated candidate remains auditable");
    std::set<std::string> candidateIdentities;
    for (const auto& c: d.evaluatedCandidates) {
      demand(candidateIdentities.insert(c.settingsIdentity).second,
             "All candidate source/EQ identity suffixes must be unique");
      demand(c.settingsIdentity.find("|case=TEST1|")!=std::string::npos &&
             c.settingsIdentity.find("|eq=120F8")!=std::string::npos,
             "Candidate full settings identity remains separated");
      close(c.residualScore,std::hypot(c.precursorRss,c.postcursorRss),
            1e-12,"Every candidate score equals its RSS evidence");
      demand(d.best.residualScore <= c.residualScore+1e-12,
             "Chosen candidate minimum across every audited neighbor");
    }
    validateCom120F8Equalizer(d.best.equalizer);
    validateCom120F8DfeTaps(d.best.dfeTaps);
    demand(d.testCase=="TEST1" &&
           found.provenance.sourceSha256==p.sourceSha256 &&
           found.provenance.workspaceGeneration==21,
           "Full result/source and selected test case retained");

    const auto repeated=searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,p);
    demand(repeated.value->evaluated==d.evaluated &&
           repeated.value->rejected==d.rejected &&
           repeated.value->best.equalizer.txFfe==d.best.equalizer.txFfe &&
           repeated.value->best.equalizer.ctleDcGainDb==
             d.best.equalizer.ctleDcGainDb &&
           repeated.value->best.equalizer.ctleDcGain2Db==
             d.best.equalizer.ctleDcGain2Db &&
           repeated.value->best.dfeTaps==d.best.dfeTaps,
           "Deterministic repeat local optimization");

    const auto other=searchCom120F8LocalEqualizer(
        channel,start,"TEST2",cursor,source("TEST2"));
    demand(other.usable() && other.value->testCase=="TEST2",
           "Different physical Test2 stays independent");
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST2",cursor,p);},
        "Cross-case source-identity substitution forbidden");
    auto altered=p;
    altered.sourceSha256="corrupt";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,altered);},
        "Incomplete SHA source rejected");
    altered=p; altered.mappingGeneration=0;
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,altered);},
        "Missing mapping generation rejected");
    altered=p; altered.settingsIdentity+="|eq=old|";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,altered);},
        "Previously mixed equalizer identity rejected");
    auto wrong=start; wrong.txFfe[0]=-0.01;
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,wrong,"TEST1",cursor,p);},
        "Off-grid FFE candidate rejected");
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",1e-12,p);},
        "Negative UI window rejected");
    reject([&]{(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",std::numeric_limits<double>::infinity(),p);},
        "Infinite cursor timing rejected");
    auto invalidPhysical=channel;
    invalidPhysical.value->model="INVALID";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        invalidPhysical,start,"TEST1",cursor,p);},
        "Unqualified physical model rejected");
    auto truncated=channel;
    truncated.value->frequencyHz.resize(400);
    reject([&]{(void)searchCom120F8LocalEqualizer(
        truncated,start,"TEST1",cursor,p);},
        "Missing Nyquist grid rejected");
    // A matching-looking raw physical model is not sufficient: the
    // origin-bearing Result and its algorithm/version must actually match.
    auto alteredPhysical=channel;
    alteredPhysical.provenance.sourceSha256=std::string(64,'d');
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Different upstream source SHA fails closed");
    alteredPhysical=channel;
    alteredPhysical.provenance.mappingGeneration++;
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Different physical mapping generation fails closed");
    alteredPhysical=channel;
    alteredPhysical.provenance.revision+="tampered";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Different physical revision fails closed");
    alteredPhysical=channel;
    alteredPhysical.provenance.algorithm="unqualified-physical-spectra";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Unknown upstream physical algorithm fails closed");
    alteredPhysical=channel;
    alteredPhysical.provenance.version="wrong-version";
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Unknown physical algorithm version fails closed");
    alteredPhysical=channel;
    alteredPhysical.status=Status::NotEvaluated;
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Not-evaluated physical source rejected");
    alteredPhysical=channel;
    alteredPhysical.value.reset();
    reject([&]{(void)searchCom120F8LocalEqualizer(
        alteredPhysical,start,"TEST1",cursor,p);},
        "Physical source without spectrum rejected");

    auto excessive=channel;
    excessive.value->frequencyHz.clear();
    for (int k=0; k<=32769; ++k)
      excessive.value->frequencyHz.push_back(8e6*static_cast<double>(k));
    reject([&]{(void)searchCom120F8LocalEqualizer(
        excessive,start,"TEST1",cursor,p);},
        "Over-budget frequency grid fails before symbol quadrature");
    // Reject a non-uniform valid-step grid, without silently resampling.
    auto nonuniform=channel;
    nonuniform.value->frequencyHz[200]+=1e6;
    reject([&]{(void)searchCom120F8LocalEqualizer(
        nonuniform,start,"TEST1",cursor,p);},
        "Nonuniform source grid must not be silently integrated");
    std::atomic_bool cancelled{true};
    bool stopped=false;
    try {(void)searchCom120F8LocalEqualizer(
        channel,start,"TEST1",cursor,p,&cancelled);}
    catch(const si::Cancelled&) {stopped=true;}
    demand(stopped,"Cancellation propagates, never becomes a candidate error");

    std::cout<<"012_18: independent DFE golden, bounded search, "
                "physical complex spectrum and fail-closed PASS\n";
  } catch(const std::exception& e) {
    std::cerr<<"012_18 failure: "<<e.what()<<'\n';
    return 1;
  }
  return 0;
}
