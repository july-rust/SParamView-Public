#include "si/channel_analysis/icn.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
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
    std::cout<<"012_06 ICN numerical golden PASS\n";
    return 0;
  } catch(const std::exception &e) {
    std::cerr<<"012_06 ICN numeric FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
