#include "si/channel_analysis/com_fixed_fom.hpp"
#include <cmath>
#include <set>

namespace si::channel {
namespace {
void require(bool ok,const char* why){if(!ok)throw si::Error(why);}
void check(Control* c){if(c)c->check();}
bool named(const std::string& s){return !s.empty()&&s.size()<=4096&&s.find_first_not_of(" \t\r\n")!=std::string::npos;}
bool caseName(std::string_view s){return s=="TEST1"||s=="TEST2";}
bool positive(double x){return std::isnormal(x)&&x>0.;}
bool nonnegative(double x){return x==0.||positive(x);}
void addOwner(std::set<std::string>& owners,const std::string& id){
  require(named(id)&&owners.insert(id).second,"012_37 missing or duplicate noise owner");
}
constexpr std::array<ComFomNoiseKind,4> kinds{
  ComFomNoiseKind::FullResidualIsi,ComFomNoiseKind::FullReferenceJitter,
  ComFomNoiseKind::FullCombinedCrosstalk,ComFomNoiseKind::FullReferenceRx};

// 4.17 Eq. 93A-30 without SNR_TXwC0: cursor * 10^(-SNR/20).
// Normal fast path preserves accuracy; the fallback avoids intermediate
// factor underflow when a very large cursor compensates a large SNR.
double txNoise(double cursor,double snr){
  const double exponent=-snr/20.;
  const double factor=std::pow(10.,exponent);
  const double tx=std::isnormal(factor)?cursor*factor:
    std::exp2(std::log2(cursor)+exponent*std::log2(10.));
  require(positive(tx),"012_37 TX noise outside normal representable range");
  return tx;
}
double voltageScore(double signal,double noise){
  // Mantissa ratio is always in [0.5,2]; exponent difference cannot
  // overflow. Unlike subtraction of two large logs this also preserves
  // a small nonzero score for almost equal voltages.
  int se=0,ne=0;
  const double sm=std::frexp(signal,&se),nm=std::frexp(noise,&ne);
  if(se==ne)return 20.*std::log1p((sm-nm)/nm)/std::log(10.);
  if(se-ne==1)return 20.*std::log1p((2.*sm-nm)/nm)/std::log(10.);
  if(ne-se==1)return 20.*std::log1p((sm-2.*nm)/(2.*nm))/std::log(10.);
  return 20.*(std::log10(sm/nm)+(se-ne)*std::log10(2.));
}
}
Result<ComFixedFomData> calculateComFixedFomSynthetic(const ComFixedFomInput& in,Control* control){
  check(control);
  require(caseName(in.identity.testCase)&&named(in.identity.sourceIdentity)&&
    named(in.identity.settingsIdentity)&&named(in.identity.decisionPlane),
    "012_37 incomplete fixed case identity");
  require(in.c2cNonMmseNoRxFfeNoCalibrationNoC0Declared&&
    in.independentEquiprobablePam4Declared&&in.levels==4,
    "012_37 unsupported reference branch or symbol contract");
  require(named(in.cursorEvidence)&&named(in.txNoiseEvidence)&&named(in.neInactiveEvidence)&&
    positive(in.cursorV)&&positive(in.rlm)&&in.rlm<=1.&&
    std::isfinite(in.snrTxDb)&&in.snrTxDb>=0.,"012_37 invalid amplitude or noise declaration");
  ComFixedFomData data;data.identity=in.identity;
  // Eq. 93A-30 uses the unadjusted cursor; RLM belongs to A_s only.
  data.signalV=(in.cursorV/3.)*in.rlm;
  require(positive(data.signalV),"012_37 signal outside normal representable range");
  std::set<std::string> owners;
  addOwner(owners,in.txNoiseOwner);
  for(std::size_t k=0;k<kinds.size();++k){
    check(control);
    require(in.noise[k].has_value(),"012_37 missing noise component");
    const auto& n=*in.noise[k];
    require(n.identity==in.identity&&n.kind==kinds[k]&&named(n.evidence)&&
      nonnegative(n.rmsV),"012_37 incompatible noise component");
    require(n.explicitlyInactive==(n.rmsV==0.)&&!n.owners.empty()&&
      n.owners.size()<=256,"012_37 missing zero declaration or bounded ownership");
    for(const auto& id:n.owners)addOwner(owners,id);
    data.rmsV[k]=n.rmsV;
  }
  data.rmsV[4]=txNoise(in.cursorV,in.snrTxDb);
  // Reference lines 9538-9545: sigma_ne=0 outside RX calibration.
  data.rmsV[5]=0.;
  for(double n:data.rmsV)data.totalRmsV=std::hypot(data.totalRmsV,n);
  require(positive(data.totalRmsV),"012_37 total noise outside normal representable range");
  data.fomDb=voltageScore(data.signalV,data.totalRmsV);
  require(std::isfinite(data.fomDb),"012_37 nonfinite score");
  check(control);
  Result<ComFixedFomData> out;out.status=Status::Warning;out.value=std::move(data);
  out.provenance.algorithm="com-fixed-fom-synthetic";out.provenance.version="012_37";
  out.provenance.settingsIdentity=in.identity.settingsIdentity;
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Synthetic complete-component FOM arithmetic only; final PHY reference blocked."});
  return out;
}
Result<ComFixedFomData> assessComFixedFomSource(const Result<ComFixedNoiseData>& replay,
    std::string_view testCase,Control* control){
  check(control);
  require(caseName(testCase),"012_37 invalid source case");
  Result<ComFixedFomData> out;out.provenance=replay.provenance;
  out.provenance.algorithm="com-fixed-fom-source-eligibility";out.provenance.version="012_37";
  if(!replay.usable()){
    out.status=(replay.status==Status::Valid||replay.status==Status::Warning)?
      Status::NotEvaluated:replay.status;
    out.diagnostics=replay.diagnostics;
  }else{
    const std::size_t k=testCase=="TEST1"?0u:1u;
    require(replay.value->cases[k].testCase==testCase,"012_37 source case mismatch");
    out.status=Status::NotEvaluated;
  }
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED",
    "012_36 does not establish complete ISI/jitter/crosstalk/RX span and final PHY voltage calibration."});
  check(control);return out;
}
} // namespace si::channel
