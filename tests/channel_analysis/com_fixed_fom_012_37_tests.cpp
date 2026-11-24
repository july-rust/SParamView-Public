// Independent Decimal expectations frozen before implementation.
#include "si/channel_analysis/com_fixed_fom.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace si::channel;
namespace {
void demand(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void close(double actual,double expected,double relative=2e-12){
  demand(std::isfinite(actual)&&std::abs(actual-expected)<=
    relative*std::abs(expected),"012_37 Golden mismatch");
}
void score(double actual,double expected){
  demand(std::isfinite(actual)&&std::abs(actual-expected)<=2e-11,"012_37 dB Golden mismatch");
}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const si::Error&){bad=true;}demand(bad,"invalid FOM accepted");}
ComFixedFomInput fixture(bool second=false){
  ComFixedFomInput x;
  x.identity={second?"TEST2":"TEST1","synthetic complete inputs",second?"EQ2":"EQ1","RX decision volts"};
  x.cursorEvidence="independent analytic cursor";x.txNoiseOwner="TX";x.txNoiseEvidence="declared SNR";
  x.neInactiveEvidence="reference no RX calibration";x.cursorV=second?.06:.12;
  x.rlm=second?.9:.95;x.snrTxDb=second?20.:30.;x.levels=4;
  x.independentEquiprobablePam4Declared=true;x.c2cNonMmseNoRxFfeNoCalibrationNoC0Declared=true;
  const ComFomNoiseKind k[]={ComFomNoiseKind::FullResidualIsi,ComFomNoiseKind::FullReferenceJitter,
    ComFomNoiseKind::FullCombinedCrosstalk,ComFomNoiseKind::FullReferenceRx};
  const double a[]={.004,.002,.003,.002},b[]={.010,.008,.007,.005};
  for(std::size_t i=0;i<4;++i)
    x.noise[i]=ComFomNoise{x.identity,k[i],second?b[i]:a[i],{"owner"+std::to_string(i)},"analytic",false};
  return x;
}
void disable(ComFixedFomInput& x){for(auto& n:x.noise){n->rmsV=0.;n->explicitlyInactive=true;}}
}
int main(){try{
  auto x=fixture(),y=fixture(true);
  auto run=[](const auto& in){return calculateComFixedFomSynthetic(in);};
  auto r=run(x),s=run(y);
  demand(r.status==Status::Warning&&s.status==Status::Warning&&r.usable()&&s.usable(),"synthetic status");
  demand(r.value->identity==x.identity&&s.value->identity==y.identity,"case identity");
  close(r.value->signalV,.038);close(r.value->rmsV[4],.0037947331922020552);
  close(r.value->totalRmsV,.00688476579122340225);score(r.value->fomDb,14.8378885155953525);
  close(s.value->signalV,.018);close(s.value->rmsV[4],.006);
  close(s.value->totalRmsV,.0165529453572468486);score(s.value->fomDb,.72794447386224176);
  for(std::size_t i=0;i<4;++i){close(r.value->rmsV[i],x.noise[i]->rmsV);close(s.value->rmsV[i],y.noise[i]->rmsV);}
  demand(r.value->rmsV[5]==0.&&s.value->rmsV[5]==0.,"ne inactive");
  for(double scale:{1e-280,1e280}){
    auto z=x;z.cursorV*=scale;for(auto& n:z.noise)n->rmsV*=scale;
    auto q=run(z);close(q.value->totalRmsV,r.value->totalRmsV*scale);score(q.value->fomDb,r.value->fomDb);
  }
  auto z=x;disable(z);z.cursorV=3.;z.rlm=1.;z.snrTxDb=20.;
  score(run(z).value->fomDb,10.4575749056067513);
  z.cursorV=3.;z.snrTxDb=0.;score(run(z).value->fomDb,-9.54242509439324875);
  z.cursorV=3.;z.snrTxDb=20.*std::log10(3.);score(run(z).value->fomDb,0.);
  z=x;z.noise[0].reset();rejects([&]{run(z);});
  z=x;z.noise[0]->identity=y.identity;rejects([&]{run(z);});
  z=x;z.noise[0]->identity.decisionPlane="other";rejects([&]{run(z);});
  z=x;z.noise[1]->kind=ComFomNoiseKind::ObservedJitter;rejects([&]{run(z);});
  z=x;z.noise[0]->kind=ComFomNoiseKind::PreDfeIsi;rejects([&]{run(z);});
  z=x;z.noise[2]->kind=ComFomNoiseKind::ObservedCrosstalk;rejects([&]{run(z);});
  z=x;z.noise[3]->kind=ComFomNoiseKind::CombinedGaussian;rejects([&]{run(z);});
  z=x;z.noise[2]->owners.push_back(z.txNoiseOwner);rejects([&]{run(z);});
  z=x;z.noise[2]->owners.push_back(z.noise[0]->owners[0]);rejects([&]{run(z);});
  z=x;z.noise[0]->rmsV=0.;rejects([&]{run(z);});
  z=x;z.noise[0]->rmsV=-1.;rejects([&]{run(z);});
  z=x;z.noise[0]->rmsV=std::numeric_limits<double>::quiet_NaN();rejects([&]{run(z);});
  z=x;z.noise[0]->rmsV=std::numeric_limits<double>::infinity();rejects([&]{run(z);});
  z=x;z.cursorV=0.;rejects([&]{run(z);});
  z=x;z.rlm=1.1;rejects([&]{run(z);});
  z=x;z.levels=1;rejects([&]{run(z);});
  z=x;z.snrTxDb=-1.;rejects([&]{run(z);});
  z=x;z.snrTxDb=1e6;rejects([&]{run(z);});
  z=x;z.cursorV=1e-320;rejects([&]{run(z);});
  z=x;z.c2cNonMmseNoRxFfeNoCalibrationNoC0Declared=false;rejects([&]{run(z);});
  z=x;z.neInactiveEvidence.clear();rejects([&]{run(z);});
  z=x;z.identity.sourceIdentity.clear();rejects([&]{run(z);});
  z=x;for(auto& n:z.noise)n->rmsV=1.7e308;rejects([&]{run(z);});
  si::Control ctrl;ctrl.cancelled.store(true);bool cancelled=false;
  try{calculateComFixedFomSynthetic(x,&ctrl);}catch(const si::Cancelled&){cancelled=true;}
  demand(cancelled,"cancelled");
  demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  std::cout<<"012_37 synthetic FOM Golden PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
