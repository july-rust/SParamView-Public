#include "si/channel_analysis/com_physical_channel.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void require(bool ok,const char* message) {
  if(!ok)throw std::runtime_error(message);
}
void near(Complex actual,Complex expected,double epsilon,const char* msg) {
  if(!std::isfinite(actual.real()) || !std::isfinite(actual.imag()) ||
     std::abs(actual-expected)>epsilon)throw std::runtime_error(msg);
}
template<class F>void rejected(F fn,const char* message) {
  bool caught=false;
  try {fn();} catch(const Error&){caught=true;}
  require(caught,message);
}
ComPhysicalFourPort wires(Complex gain,Complex reflection={0,0}) {
  ComPhysicalFourPort out;
  out.referenceOhm={50,50,50,50};
  for (size_t n=0;n<2;++n) {
    const size_t nearPort=n,farPort=n+2;
    out.scattering[4*farPort+nearPort]=gain;
    out.scattering[4*nearPort+farPort]=gain;
    out.scattering[4*nearPort+nearPort]=reflection;
    out.scattering[4*farPort+farPort]=reflection;
  }
  return out;
}
ComPhysicalChannelInput example() {
  ComPhysicalChannelInput x;
  x.frequencyHz={0,1e9,2e9,4e9,8e9,16e9};
  for(double f:x.frequencyHz) {
    const Complex transmission=0.8*std::exp(Complex{0,-2*
      std::numbers::pi*f*12e-12});
    x.boardS4p.push_back(wires(transmission));
  }
  x.transmitter.primaryLineSingleEndedOhm=50;
  x.transmitter.secondaryLineSingleEndedOhm=50;
  x.receiver.primaryLineSingleEndedOhm=50;
  x.receiver.secondaryLineSingleEndedOhm=50;
  x.transmitter.secondaryLineMm=1.8;
  x.receiver.secondaryLineMm=1.8;
  x.transmitter.delaySecondsPerMm=5e-12;
  x.receiver.delaySecondsPerMm=5e-12;
  x.transmitter.attenuationSqrtGHzPerMm=0.0007;
  x.receiver.attenuationSqrtGHzPerMm=0.0007;
  x.transmitter.attenuationGHzPerMm=0.0001;
  x.receiver.attenuationGHzPerMm=0.0001;
  return x;
}
}
int main() {
  try {
    const auto x=example();
    const auto result=calculateComPhysicalTestCases(x);
    require(result.usable() && result.status==Status::Warning,
            "physical core must be explicitly non-normative");
    require(result.diagnostics.front().code=="NOT_IEEE_COM",
            "no unjustified IEEE compliance claim");
    const auto& d=*result.value;
    require(d.referenceOhmDifferential==100,
            "50 ohm single-ended -> 100 ohm differential");
    require(d.test1.txPrimaryLineMm==13 && d.test1.rxPrimaryLineMm==11,
            "Test1 TX/RX line lengths");
    require(d.test2.txPrimaryLineMm==31 && d.test2.rxPrimaryLineMm==29,
            "Test2 TX/RX line lengths");
    require(d.test1.loadedSdd.size()==x.frequencyHz.size(),
            "complete band");
    const double pi=std::numbers::pi;
    for(size_t i=0;i<x.frequencyHz.size();++i) {
      const double f=x.frequencyHz[i],fGHz=f/1e9;
      const double alpha=0.0007*std::sqrt(fGHz)+0.0001*fGHz;
      const Complex gamma{alpha,2*pi*f*5e-12};
      const Complex board=0.8*std::exp(Complex{0,-2*pi*f*12e-12});
      for(const auto& testcase:{d.test1,d.test2}) {
        const double length=testcase.txPrimaryLineMm+
                            testcase.rxPrimaryLineMm+3.6;
        const Complex analyticallyExpected=board*std::exp(-gamma*length);
        const auto& s=testcase.loadedSdd[i];
        near(s.s21,analyticallyExpected,2e-12,
             "two case loaded Sdd21 vs independently derived exponent");
        near(s.s12,analyticallyExpected,2e-12,
             "two case reverse Sdd12 propagation");
        near(s.s11,{0,0},2e-12,
             "matched package/board must have zero Sdd11");
        near(s.s22,{0,0},2e-12,
             "matched package/board must have zero Sdd22");
      }
      if(f>0)
        require(std::abs(d.test2.loadedSdd[i].s21)<
                std::abs(d.test1.loadedSdd[i].s21),
                "longer Test2 package must lose more");
    }
    // Independent unmatched board model: no package loss or delay, so
    // the exact wave-star loaded S parameters equal the raw board.
    auto reflected=example();
    reflected.transmitter.attenuationSqrtGHzPerMm=0;
    reflected.receiver.attenuationSqrtGHzPerMm=0;
    reflected.transmitter.attenuationGHzPerMm=0;
    reflected.receiver.attenuationGHzPerMm=0;
    reflected.transmitter.delaySecondsPerMm=0;
    reflected.receiver.delaySecondsPerMm=0;
    for(auto& m:reflected.boardS4p)m=wires({.8,0},{.1,0});
    auto rb=calculateComPhysicalTestCases(reflected);
    for(auto& p:rb.value->test1.loadedSdd) {
      near(p.s21,{.8,0},2e-12,"unmatched Sdd21 preserved");
      near(p.s11,{.1,0},2e-12,"unmatched Sdd11 preserved");
      near(p.s22,{.1,0},2e-12,"unmatched Sdd22 preserved");
    }
    // Different positive/negative leg throughput causes Sdc mixed mode.
    auto converted=example();
    converted.boardS4p[2].scattering[2*4+0]={.7,0};
    converted.boardS4p[2].scattering[0*4+2]={.7,0};
    rejected([&]{(void)calculateComPhysicalTestCases(converted);},
             "do not silently ignore mode conversion");
    converted.maxAllowedModeConversion=.1;
    auto convertedResult=calculateComPhysicalTestCases(converted);
    require(convertedResult.usable() &&
            convertedResult.value->maxObservedModeConversion>.04,
            "report magnitude of permitted explicit mode conversion");
    auto bad=example();
    bad.boardS4p[1].referenceOhm[1]=75;
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "unequal source reference impedances must fail closed");
    bad=example();bad.boardS4p[2].referenceOhm={75,75,75,75};
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "frequency-varying physical reference impedances unsupported");
    bad=example();bad.boardS4p[1]=wires({1.2,0});
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "active physical channel must fail closed");
    bad=example();bad.boardS4p.pop_back();
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "missing source frequency sample must fail closed");
    bad=example();bad.frequencyHz[0]=.01;
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "DC omission must fail closed");
    bad=example();bad.transmitter.primaryLineMm=13;
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "override must be explicit, never silently discard length");
    bad=example();bad.receiver.cdFarad=-1e-15;
    rejected([&]{(void)calculateComPhysicalTestCases(bad);},
             "invalid package elements must fail closed");
    std::atomic_bool cancelled{true};
    bool threw=false;
    try{(void)calculateComPhysicalTestCases(x,{},&cancelled);}
    catch(const Cancelled&){threw=true;}
    require(threw,"propagate cancellation");
    std::cout<<"012_09 two Test1/Test2 S4P physical channel network "
                "analytic oracles PASS; normative IEEE COM NOT_EVALUATED\n";
    return 0;
  }catch(const std::exception& e) {
    std::cerr<<"012_09 physical test case pipeline FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
