// Independent 80-digit mixture Goldens, frozen before C++ implementation.
#include "si/channel_analysis/com_fixed_ratio.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace si::channel;
namespace {
SourceIdentity live(const Provenance& p){return {p.sourceSha256,p.revision,p.mappingIdentity,p.workspaceGeneration,p.mappingGeneration};}
void demand(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const si::Error&){bad=true;}demand(bad,"invalid 012_39 input accepted");}
void close(double a,double b){demand(std::isfinite(a)&&std::abs(a-b)<=2e-10*std::abs(b),"012_38 Golden mismatch");}
Provenance provenance(bool second){
  Provenance p;p.sourceSha256=std::string(64,'a');p.revision="synthetic";p.mappingIdentity="map";
  p.settingsIdentity=second?"synthetic|case=TEST2|eq=2":"synthetic|case=TEST1|eq=1";
  p.workspaceGeneration=1;p.mappingGeneration=1;return p;
}
ComFixedTailInput fixture(bool second){
  auto p=provenance(second);ComFixedTailInput x;auto& f=x.fom;
  f.identity={second?"TEST2":"TEST1",p.sourceSha256,p.settingsIdentity,"RX decision V"};
  f.cursorEvidence="synthetic";f.txNoiseOwner="TX";f.txNoiseEvidence="SNR declaration";
  f.neInactiveEvidence="no calibration";f.cursorV=second?.06:.03;f.rlm=.95;f.snrTxDb=20.;f.levels=4;
  f.independentEquiprobablePam4Declared=true;f.c2cNonMmseNoRxFfeNoCalibrationNoC0Declared=true;
  x.calibrationIdentity="synthetic calibration";x.rjOwner="RJ";x.rjEvidence="Gaussian assumption";
  x.rjRmsV=second?.005:.01;x.independentGaussianRxTxRjDeclared=true;
  x.oneSidedProbability=second?1e-8:1e-5;
  const double isi=second?.004:.012,xt=second?-.006:-.009,dj=second?0.:.007;
  x.taps={{f.identity,ComFixedTailRole::ResidualIsi,"ISI","synthetic",isi},
    {f.identity,second?ComFixedTailRole::Fext:ComFixedTailRole::Next,"XT","synthetic",xt}};
  if(!second)x.taps.push_back({f.identity,ComFixedTailRole::SampledDj,"DJ","synthetic",dj});
  const double k=std::sqrt(5./9.);
  f.noise[0]=ComFomNoise{f.identity,ComFomNoiseKind::FullResidualIsi,k*std::abs(isi),{"ISI"},"synthetic",false};
  f.noise[1]=ComFomNoise{f.identity,ComFomNoiseKind::FullReferenceJitter,std::hypot(x.rjRmsV,k*dj),
    second?std::vector<std::string>{"RJ"}:std::vector<std::string>{"RJ","DJ"},"synthetic",false};
  f.noise[2]=ComFomNoise{f.identity,ComFomNoiseKind::FullCombinedCrosstalk,k*std::abs(xt),{"XT"},"synthetic",false};
  f.noise[3]=ComFomNoise{f.identity,ComFomNoiseKind::FullReferenceRx,second?.01:.02,{"RX"},"synthetic",false};
  return x;
}
}
int main(){try{
  auto p=provenance(false),p2=provenance(true);auto x=fixture(false),y=fixture(true);
  auto run=[&](const auto& in){return calculateComFixedRatioSynthetic(in,p,live(p));};
  auto a=run(x),b=calculateComFixedRatioSynthetic(y,p2,live(p2));
  const double gold1=-21.1131006831795014961750022034275,gold2=-12.0582069631140031538215907294917;
  auto dbclose=[](double actual,double expected){demand(std::isfinite(actual)&&
    std::abs(actual-expected)<=2e-9,"012_39 dB Golden mismatch");};
  for(const auto& pair:{std::pair{&a,gold1},std::pair{&b,gold2}}){
    const auto& r=*pair.first;
    demand(r.status==Status::Warning&&r.usable()&&r.provenance.version=="012_39","synthetic status");
    const auto& d=*r.value;dbclose(d.ratioDb,pair.second);
    demand(d.ratioUpperDb&&d.ratioLowerDb<=pair.second&&*d.ratioUpperDb>=pair.second,
      "numeric bracket");
    demand(d.numericalQualification=="NUMERICALLY_BRACKETED_NOT_CERTIFIED"&&
      d.model=="SYNTHETIC_FIXED_EQ_RATIO_NOT_IEEE_COM","qualification");
    demand(d.fomDb>d.ratioDb,"FOM is not tail ratio");
  }
  close(a.value->signalV,.0095);close(b.value->signalV,.019);
  close(a.value->tailV,.107988781162668208425209171806661);
  close(b.value->tailV,.076148955221585369769877956872328);
  demand(a.value->identity==x.fom.identity&&b.value->identity==y.fom.identity,"case");
  for(double scale:{1e-100,1e100}){
    auto z=x;z.fom.cursorV*=scale;z.rjRmsV*=scale;
    for(auto& n:z.fom.noise)n->rmsV*=scale;
    for(auto& t:z.taps)t.voltageV*=scale;
    dbclose(run(z).value->ratioDb,gold1);
  }
  rejects([&]{run(y);});
  auto stale=live(p);++stale.mappingGeneration;
  rejects([&]{calculateComFixedRatioSynthetic(x,p,stale);});
  auto z=x;z.fom.cursorV=0.;rejects([&]{run(z);});
  z=x;z.fom.cursorV=-.03;rejects([&]{run(z);});
  z=x;z.fom.cursorV=std::numeric_limits<double>::denorm_min();rejects([&]{run(z);});
  z=x;z.fom.cursorV=std::numeric_limits<double>::quiet_NaN();rejects([&]{run(z);});
  z=x;z.fom.cursorV=std::numeric_limits<double>::infinity();rejects([&]{run(z);});
  z=x;z.fom.noise[0].reset();rejects([&]{run(z);});
  z=x;z.fom.noise[0]->rmsV*=2.;rejects([&]{run(z);});
  z=x;z.taps[0].identity=y.fom.identity;rejects([&]{run(z);});
  z=x;z.taps[0].identity.decisionPlane="other";rejects([&]{run(z);});
  z=x;z.calibrationIdentity.clear();rejects([&]{run(z);});
  z=x;z.oneSidedProbability=0.;rejects([&]{run(z);});
  z=x;z.taps[0].owner="TX";rejects([&]{run(z);});
  z=x;z.taps.pop_back();rejects([&]{run(z);});
  auto wrong=p;wrong.settingsIdentity+="|case=TEST2|";
  rejects([&]{calculateComFixedRatioSynthetic(x,wrong,live(wrong));});
  std::atomic_bool cancelled{true};bool caught=false;
  try{calculateComFixedRatioSynthetic(x,p,live(p),&cancelled);}catch(const si::Cancelled&){caught=true;}
  demand(caught,"cancelled");
  demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  std::cout<<"012_39 fixed EQ synthetic continuous ratio Golden PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
