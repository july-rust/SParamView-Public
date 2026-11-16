#include "si/channel_analysis/com_pipeline.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void require(bool valid, const char *message) {
  if(!valid)throw std::runtime_error(message);
}
void near(Complex got,Complex expected,double tol,const char *message) {
  if(std::abs(got-expected)>tol)throw std::runtime_error(message);
}
template<class Fn>void reject(Fn f,const char *message) {
  bool caught=false;
  try{f();}catch(const Error&){caught=true;}
  require(caught,message);
}
ComPhysicalFourPort idealFour(Complex h) {
  ComPhysicalFourPort p;
  p.referenceOhm={50,50,50,50};
  p.scattering[2*4+0]=h;p.scattering[0*4+2]=h;
  p.scattering[3*4+1]=h;p.scattering[1*4+3]=h;
  return p;
}
ComPhysicalThruRequest ideal() {
  ComPhysicalThruRequest r;
  r.frequencyHz={0,0.5e9,1e9,1.5e9,2e9};
  for(double f:r.frequencyHz)
    r.measured.push_back(idealFour(
        std::polar(0.9,-2*std::numbers::pi*f*100e-12)));
  r.symbolRateHz=2e9;
  r.txPackage.primaryLineSingleEndedOhm=50;
  r.txPackage.secondaryLineSingleEndedOhm=50;
  r.rxPackage.primaryLineSingleEndedOhm=50;
  r.rxPackage.secondaryLineSingleEndedOhm=50;
  return r;
}
}
int main() {
  try {
    auto r=ideal();
    auto d=calculateComPhysicalThru(r);
    require(d.usable()&&d.status==Status::Warning,"non-normative physical response");
    require(d.value->cases[0].packageCase==ComPackageCase::Test1&&
            d.value->cases[1].packageCase==ComPackageCase::Test2,
            "both package test cases computed separately");
    require(d.diagnostics[0].code=="NOT_IEEE_COM",
            "cannot claim IEEE COM qualified");
    for(const auto& row:d.value->cases) {
      require(row.thru.frequencyHz==r.frequencyHz,"frequency preserved");
      require(row.thru.parameter=="Sdd21","direction of forward THRU");
      require(row.thru.stimulusOhm==100,"single-ended 50 to differential 100ohm");
      for(size_t i=0;i<r.frequencyHz.size();++i) {
        near(row.thru.transfer[i],r.measured[i].scattering[2*4+0],
             1e-13,"identity package must preserve Sdd21 transfer");
        near(row.inputReflection[i],{0,0},1e-13,
             "ideal matched package no return reflection");
      }
    }
    // Nonzero explicit package loss: Test2 longer than Test1. Compare
    // independently with matched-line exponential attenuation and phase.
    r.txPackage.attenuationSqrtGHzPerMm=0.002;
    r.rxPackage.attenuationSqrtGHzPerMm=0.002;
    r.txPackage.delaySecondsPerMm=6e-12;
    r.rxPackage.delaySecondsPerMm=6e-12;
    d=calculateComPhysicalThru(r);
    const size_t k=2;
    const double f=r.frequencyHz[k];
    for(const auto& row:d.value->cases) {
      const auto lens=com8023ckC2cPackageLengths(row.packageCase);
      const double total=lens.txMm+lens.rxMm;
      const double alpha=.002*std::sqrt(f/1e9);
      const Complex expected=r.measured[k].scattering[2*4+0]*
          std::exp(Complex{-alpha*total,
               -2*std::numbers::pi*f*(6e-12)*total});
      near(row.thru.transfer[k],expected,1e-12,
           "full S4P package transfer analytic oracle");
      near(row.inputReflection[k],{0,0},1e-12,
           "matched package reflection 0");
    }
    require(std::abs(d.value->cases[1].thru.transfer[k])<
            std::abs(d.value->cases[0].thru.transfer[k]),
            "Test2 length must have larger attenuation");
    r.reverse=true;
    const auto backward=calculateComPhysicalThru(r);
    require(backward.value->cases[0].thru.parameter=="Sdd12",
            "reverse THRU maps Sdd12");
    near(backward.value->cases[0].thru.transfer[k],
         d.value->cases[0].thru.transfer[k],1e-13,
         "reciprocal ideal board reverse THRU");
    r.reverse=false;
    r.measured[2].scattering[2*4+0]={1.5,0};
    reject([&]{(void)calculateComPhysicalThru(r);},
           "active S4P must fail closed");
    r=ideal();r.measured[2].referenceOhm[3]=75;
    reject([&]{(void)calculateComPhysicalThru(r);},
           "inconsistent port reference");
    r=ideal();r.frequencyHz.erase(r.frequencyHz.begin());
    r.measured.erase(r.measured.begin());
    reject([&]{(void)calculateComPhysicalThru(r);},
           "DC missing");
    r=ideal();r.measured.pop_back();
    reject([&]{(void)calculateComPhysicalThru(r);},
           "missing sample");
    r=ideal();r.txPackage.primaryLineMm=13;
    reject([&]{(void)calculateComPhysicalThru(r);},
           "case length cannot be silently overridden");
    r=ideal();r.symbolRateHz=8e9;
    reject([&]{(void)calculateComPhysicalThru(r);},
           "insufficient measured bandwidth");
    r=ideal();
    std::atomic_bool cancelled{true};
    bool cancelledThrown=false;
    try{(void)calculateComPhysicalThru(r,{},&cancelled);}
    catch(const Cancelled&){cancelledThrown=true;}
    require(cancelledThrown,"must propagate cancellation");
    std::cout<<"012_09 end-to-end physical 4port+package Test1/Test2 "
                "independent analytical PASS; IEEE COM NOT_EVALUATED\n";
    return 0;
  }catch(const std::exception &e) {
    std::cerr<<"012_09 physical THRU test FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
