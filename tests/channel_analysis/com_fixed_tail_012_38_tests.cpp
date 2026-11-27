// Independent 80-digit mixture Goldens, frozen before C++ implementation.
#include "si/channel_analysis/com_fixed_tail.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace si::channel;
namespace {
void demand(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const si::Error&){bad=true;}demand(bad,"invalid 012_38 input accepted");}
void close(double a,double b){demand(std::isfinite(a)&&std::abs(a-b)<=2e-10*std::abs(b),"012_38 Golden mismatch");}
Provenance provenance(bool second){
  Provenance p;p.sourceSha256=std::string(64,'a');p.revision="synthetic";p.mappingIdentity="map";
  p.settingsIdentity=second?"case=TEST2|eq=2":"case=TEST1|eq=1";
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
  auto run=[&](const auto& in){return calculateComFixedTailSynthetic(in,p);};
  auto a=run(x),b=calculateComFixedTailSynthetic(y,p2);
  demand(a.status==Status::Warning&&b.status==Status::Warning&&a.usable()&&b.usable(),"synthetic status");
  const double q1=.107988781162668208425209171806661,q2=.076148955221585369769877956872328;
  for(const auto& pair:{std::pair{&a,q1},std::pair{&b,q2}}){
    const auto& t=pair.first->value->tail;close(t.oneSidedNoiseQuantileV,pair.second);
    demand(t.numericalQuantileLowerV<pair.second&&t.numericalQuantileUpperV>pair.second,"numeric bracket");
    demand(t.numericalQualification=="NUMERICALLY_BRACKETED_NOT_CERTIFIED","qualification");
  }
  close(a.value->txRmsV,.003);close(b.value->txRmsV,.006);
  close(a.value->gaussianRmsV,.02256102834535695585147882);
  close(b.value->gaussianRmsV,.01268857754044952064432785);
  demand(a.value->tail.statesEvaluated==64&&b.value->tail.statesEvaluated==16,"state count");
  demand(a.value->identity==x.fom.identity&&b.value->identity==y.fom.identity,"case");
  rejects([&]{run(y);});
  auto z=x;z.taps[0].identity=y.fom.identity;rejects([&]{run(z);});
  z=x;z.taps[0].identity.decisionPlane="other";rejects([&]{run(z);});
  z=x;z.taps[0].owner="TX";rejects([&]{run(z);});
  z=x;z.taps[1].owner="ISI";rejects([&]{run(z);});
  z=x;z.taps[2].owner="RJ";rejects([&]{run(z);});
  z=x;z.taps[0].voltageV*=2.;rejects([&]{run(z);});
  z=x;z.taps.pop_back();rejects([&]{run(z);});
  z=x;z.fom.noise[3].reset();rejects([&]{run(z);});
  z=x;z.fom.noise[3]->owners={"RX","extra"};rejects([&]{run(z);});
  z=x;z.rjRmsV=0.;rejects([&]{run(z);});
  z=x;z.rjEvidence.clear();rejects([&]{run(z);});
  z=x;z.independentGaussianRxTxRjDeclared=false;rejects([&]{run(z);});
  z=x;z.oneSidedProbability=1e-13;rejects([&]{run(z);});
  z=x;z.oneSidedProbability=.11;rejects([&]{run(z);});
  z=x;z.taps[0].voltageV=std::numeric_limits<double>::infinity();rejects([&]{run(z);});
  z=x;while(z.taps.size()<7)z.taps.push_back(z.taps[0]);rejects([&]{run(z);});
  z=x;z.fom.cursorV*=1e-180;z.rjRmsV*=1e-180;
  for(auto& n:z.fom.noise)n->rmsV*=1e-180;
  for(auto& tap:z.taps)tap.voltageV*=1e-180;
  rejects([&]{run(z);}); // legacy variance underflow must not pass
  z=x;z.fom.cursorV*=1e180;z.rjRmsV*=1e180;
  for(auto& n:z.fom.noise)n->rmsV*=1e180;
  for(auto& tap:z.taps)tap.voltageV*=1e180;
  rejects([&]{run(z);});
  auto g=syntheticStrictLowerCdfCrossing({.0625,.0625,.75,.125},-2,.01,.0625);
  demand(g.index==1&&g.signedVoltageV==-.01&&g.magnitudeV==.01&&g.cdfBefore==.0625&&g.cdfAt==.125,"strict equality");
  g=syntheticStrictLowerCdfCrossing({.0625,0.,.0625,.875},-3,.01,.0625);
  demand(g.index==2&&g.signedVoltageV==-.01,"zero-mass plateau");
  g=syntheticStrictLowerCdfCrossing({.0625,.9375},-1,.01,.01);
  demand(g.index==0&&g.cdfBefore==0.,"first bin");
  rejects([&]{syntheticStrictLowerCdfCrossing({1.-5e-13},-1,.01,1e-12);});
  rejects([&]{syntheticStrictLowerCdfCrossing({.1,.8},-1,.01,.05);});
  rejects([&]{syntheticStrictLowerCdfCrossing({.1,-.1,1.},-1,.01,.05);});
  rejects([&]{syntheticStrictLowerCdfCrossing({.1,.9},2,std::numeric_limits<double>::max(),.05);});
  std::atomic_bool cancelled{true};bool caught=false;
  try{calculateComFixedTailSynthetic(x,p,&cancelled);}catch(const si::Cancelled&){caught=true;}
  demand(caught,"cancelled");
  demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  std::cout<<"012_38 bounded synthetic tail and strict CDF Golden PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
