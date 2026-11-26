#include "si/channel_analysis/icn.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <array>
#include <limits>
#include <stdexcept>
using namespace si::channel;
namespace {
void require(bool good, const char *name) {
  if (!good) throw std::runtime_error(name);
}
void close(double value,double reference,double tolerance,const char *name) {
  require(std::abs(value-reference)<=tolerance*std::max(1e-12,std::abs(reference)),name);
}
ComplexTrace flat(double gain, bool uneven=false) {
  ComplexTrace t;
  t.frequencyHz=uneven?std::vector<double>{0,0.1e9,0.5e9,2e9}
                       :std::vector<double>{0,0.5e9,1e9,1.5e9,2e9};
  t.transfer.assign(t.frequencyHz.size(),si::Complex(gain,0));
  t.stimulusOhm=t.responseOhm=50;
  t.parameter="S31";
  return t;
}
IcnSettings settings() {
  IcnSettings s;
  s.startHz=0;s.stopHz=2e9;s.symbolRateHz=4e9;
  s.txPeakToPeakV=0.8;s.riseTimeSeconds=0;s.rxBandwidthHz=1e12;
  return s;
}
double independentPsd(double frequency,const IcnSettings &s) {
  const double x=3.14159265358979323846*frequency/s.symbolRateHz;
  const double sinc= x==0?1:std::sin(x)/x;
  return 2*std::pow(s.txPeakToPeakV/2,2)/s.symbolRateHz*sinc*sinc/
         (1+std::pow(frequency/s.rxBandwidthHz,2));
}
}
int main() {
  try {
    const auto s=settings();
    auto zero=calculateIcn({{"A",IcnEnd::Next,flat(0)}},s);
    require(zero.usable() && zero.value->totalRmsV==0,"Zero crosstalk");
    auto single=calculateIcn({{"A",IcnEnd::Next,flat(0.1)}},s);
    require(single.usable(),"Single source");
    const auto &freq=single.value->frequencyHz;
    double golden=0;
    for(size_t i=1;i<freq.size();++i)
      golden+=0.5*(independentPsd(freq[i-1],s)+independentPsd(freq[i],s))*
              (freq[i]-freq[i-1])*0.01;
    close(single.value->totalRmsV,std::sqrt(golden),1e-12,"Trapezoidal independent oracle");
    auto multi=calculateIcn({{"A",IcnEnd::Next,flat(0.1)},
                            {"B",IcnEnd::Fext,flat(0.2)}},s);
    close(multi.value->totalRmsV,single.value->totalRmsV*std::sqrt(5.0),1e-12,
          "Multiple aggressor power addition");
    close(multi.value->aggressors[0].powerPercent,20.,1e-12,"NEXT percentage");
    auto reversed=calculateIcn({{"A",IcnEnd::Next,flat(-0.1)}},s);
    close(reversed.value->totalRmsV,single.value->totalRmsV,1e-12,"Sign invariant");
    auto doubled=s;doubled.txPeakToPeakV*=2;
    close(calculateIcn({{"A",IcnEnd::Next,flat(0.1)}},doubled).value->totalRmsV,
      single.value->totalRmsV*2,1e-12,"TX amplitude scaling");
    auto uneven=calculateIcn({{"A",IcnEnd::Next,flat(0.1,true)}},s);
    require(uneven.usable() && uneven.value->frequencyHz.size()==4,
            "Nonuniform frequency");
    // Independent long-double endpoint oracle: exact complex interpolation,
    // nonuniform frequency, finite Gaussian edge and RX corner at 1 GHz.
    auto clipped=s; clipped.startHz=0.25e9; clipped.stopHz=1.6e9;
    clipped.riseTimeSeconds=80e-12; clipped.rxBandwidthHz=1e9;
    auto complex=flat(0,true);
    complex.frequencyHz={0,0.5e9,1e9,2e9};
    complex.transfer={{0.1,0.2},{0.3,-0.2},{-0.1,0.4},{0.5,0.0}};
    auto actual=calculateIcn({{"A",IcnEnd::Next,complex},{"B",IcnEnd::Fext,complex}},clipped);
    require(actual.usable() && actual.status==Status::Warning,"Clipped complex ICN warning");
    const auto &d=*actual.value;
    const std::array<long double,4> f{0.25e9L,0.5e9L,1e9L,1.6e9L};
    // 0.25 GHz: midpoint; 1.6 GHz: 60% through last interval.
    const std::array<long double,4> re{0.2L,0.3L,-0.1L,0.26L};
    const std::array<long double,4> im{0.0L,-0.2L,0.4L,0.16L};
    const long double pi=std::acos(-1.0L);
    long double integral=0, prev=0;
    require(d.frequencyHz.size()==f.size(),"Exact clipped grid size");
    for(size_t i=0;i<f.size();++i) {
      const long double z=pi*f[i]/4e9L;
      const long double edge=2*pi*f[i]*80e-12L/2.5631031310892007L;
      const long double rx=f[i]/1e9L;
      const long double psd=0.32L/4e9L*std::pow(std::sin(z)/z,2)*
          std::exp(-edge*edge)/(1+rx*rx)*(re[i]*re[i]+im[i]*im[i]);
      if(i) integral+=(prev+psd)*(f[i]-f[i-1])/2;
      prev=psd;
      close(d.frequencyHz[i],double(f[i]),1e-13,"Clipped bin");
      require(std::abs(d.nextDensityV2Hz[i]-double(psd))<=1e-23+1e-12*double(psd),"Complex Gaussian RX PSD");
      close(d.nextPowerDb[i],double(10*std::log10(re[i]*re[i]+im[i]*im[i])),1e-12,"Complex power");
      close(d.nextCumulativeRmsV[i],double(std::sqrt(integral)),1e-12,"Cumulative NEXT oracle");
      close(d.fextCumulativeRmsV[i],double(std::sqrt(integral)),1e-12,"Cumulative FEXT oracle");
      close(d.totalCumulativeRmsV[i],double(std::sqrt(2*integral)),1e-12,"Cumulative total oracle");
    }
    close(d.totalRmsV,double(std::sqrt(2*integral)),1e-12,"Complex clipped total");
    close(d.aggressors[0].powerPercent,50,1e-12,"Equal path percentage");
    close(d.aggressors[1].powerPercent,50,1e-12,"Equal path percentage");
    require(d.settings.riseTimeSeconds==clipped.riseTimeSeconds &&
            d.settings.startHz==clipped.startHz,"Immutable result settings");
    auto twice=clipped; twice.txPeakToPeakV*=2;
    auto scaled=calculateIcn({{"A",IcnEnd::Next,complex}},twice);
    for(size_t i=0;i<f.size();++i)
      require(std::abs(scaled.value->nextDensityV2Hz[i]-4*d.nextDensityV2Hz[i])<=1e-22,
              "PSD fourfold voltage scaling");
    std::vector<IcnAggressorInput> many;
    for(int i=0;i<32;++i) many.push_back({std::to_string(i),IcnEnd::Next,flat(0.1)});
    close(calculateIcn(many,s).value->totalRmsV,single.value->totalRmsV*std::sqrt(32.),1e-12,"32 paths RSS");
    auto rejects=[&](const std::vector<IcnAggressorInput>& a,const IcnSettings &cfg) {
      try {(void)calculateIcn(a,cfg);return false;} catch(const si::Error&) {return true;}
    };
    many.push_back({"overflow",IcnEnd::Next,flat(0.1)});
    require(rejects(many,s),"33 paths rejected");
    auto outside=s; outside.startHz=3e9;outside.stopHz=4e9;
    require(rejects({{"A",IcnEnd::Next,flat(.1)}},outside),"No overlap rejected");
    auto mismatch=flat(.1);mismatch.responseOhm=75;
    require(rejects({{"A",IcnEnd::Next,mismatch}},s),"Reference mismatch rejected");
    require(rejects({{"A",IcnEnd::Next,flat(.1)},{"B",IcnEnd::Fext,flat(.1,true)}},s),"Grid mismatch rejected");
    require(rejects({{"A",static_cast<IcnEnd>(9),flat(.1)}},s),"Invalid path type rejected");
    auto noDc=s;noDc.startHz=0;
    auto measured=flat(0.1);measured.frequencyHz.erase(measured.frequencyHz.begin());
    measured.transfer.erase(measured.transfer.begin());
    require(calculateIcn({{"A",IcnEnd::Next,measured}},noDc).status==Status::Warning,
      "No DC must be band-limited");
    auto broken=s; broken.txPeakToPeakV=0;
    bool rejected=false;
    try { (void)calculateIcn({{"A",IcnEnd::Next,flat(0.1)}},broken); }
    catch(const si::Error&) {rejected=true;}
    require(rejected,"Missing absolute TX Vpp");
    rejected=false;
    try { (void)calculateIcn({{"A",IcnEnd::Next,flat(0.1)},
                             {"A",IcnEnd::Next,flat(0.2)}},s); }
    catch(const si::Error&) {rejected=true;}
    require(rejected,"Duplicate aggressor");
    auto invalid=flat(0.1);invalid.transfer[0]={NAN,0};
    rejected=false;
    try { (void)calculateIcn({{"A",IcnEnd::Next,invalid}},s); }
    catch(const si::Error&) {rejected=true;}
    require(rejected,"NaN transfer");
    std::atomic_bool cancelled{true};
    rejected=false;
    try { (void)calculateIcn({{"A",IcnEnd::Next,flat(0.1)}},s,{},&cancelled); }
    catch(const si::Cancelled&) {rejected=true;}
    require(rejected,"Cancelled");
    // Auto values are assumptions; compare explicit settings to the same
    // measured-band numeric engine and verify Vpp linear sensitivity.
    const auto autoTrace=flat(0.1);
    const auto automatic=automaticIcnSettings(autoTrace);
    IcnSettings explicitSettings;
    explicitSettings.startHz=0;explicitSettings.stopHz=2e9;
    explicitSettings.symbolRateHz=2e9/1.5;explicitSettings.txPeakToPeakV=0.4;
    explicitSettings.riseTimeSeconds=30e-12;explicitSettings.rxBandwidthHz=2e9;
    const std::vector<IcnAggressorInput> autoPaths{{"A",IcnEnd::Next,autoTrace},
      {"B",IcnEnd::Fext,flat(0.2)}};
    const auto autoResult=calculateIcn(autoPaths,automatic);
    const auto manualResult=calculateIcn(autoPaths,explicitSettings);
    close(autoResult.value->totalRmsV,manualResult.value->totalRmsV,1e-12,"auto/manual ICN RMS");
    double autoPower=0,previousPsd=0;
    for(size_t i=0;i<autoTrace.frequencyHz.size();++i) {
      const double f=autoTrace.frequencyHz[i];
      const double edge=2*std::acos(-1.)*f*30e-12/2.5631031310892007;
      const double p=0.05*independentPsd(f,explicitSettings)*std::exp(-edge*edge);
      if(i) autoPower+=0.5*(p+previousPsd)*(f-autoTrace.frequencyHz[i-1]);
      previousPsd=p;
    }
    close(autoResult.value->totalRmsV,std::sqrt(autoPower),1e-12,"auto independent PSD integral");
    auto variation=automatic;variation.txPeakToPeakV*=1.1;
    close(calculateIcn(autoPaths,variation).value->totalRmsV,1.1*autoResult.value->totalRmsV,1e-12,"auto amplitude sensitivity");
    const auto noDcAuto=automaticIcnSettings(measured);
    require(calculateIcn({{"A",IcnEnd::Next,measured}},noDcAuto).status==Status::Warning,
      "Auto ICN missing DC remains band limited");
    std::cout<<"012_54 automatic ICN policy, independent PSD and sensitivity PASS\n";
    std::cout<<"012_06 ICN numerical golden PASS\n";
    return 0;
  } catch(const std::exception &e) {
    std::cerr<<"012_06 ICN numeric FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
