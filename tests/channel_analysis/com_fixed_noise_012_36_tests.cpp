// 012_35 bounded source-backed matched-board diagnostics, not IEEE COM.
#include "si/channel_analysis/com_fixed_noise.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void demand(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void close(double v,double g){if(!std::isfinite(v)||std::abs(v/g-1.)>2e-6){std::cerr<<std::setprecision(17)<<v<<" != "<<g<<"\n";throw std::runtime_error("Golden mismatch");}}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const si::Error&){bad=true;}demand(bad,"invalid input accepted");}
void write(const std::filesystem::path& file){
  std::ofstream o(file);o<<"# GHz S RI R 50\n"<<std::setprecision(17);
  for(int k=0;k<=2657;++k){
    double f=k*1e7;std::array<Complex,64> s{};
    auto connect=[&](int a,int b,double gain,double delay){
      auto z=gain*std::polar(1.,-2.*std::numbers::pi*f*delay);
      for(int q=0;q<2;++q){s[8*(a+q)+b+q]=z;s[8*(b+q)+a+q]=z;}
    };
    connect(0,2,.5,12e-12);connect(4,6,.5,12e-12);
    connect(2,6,.05,20e-12);connect(2,4,-.03,35e-12);
    o<<f/1e9;
    for(int a=0;a<8;++a)for(int b=0;b<8;++b)o<<" "<<s[8*b+a].real()<<" "<<s[8*b+a].imag();
    o<<"\n";
  }
  demand(bool(o),"fixture write");
}

}
int main(int argc,char** argv){try{
  demand(argc==2,"scratch directory");std::filesystem::path dir=argv[1];
  std::filesystem::create_directories(dir);write(dir/"board.s8p");
  auto cache=Cache::open(dir/"board.s8p",dir/"cache",nullptr,true);
  Channel v,a;v.id="victim";v.name=v.id;v.nearP=0;v.nearN=1;v.farP=2;v.farN=3;
  a.id="aggressor";a.name=a.id;a.nearP=4;a.nearN=5;a.farP=6;a.farN=7;
  auto source=SourceSnapshot::capture(cache,{v,a},"synthetic-012_36",17,19);
  auto live=source.identity();ComFixedNoiseInput x;
  x.eq.txFfe={-.04,.10,-.20,.56,-.10};x.eq.ctleDcGainDb=-20.;x.eq.ctleDcGain2Db=-4.;
  auto phys=prepareCom120F8HistoricalCausalFromSource(source,v.id,x.eq,live);
  demand(phys.usable(),"physical");
  for(size_t k=0;k<2;++k){
    auto& vc=x.victim[k];vc.testCase=k==0?"TEST1":"TEST2";
    vc.decisionTimeSeconds=k==0?210e-12:430e-12;
    vc.dfeTaps={.30,.05,-.04,.10,0.,.03};
    Result<ComVoltageScaleEvidence> ev;ev.status=Status::Warning;ev.provenance=phys.provenance;
    ComVoltageScaleEvidence e;e.testCase=vc.testCase;e.voltsPerUnitSymbol=k==0?.2:.4;
    e.transmitterReferencePlane="synthetic TX die";e.receiverDecisionPlane="synthetic RX decision";
    e.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
    e.calibrationIdentity="synthetic scale";e.calibrationSourceIdentity="synthetic independent constants";
    e.txEqualizerIdentity=e.rxEqualizerIdentity=phys.provenance.settingsIdentity;
    ev.value=e;vc.voltageScale=ev;
    auto& n=x.noise[k];n.plane=k==0?ComFixedNoisePlane::RxFilterInput:ComFixedNoisePlane::CtleInput;
    n.oneSidedPsdV2PerHz=final8023ck120F8().oneSidedNoiseSpectralDensityV2PerGHz*1e-9;
    n.noiseSource="synthetic RX Gaussian";n.receiverDecisionPlane=e.receiverDecisionPlane;
    n.independenceDeclaration="synthetic independent Gaussian RJ and equal-weight DJ";
  }
  auto run=[&](const auto& y){return calculateComFixedNoiseFromSource(source,v.id,live,y);};
  auto r=run(x);demand(r.usable()&&r.status==Status::Warning,"bounded candidate");
  const double volts[2][3]={{.013941801951709965,.01423926335753524,.01447460824786692},
    {.021496570539206106,.021789620826230417,.021991424621149502}};
  const double slopes[2]={452885351.73340636,420625969.65188146};
  const double variance[2]={5.291375372119368e-7,1.353973295092927e-7};
  const double rms[2]={.0007274184058792689,.0003679637611359204};
  const double rj[2]={8.524900738511179e-5,7.91766531109424e-5};
  for(size_t k=0;k<2;++k){
    const auto& c=r.value->cases[k];demand(c.testCase==x.victim[k].testCase,"case");
    for(size_t j=0;j<3;++j)close(c.localVoltageV[j],volts[k][j]);
    close(c.receiverNoise.value->varianceV2,variance[k]);close(c.receiverNoise.value->rmsV,rms[k]);
    close(c.jitter.value->decisionSlopeVPerSecond,slopes[k]);
    close(c.jitter.value->jitterGaussianRmsV,rj[k]);close(c.jitter.value->dualDiracVoltagePeakV,2*rj[k]);
    close(c.jitter.value->combinedGaussianRmsV,std::hypot(rj[k],rms[k]));
    demand(c.receiverNoise.provenance.settingsIdentity==c.jitter.provenance.settingsIdentity,
      "case provenance");
    demand(!c.crosstalkObservedRmsV&&c.residualIsiObservedRmsV,"missing is not zero");
  }
  demand(r.value->cases[0].jitter.provenance.settingsIdentity!=r.value->cases[1].jitter.provenance.settingsIdentity,"case isolation");
  // Actual historical source-backed crosstalk, with the same declared victim planes.
  auto y=x;ComBoardCrosstalkInput ci;ci.inventoryConfirmed=true;ci.independentEquiprobablePam4Declared=true;
  ci.statisticalAssumptionSource="synthetic independent PAM4";
  auto victim=calculateComFixedVictimFromSource(source,v.id,x.eq,live,x.victim[0],x.victim[1]);
  demand(victim.usable(),"victim");
  ComBoardAggressorChoice next,fext;next.id="NEXT";next.channelId=a.id;next.coupling=ComCoupling::Next;
  fext.id="FEXT";fext.channelId=a.id;fext.coupling=ComCoupling::Fext;
  for(size_t k=0;k<2;++k){
    ci.cases[k]={x.victim[k].testCase,x.victim[k].decisionTimeSeconds,x.eq.ctleDcGainDb,x.eq.ctleDcGain2Db};
    for(auto* ag:{&next,&fext}){
      auto& ac=ag->cases[k];ac.declaredVoltsPerUnitSymbol=.2;ac.calibrationSource="synthetic scale";
      ac.dieCalibrationProvenance=victim.provenance;
      ac.dieCalibrationProvenance->settingsIdentity=k==0?victim.value->test1.settingsIdentity:victim.value->test2.settingsIdentity;
      ac.receiverDecisionPlane="synthetic RX decision";ac.calibrationIdentity="synthetic scale";
    }
  }
  ci.aggressors={next,fext};y.crosstalk=ci;y.historicalPackageAssumption=std::string(kHistoricalAggressorOverlay);
  auto joined=run(y);demand(joined.usable(),"historical replay");
  for(const auto& c:joined.value->cases)demand(c.crosstalkObservedRmsV&&c.crosstalkPsdPowerV2&&!c.crosstalkSettingsIdentity.empty(),"historical ownership");
  y.noise[0].noiseSource="NEXT";rejects([&]{run(y);});
  y=x;y.noise[0].oneSidedPsdV2PerHz=0.;auto zero=run(y);
  demand(std::fpclassify(zero.value->cases[0].receiverNoise.value->rmsV)==FP_ZERO,"declared zero");
  y=x;y.noise[0].oneSidedPsdV2PerHz=1e-320;rejects([&]{run(y);});
  y=x;y.noise[0].oneSidedPsdV2PerHz=-1.;rejects([&]{run(y);});
  y=x;y.noise[0].oneSidedPsdV2PerHz=std::numeric_limits<double>::infinity();rejects([&]{run(y);});
  y=x;y.noise[0].plane=static_cast<ComFixedNoisePlane>(99);rejects([&]{run(y);});
  y=x;y.noise[0].independenceDeclaration.clear();rejects([&]{run(y);});
  y=x;y.noise[0].receiverDecisionPlane="wrong";rejects([&]{run(y);});
  y=x;y.victim[0].voltageScale.reset();rejects([&]{run(y);});
  y=x;y.victim[0].voltageScale->provenance.mappingGeneration++;rejects([&]{run(y);});
  y=x;y.victim[0].testCase="TEST2";rejects([&]{run(y);});
  using R=ComNoiseRepresentation;
  validateComFixedNoiseRepresentations({R::ResidualIsi,R::CrosstalkObserved,R::JitterMixture});
  rejects([]{validateComFixedNoiseRepresentations({R::ResidualIsi,R::PreDfeIsi});});
  rejects([]{validateComFixedNoiseRepresentations({R::CrosstalkObserved,R::CrosstalkPsd});});
  rejects([]{validateComFixedNoiseRepresentations({R::RxGaussian,R::CombinedGaussian});});
  rejects([]{validateComFixedNoiseRepresentations({R::DualDirac,R::JitterMixture});});
  rejects([]{validateComFixedNoiseRepresentations({R::RandomJitter,R::RandomJitter});});
  live.mappingGeneration++;demand(run(x).status==Status::Stale,"stale");live=source.identity();
  Control ctrl;ctrl.cancelled.store(true);bool cancelled=false;
  try{(void)calculateComFixedNoiseFromSource(source,v.id,live,x,&ctrl);}catch(const si::Cancelled&){cancelled=true;}
  demand(cancelled,"cancellation");
  demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  std::cout<<"012_36 bounded fixed EQ noise/jitter Golden PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
