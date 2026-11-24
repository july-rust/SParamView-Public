#include "si/channel_analysis/com_fixed_ratio.hpp"
#include "si/channel_analysis/com_voltage_ratio.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace si::channel {
namespace {
void require(bool ok,const char* why){if(!ok)throw si::Error(why);}
void check(const std::atomic_bool* c){if(c&&c->load(std::memory_order_relaxed))throw si::Cancelled();}
bool positive(double x){return std::isnormal(x)&&x>0.;}
bool named(const std::string& s){return !s.empty()&&s.size()<=256&&s.find_first_not_of(" \t\r\n")!=std::string::npos;}
}
Result<ComFixedRatioData> calculateComFixedRatioSynthetic(const ComFixedTailInput& in,
    const Provenance& p,const SourceIdentity& live,const std::atomic_bool* cancelled){
  check(cancelled);
  const auto& id=in.fom.identity;
  require(id.testCase=="TEST1"||id.testCase=="TEST2","012_39 invalid Test case");
  require(p.sourceSha256.size()==64&&std::all_of(p.sourceSha256.begin(),p.sourceSha256.end(),
    [](unsigned char ch){return std::isxdigit(ch)!=0;})&&named(p.revision)&&named(p.mappingIdentity)&&
    named(p.settingsIdentity)&&p.workspaceGeneration>0&&p.mappingGeneration>0,
    "012_39 incomplete source identity");
  require(live==SourceIdentity{p.sourceSha256,p.revision,p.mappingIdentity,p.workspaceGeneration,p.mappingGeneration},
    "012_39 stale source identity");
  const std::string token="|case="+id.testCase+"|";
  const auto pos=p.settingsIdentity.find(token);
  require(pos!=std::string::npos&&p.settingsIdentity.find(token,pos+1)==std::string::npos&&
    p.settingsIdentity.find(id.testCase=="TEST1"?"|case=TEST2|":"|case=TEST1|")==std::string::npos,
    "012_39 missing or mixed case token");
  const auto fom=calculateComFixedFomSynthetic(in.fom);
  check(cancelled);
  const auto tail=calculateComFixedTailSynthetic(in,p,cancelled);
  require(fom.usable()&&tail.usable()&&fom.value->identity==id&&tail.value->identity==id,
    "012_39 unusable upstream calculation");
  const auto& t=tail.value->tail;
  const double s=fom.value->signalV,q=t.oneSidedNoiseQuantileV,
    lo=t.numericalQuantileLowerV,hi=t.numericalQuantileUpperV;
  require(t.testCase==id.testCase&&t.referencePlane==id.decisionPlane&&
    t.calibrationIdentity==in.calibrationIdentity&&t.numericalQualification=="NUMERICALLY_BRACKETED_NOT_CERTIFIED"&&
    positive(s)&&positive(q)&&positive(hi)&&std::isfinite(lo)&&lo<=q&&q<=hi,
    "012_39 incompatible tail or numerical bracket");
  // Same monotone endpoint reversal as 012_29, same stable 012_19 kernel.
  // A nonpositive lower voltage does not imply a finite upper dB endpoint.
  ComFixedRatioData d;
  d.identity=id;d.calibrationIdentity=in.calibrationIdentity;
  d.signalV=s;d.tailV=q;d.probability=in.oneSidedProbability;d.fomDb=fom.value->fomDb;
  d.tailLowerV=lo;d.tailUpperV=hi;
  d.ratioDb=calculateDiagnosticVoltageRatioDb(s,q);
  d.ratioLowerDb=calculateDiagnosticVoltageRatioDb(s,hi);
  if(positive(lo))d.ratioUpperDb=calculateDiagnosticVoltageRatioDb(s,lo);
  else require(lo<=0.,"012_39 unsupported positive subnormal lower voltage");
  require(d.ratioLowerDb<=d.ratioDb&&(!d.ratioUpperDb||d.ratioDb<=*d.ratioUpperDb),
    "012_39 invalid dB bracket");
  d.numericalQualification=t.numericalQualification;
  check(cancelled);
  Result<ComFixedRatioData> out;out.status=Status::Warning;out.provenance=tail.provenance;
  out.provenance.algorithm="com-fixed-ratio-synthetic";out.provenance.version="012_39";
  out.diagnostics=tail.diagnostics;
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Synthetic continuous voltage ratio, not reference discrete A_ni or a certified COM interval."});
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED","Final IEEE PHY reference not qualified."});
  out.value=std::move(d);return out;
}
Result<ComFixedRatioData> assessComFixedRatioSource(const Result<ComFixedNoiseData>& replay,
    std::string_view test,Control* control){
  const auto gate=assessComFixedTailSource(replay,test,control);
  Result<ComFixedRatioData> out;out.status=gate.status;out.provenance=gate.provenance;
  out.provenance.algorithm="com-fixed-ratio-source-eligibility";out.provenance.version="012_39";
  out.diagnostics=gate.diagnostics;
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED","No qualified final A_s/A_ni contract."});
  return out;
}
} // namespace si::channel
