// 012_34: independent ABCD / measured-band pulse Golden, synthetic only.
#include "si/channel_analysis/com_causal_channel.hpp"
#include "si/channel_analysis/com_voltage_scaling.hpp"
#include "si/channel_analysis/com_120f8.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

using namespace si;
using namespace si::channel;

namespace {
void demand(bool ok, const char* why) {
  if(!ok) throw std::runtime_error(why);
}
template<typename F> void rejects(F&& function, const char* why) {
  bool failed=false;
  try { function(); } catch(const si::Error&) { failed=true; }
  demand(failed,why);
}
ComPhysicalFourPort synthetic4p(double f) {
  ComPhysicalFourPort sample;
  sample.referenceOhm={50.,50.,50.,50.};
  const Complex g=std::polar(0.75,
      -2.*std::numbers::pi*f*12.e-12);
  // Four physical ports (nearP, nearN, farP, farN).
  // No mixed-mode conversion; matched differential board transfer.
  for(int pair=0;pair<2;++pair) {
    sample.scattering[4*(pair+2)+pair]=g;
    sample.scattering[4*pair+(pair+2)]=g;
  }
  return sample;
}
ComCausalPhysicalInput writeSyntheticS4p(const std::filesystem::path& file) {
  ComCausalPhysicalInput input;
  std::ofstream out(file);
  demand(bool(out),"cannot create ephemeral synthetic physical S4P");
  out<<"# GHz S RI R 50\n"<<std::setprecision(17);
  for(int k=0;k<=2657;++k) {
    const double f=static_cast<double>(k)*1.e7;
    const auto sample=synthetic4p(f);
    input.frequencyHz.push_back(f);
    input.boardS4p.push_back(sample);
    out<<f/1.e9;
    // Touchstone v1: stimulus column first; measured response inside.
    for(int stimulus=0;stimulus<4;++stimulus)
      for(int response=0;response<4;++response) {
        const Complex s=sample.scattering[4*response+stimulus];
        out<<" "<<s.real()<<" "<<s.imag();
      }
    out<<"\n";
  }
  out.close();
  demand(bool(out),"synthetic S4P write failed");
  return input;
}

void close(double actual,double expected,const char* what) {
  // Locked before executing the C++ candidate: absolute 5e-10 for unit
  // symbols, volts and observed RSS. Synthetic model comparison only.
  if(!std::isfinite(actual) || std::abs(actual-expected)>5e-10) {
    std::cerr<<what<<": "<<std::setprecision(17)<<actual<<" expected "<<expected<<"\n";
    throw std::runtime_error(what);
  }
}
si::channel::Result<ComVoltageScaleEvidence> evidence(const Provenance& p,
      const char* which,double factor) {
  si::channel::Result<ComVoltageScaleEvidence> result;
  result.status=Status::Warning;
  result.provenance=p;
  ComVoltageScaleEvidence s;
  s.testCase=which;
  s.voltsPerUnitSymbol=factor;
  s.transmitterReferencePlane="synthetic TX die matched-wave";
  s.receiverDecisionPlane="synthetic RX decision";
  s.normalizationIdentity="EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL";
  s.calibrationIdentity="synthetic scale only";
  s.calibrationSourceIdentity="independent synthetic constants";
  s.txEqualizerIdentity=s.rxEqualizerIdentity=p.settingsIdentity;
  result.value=s;
  return result;
}
void compare(const ComFixedVictimCaseData& c,const std::array<double,13>& expected,
      const ComFixedVictimCaseChoice& choice,double scale) {
  demand(c.testCase==choice.testCase && c.voltage.has_value(),"case/voltage role");
  demand(c.pulse.timeSeconds.size()==13 && c.pulse.unitSymbolPulse.size()==13,
         "13 symbol samples expected");
  double before=0.,after=0.;
  for(size_t k=0;k<13;++k) {
    const double residual=(k>=4 && k<=9)?
        expected[k]-choice.dfeTaps[k-4]*expected[3]:expected[k];
    close(c.pulse.unitSymbolPulse[k],expected[k],"independent pulse Golden");
    close(c.residualUnitSymbol[k],residual,"independent residual Golden");
    close(c.voltage->before.equivalentPulseV[k],expected[k]*scale,"voltage Golden");
    close(c.voltage->residualV[k],residual*scale,"residual voltage Golden");
    demand(std::abs(c.pulse.timeSeconds[k]-(choice.decisionTimeSeconds+
       (static_cast<int>(k)-3)/53.125e9))<1e-21,"absolute decision grid");
    if(k!=3) {before+=expected[k]*expected[k];after+=residual*residual;}
    if(k<4 || k>9) demand(c.residualUnitSymbol[k]==c.pulse.unitSymbolPulse[k],
                          "uncompensated samples must remain exact");
  }
  close(c.dfe.cursorAmplitudeUnitSymbol,expected[3],"cursor Golden");
  close(c.isiBefore.totalRss,std::sqrt(before),"RSS before Golden");
  close(c.isiAfter.totalRss,std::sqrt(after),"RSS after Golden");
  close(c.voltage->isiAfterV.pam4Rms,std::sqrt(after*5./9.)*scale,"PAM4 assumption Golden");
}
}
int main(int argc,char** argv) {
  try {
    demand(argc==2,"expected CTest scratch directory");
    namespace fs=std::filesystem;
    const fs::path root=argv[1];
    fs::create_directories(root);
    (void)writeSyntheticS4p(root/"victim-012_34.s4p");
    auto cache=Cache::open(root/"victim-012_34.s4p",root/"cache",nullptr,true);
    Channel ch; ch.id="victim"; ch.name="SYNTHETIC";
    ch.nearP=0;ch.nearN=1;ch.farP=2;ch.farN=3;
    const auto source=SourceSnapshot::capture(cache,{ch},"synthetic-012_34",17,19);
    auto live=source.identity();
    Com120F8EqualizerChoice eq;
    eq.txFfe={-.04,.10,-.20,.56,-.10};eq.ctleDcGainDb=-20.;eq.ctleDcGain2Db=-4.;
    const auto phys=prepareCom120F8HistoricalCausalFromSource(source,"victim",eq,live);
    demand(phys.usable(),"012_33 physical fixture");
    ComFixedVictimCaseChoice a,b;
    a.testCase="TEST1";a.decisionTimeSeconds=210e-12;
    b.testCase="TEST2";b.decisionTimeSeconds=430e-12;
    a.dfeTaps={.30,.05,-.04,.10,0.,.03};
    b.dfeTaps={.65,.15,.10,-.04,.02,0.};
    a.voltageScale=evidence(phys.provenance,"TEST1",.2);
    b.voltageScale=evidence(phys.provenance,"TEST2",.4);
    const auto run=[&](const auto& x,const auto& y) {
      return calculateComFixedVictimFromSource(source,"victim",eq,live,x,y);
    };
    const auto result=run(a,b);
    demand(result.status==Status::Warning && result.usable(),"bounded source victim result");
    const std::array<double,13> ga{
      -.011698612279721649,.023870951148436334,-.037773084485843031,.10711188985613262,
      -.031364929321093075,-.044488008128143894,.018918136284816211,-.012359806707692402,
      .0085546805204671506,-.0075941148305384227,.0058021556385625429,-.0051386441967247263,.0034996698836904728};
    const std::array<double,13> gb{
      -.0055070238864009333,.013510662926234658,-.02188779123646524,.082149621453064856,
      -.023849886394776108,-.029513257447445766,.0082139086091590242,-.0063524052867121126,
      .0035209033201879557,-.0034031392408318982,.0021594856874470837,-.0023398130895621806,.0015650006238485648};
    compare(result.value->test1,ga,a,.2);compare(result.value->test2,gb,b,.4);
    demand(result.value->test1.settingsIdentity!=result.value->test2.settingsIdentity,
           "case provenance must differ");
    auto altered=a; altered.decisionTimeSeconds+=1./53.125e9/32.;
    const auto shifted=run(altered,b);
    demand(shifted.usable() &&
      shifted.provenance.settingsIdentity!=result.provenance.settingsIdentity &&
      std::abs(shifted.value->test1.dfe.cursorAmplitudeUnitSymbol-
               result.value->test1.dfe.cursorAmplitudeUnitSymbol)>1e-6,
      "phase shift must change pulse and settings");
    demand(shifted.value->test2.pulse.unitSymbolPulse==result.value->test2.pulse.unitSymbolPulse,
      "phase change must not affect other case");
    altered=a;altered.voltageScale.reset();
    const auto noVolts=run(altered,b);
    demand(noVolts.usable() && !noVolts.value->test1.voltage &&
      noVolts.value->test2.voltage,"no invented voltage");
    altered=a;altered.testCase="TEST2";
    rejects([&]{(void)run(altered,b);},"case swap");
    altered=a;altered.dfeTaps[0]=.29;
    rejects([&]{(void)run(altered,b);},"tap range");
    altered=a;altered.decisionTimeSeconds=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)run(altered,b);},"invalid time");
    altered=a;altered.voltageScale->value->voltsPerUnitSymbol=0.;
    rejects([&]{(void)run(altered,b);},"invalid scale");
    altered=a;altered.voltageScale->provenance.mappingGeneration++;
    rejects([&]{(void)run(altered,b);},"mismatched scale provenance");
    altered=a;altered.voltageScale->value->txEqualizerIdentity="other EQ";
    rejects([&]{(void)run(altered,b);},"mismatched EQ");
    live.workspaceGeneration++;
    const auto stale=run(a,b);
    demand(stale.status==Status::Stale && !stale.value,"stale must not publish pair");
    live=source.identity();
    Control ctrl;ctrl.cancelled.store(true);
    bool cancelled=false;
    try {(void)calculateComFixedVictimFromSource(source,"victim",eq,live,a,b,0.,&ctrl);}
    catch(const si::Cancelled&){cancelled=true;}
    demand(cancelled,"cancellation required");
    demand(calculateCom({},ComSettings{}).status==Status::NotEvaluated,"production gate");
    std::cout<<"012_34 independent fixed pulse/DFE Golden PASS (NOT_IEEE_COM)\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
