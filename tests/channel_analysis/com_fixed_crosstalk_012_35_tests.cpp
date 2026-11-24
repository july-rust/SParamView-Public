// 012_35 bounded source-backed matched-board diagnostics, not IEEE COM.
#include "si/channel_analysis/com_aggressor_voltage.hpp"
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
void close(double v,double g){if(!std::isfinite(v)||std::abs(v-g)>5e-10){
  std::cerr<<std::setprecision(17)<<v<<" != "<<g<<"\n";throw std::runtime_error("Golden mismatch");}}
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
const std::array<std::array<double,13>,4> g{{
{.02588504456198018,-.005519043772416381,.0032521986926660096,-.002286806123499959,.0017618990020602575,-.001433099685239769,.0012078145259756734,-.0010437767530894922,.0009189797542612833,-.0008208375212026723,.0007416295009375837,-.0006763557410185097,.0006216350506346206},
{-.010359376821260976,-.003913103749014516,.002872787165516664,-.0014308256182712438,.0009956844558741361,-.0007659293398005429,.000623129699098824,-.0005255118704050829,.000454444442395348,-.00040033892256374304,.0003577440499927685,-.00032332600926722796,.00029492880343489394},
{-.002249911602443047,.0017372212811520731,-.001415416353511192,.0011945005449341929,-.0010333834920879352,.0009106449782460301,-.0008140155488242672,.0007359573245291658,-.0006715817873781127,.0006175789499521329,-.0005716273944619842,.0005320506541010591,-.0004976076771863335},
{-.0006201585491416415,.00041025645393064433,-.00030394408498492345,.00024017854004421688,-.00019789586788964696,.0001678929871428105,-.00014554011996056095,.000128263738207871,-.00011452286814575618,.00010334010252333411,-.00009406634759716445,.00008625406731757426,-.00007958491435224492}}};
const std::array<std::array<double,13>,4> dieGolden{{
{-.00026384746028532417,.0001227527867050105,.0028980694599910783,.031446430530180577,.012665881457450696,-.00078992435382016404,.0016171398298877875,-.00045027383089263898,.0008445207289030915,-.00041580558940890742,.00065748800402105217,-.000037766416031101446,-.00073985478085744768},
{-.00066277893244029711,.0012925089424234498,-.0015032036734836296,.0017639510816289264,-.0037108709570846722,-.0078555378022158937,.0019084600201087786,-.00091229342784574516,.00055174235020607706,-.00054121797875594378,.00035271712939374567,-.00032934432783939465,.00020077727589012365},
{-.00062383039776067566,.00077336320736772911,-.000025167473004955463,.021864012740550422,.018097593373296184,.0016994031452044248,.0024752427640618519,.00010734843876800785,.0011570991737144835,-.00007376645458198509,.00072669016094357433,-.00011385826886338313,.00052529801505090822},
{-.00042145831364841289,.00092278815806345546,-.0010882860209484469,.0013641358589908839,-.0034929958564043962,-.0060562739276036606,.00069473829816050928,-.00077056955573625739,.00025148998929380939,-.00044821545076277722,.00019697529709869358,-.00030446626413991229,.00016154953008880565}}};
const double diePsd[2][2]={{2.5883818247527124e-5,4.517203400547889e-6},
                          {7.2934768455975207e-5,3.0781069021215816e-7}};
}
int main(int argc,char** argv){try{
  demand(argc==2,"scratch directory");std::filesystem::path dir=argv[1];
  std::filesystem::create_directories(dir);write(dir/"board.s8p");
  auto cache=Cache::open(dir/"board.s8p",dir/"cache",nullptr,true);
  Channel v,a;v.id="victim";v.name=v.id;v.nearP=0;v.nearN=1;v.farP=2;v.farN=3;
  a.id="aggressor";a.name=a.id;a.nearP=4;a.nearN=5;a.farP=6;a.farN=7;
  auto source=SourceSnapshot::capture(cache,{v,a},"synthetic-012_35",17,19);
  auto live=source.identity();ComBoardCrosstalkInput x;
  x.inventoryConfirmed=true;x.independentEquiprobablePam4Declared=true;
  x.statisticalAssumptionSource="synthetic independent equiprobable PAM4 only";
  x.cases[0]={"TEST1",110e-12,0.,0.};x.cases[1]={"TEST2",170e-12,0.,0.};
  ComBoardAggressorChoice next,fext;
  next.id="a-next";next.channelId=a.id;next.coupling=ComCoupling::Next;
  fext.id="b-fext";fext.channelId=a.id;fext.coupling=ComCoupling::Fext;
  next.cases[1].relativeDelaySeconds=5e-12;
  double scales[2][2]={{.2,.3},{.4,.1}};
  for(size_t k=0;k<2;++k){
    fext.cases[k].txFfe={-.04,.10,-.20,.56,-.10};
    next.cases[k].declaredVoltsPerUnitSymbol=scales[k][0];
    fext.cases[k].declaredVoltsPerUnitSymbol=scales[k][1];
    next.cases[k].calibrationSource=fext.cases[k].calibrationSource="synthetic board scale";
  }
  x.aggressors={fext,next};
  auto run=[&](const auto& y){return calculateComMatchedBoardCrosstalkFromSource(source,v.id,live,y);};
  auto r=run(x);demand(r.usable()&&r.status==Status::Warning,"bounded candidate");
  for(size_t k=0;k<2;++k){
    const auto& c=r.value->cases[k];demand(c.aggressors.size()==2,"inventory");
    for(size_t j=0;j<2;++j){
      const auto& path=c.aggressors[j];
      demand(path.stimulusPorts[0]==(j==0?6:4)&&path.responsePorts[0]==2,"physical endpoint");
      demand(path.declaredEquivalentV.has_value(),"declared voltage");
      for(size_t i=0;i<13;++i){close(path.pulse.unitSymbolPulse[i],g[k*2+j][i]);
        close((*path.declaredEquivalentV)[i],g[k*2+j][i]*scales[k][j]);}
    }
    demand(c.observedIndependentPam4RmsV.has_value(),"declared aggregation");
    close(*c.observedIndependentPam4RmsV,k==0?.004790269648063417:.0012057255533006953);
  }
  auto y=x;std::reverse(y.aggressors.begin(),y.aggressors.end());
  demand(run(y).provenance.settingsIdentity==r.provenance.settingsIdentity,"deterministic order");
  y=x;y.aggressors.clear();close(*run(y).value->cases[0].observedIndependentPam4RmsV,0.);
  y.inventoryConfirmed=false;rejects([&]{run(y);});
  y=x;y.independentEquiprobablePam4Declared=false;y.statisticalAssumptionSource.clear();
  demand(!run(y).value->cases[0].observedIndependentPam4RmsV,"no implicit independence");
  y=x;y.aggressors[0].cases[0].declaredVoltsPerUnitSymbol.reset();y.aggressors[0].cases[0].calibrationSource.clear();
  auto missing=run(y);demand(!missing.value->cases[0].observedIndependentPam4RmsV &&
    !missing.value->cases[0].aggressors[1].declaredEquivalentV,"no invented voltage");
  // Codex P2: reject nonzero power outside the qualified normal range,
  // including a scale for which density used to vanish before integration.
  y=x;for(auto& ag:y.aggressors)for(auto& ac:ag.cases)ac.declaredVoltsPerUnitSymbol=1e-156;
  rejects([&]{run(y);});
  // A small but normal integrated power remains available and scales
  // quadratically; compare the dimensionless ratio instead of abs tolerance.
  y=x;for(auto& ag:y.aggressors)for(auto& ac:ag.cases)ac.declaredVoltsPerUnitSymbol=1e-145;
  auto tiny=run(y);
  for(size_t k=0;k<2;++k)for(size_t j=0;j<2;++j) {
    const auto& original=r.value->cases[k].aggressors[j].measuredBandPsdPowerV2;
    const auto& small=tiny.value->cases[k].aggressors[j].measuredBandPsdPowerV2;
    demand(original&&small&&std::isnormal(*small),"normal tiny PSD expected");
    const double expected=(*original/scales[k][j]/scales[k][j])*1e-290;
    demand(std::abs(*small/expected-1.)<1e-12,"tiny PSD quadratic scaling");
  }
  y=x;y.aggressors[0].cases[0].declaredVoltsPerUnitSymbol=0.;rejects([&]{run(y);});
  y=x;y.cases[0].testCase="TEST2";rejects([&]{run(y);});
  y=x;y.aggressors.push_back(y.aggressors[0]);rejects([&]{run(y);});
  y=x;y.aggressors.push_back(y.aggressors[0]);y.aggressors.back().id="other";rejects([&]{run(y);});
  y=x;y.aggressors[0].cases[0].relativeDelaySeconds=-1.;rejects([&]{run(y);});
  live.mappingGeneration++;demand(run(x).status==Status::Stale,"stale");live=source.identity();
  Control ctrl;ctrl.cancelled.store(true);bool cancel=false;
  try{(void)calculateComMatchedBoardCrosstalkFromSource(source,v.id,live,x,&ctrl);}catch(const si::Cancelled&){cancel=true;}
  demand(cancel,"cancel");demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  // Historical role-based package overlay and actual 012_34 victim replay.
  ComHistoricalFixedCrosstalkInput hx;hx.aggressors=x;
  hx.packageAssumption=std::string(kHistoricalAggressorOverlay);
  auto physical=prepareCom120F8HistoricalCausalFromSource(source,v.id,hx.victimEq,live);
  demand(physical.usable(),"victim physical");
  for(size_t k=0;k<2;++k) {
    auto& vc=hx.victimCases[k];auto& cc=hx.aggressors.cases[k];
    cc.decisionTimeSeconds=k==0?210e-12:430e-12;
    vc.testCase=cc.testCase;vc.decisionTimeSeconds=cc.decisionTimeSeconds;
    vc.dfeTaps={.30,.05,-.04,.10,0.,.03};
    si::channel::Result<ComVoltageScaleEvidence> evidence;
    evidence.status=Status::Warning;evidence.provenance=physical.provenance;
    ComVoltageScaleEvidence e;e.testCase=vc.testCase;e.voltsPerUnitSymbol=.2;
    e.transmitterReferencePlane="synthetic victim die";e.receiverDecisionPlane="synthetic die decision";
    e.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
    e.calibrationIdentity="synthetic die scale";e.calibrationSourceIdentity="independent synthetic constants";
    e.txEqualizerIdentity=e.rxEqualizerIdentity=physical.provenance.settingsIdentity;
    evidence.value=e;vc.voltageScale=evidence;
  }
  auto victim=calculateComFixedVictimFromSource(source,v.id,hx.victimEq,live,hx.victimCases[0],hx.victimCases[1]);
  demand(victim.usable(),"012_34 replay");
  for(size_t k=0;k<2;++k)for(auto& ag:hx.aggressors.aggressors) {
    auto& ac=ag.cases[k];ac.calibrationSource="synthetic die constant";
    ac.dieCalibrationProvenance=victim.provenance;
    ac.dieCalibrationProvenance->settingsIdentity=k==0?victim.value->test1.settingsIdentity:victim.value->test2.settingsIdentity;
    ac.receiverDecisionPlane="synthetic die decision";ac.calibrationIdentity="synthetic die scale";
  }
  auto dieRun=[&](const auto& z){return calculateComHistoricalFixedCrosstalkFromSource(source,v.id,live,z);};
  auto dr=dieRun(hx);
  demand(dr.usable()&&dr.status==Status::Warning&&dr.value->crosstalk.model=="EXPLORATORY_HISTORICAL_DIE_XTALK_MATCHED_UNUSED_PORTS","die role model");
  for(size_t k=0;k<2;++k){
    const auto& dc=dr.value->crosstalk.cases[k];
    const auto& vc=k==0?dr.value->victim.test1:dr.value->victim.test2;
    demand(dc.victimSettingsIdentity==vc.settingsIdentity&&dc.timeSeconds==vc.pulse.timeSeconds,"victim replay identity");
    for(size_t j=0;j<2;++j){
      const auto& path=dc.aggressors[j];
      close(path.aggressorPackageMm,j==0?(k==0?11.:29.):(k==0?13.:31.));
      close(path.victimRxPackageMm,k==0?11.:29.);
      demand(path.declaredEquivalentV&&path.measuredBandPsdPowerV2,"declared die scale and PSD");
      for(size_t i=0;i<13;++i){
        close(path.pulse.unitSymbolPulse[i],dieGolden[k*2+j][i]);
        close((*path.declaredEquivalentV)[i],dieGolden[k*2+j][i]*scales[k][j]);
      }
      // Squared volts: independent tighter absolute tolerance than pulse V.
      demand(std::abs(*path.measuredBandPsdPowerV2-diePsd[k][j])<5e-12,"independent die PSD Golden");
    }
    demand(dc.observedIndependentPam4RmsV&&dc.independentMeasuredBandPsdPowerV2,"die aggregate");
    close(*dc.observedIndependentPam4RmsV,k==0?.0055010781860841025:.0085434143707076114);
    demand(std::abs(*dc.independentMeasuredBandPsdPowerV2-(diePsd[k][0]+diePsd[k][1]))<5e-12,"independent aggregate PSD");
  }
  auto bad=hx;bad.packageAssumption.clear();rejects([&]{dieRun(bad);});
  bad=hx;bad.aggressors.cases[0].decisionTimeSeconds+=1e-12;rejects([&]{dieRun(bad);});
  bad=hx;bad.aggressors.cases[0].ctleDcGainDb=-1.;rejects([&]{dieRun(bad);});
  bad=hx;bad.aggressors.cases[0].decisionTimeSeconds=std::nextafter(
      bad.aggressors.cases[0].decisionTimeSeconds,std::numeric_limits<double>::infinity());
  rejects([&]{dieRun(bad);}); // exact immutable case identity, even one ULP
  bad=hx;bad.aggressors.aggressors[0].cases[0].dieCalibrationProvenance->mappingGeneration++;rejects([&]{dieRun(bad);});
  bad=hx;bad.aggressors.aggressors[0].cases[0].receiverDecisionPlane="board";rejects([&]{dieRun(bad);});
  bad=hx;bad.aggressors.aggressors[0].cases[0].dieCalibrationProvenance.reset();rejects([&]{dieRun(bad);});
  bad=hx;auto& missingScale=bad.aggressors.aggressors[0].cases[0];missingScale.declaredVoltsPerUnitSymbol.reset();
  missingScale.dieCalibrationProvenance.reset();missingScale.calibrationSource.clear();
  missingScale.receiverDecisionPlane.clear();missingScale.calibrationIdentity.clear();
  auto absent=dieRun(bad);demand(!absent.value->crosstalk.cases[0].observedIndependentPam4RmsV&&
      !absent.value->crosstalk.cases[0].independentMeasuredBandPsdPowerV2,"no invented die aggregate");
  live.mappingGeneration++;demand(dieRun(hx).status==Status::Stale,"die stale");
  std::cout<<"012_35 board and historical die independent Goldens PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
