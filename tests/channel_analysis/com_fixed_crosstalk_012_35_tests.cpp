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
      const auto& a=c.aggressors[j];
      demand(a.stimulusPorts[0]==(j==0?6:4)&&a.responsePorts[0]==2,"physical endpoint");
      demand(a.declaredEquivalentV.has_value(),"declared voltage");
      for(size_t i=0;i<13;++i){close(a.pulse.unitSymbolPulse[i],g[k*2+j][i]);
        close((*a.declaredEquivalentV)[i],g[k*2+j][i]*scales[k][j]);}
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
  y=x;y.aggressors[0].cases[0].declaredVoltsPerUnitSymbol=0.;rejects([&]{run(y);});
  y=x;y.cases[0].testCase="TEST2";rejects([&]{run(y);});
  y=x;y.aggressors.push_back(y.aggressors[0]);rejects([&]{run(y);});
  y=x;y.aggressors.push_back(y.aggressors[0]);y.aggressors.back().id="other";rejects([&]{run(y);});
  y=x;y.aggressors[0].cases[0].relativeDelaySeconds=-1.;rejects([&]{run(y);});
  live.mappingGeneration++;demand(run(x).status==Status::Stale,"stale");live=source.identity();
  Control ctrl;ctrl.cancelled.store(true);bool cancel=false;
  try{(void)calculateComMatchedBoardCrosstalkFromSource(source,v.id,live,x,&ctrl);}catch(const si::Cancelled&){cancel=true;}
  demand(cancel,"cancel");demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
  std::cout<<"012_35 matched-board candidate PASS (NOT_IEEE_COM)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
