#include "si/channel_analysis/s_network.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void require(bool yes,const char *msg) {
  if(!yes) throw std::runtime_error(msg);
}
void close(Complex actual,Complex expected,double tolerance,const char *msg) {
  if(!std::isfinite(actual.real()) ||
     !std::isfinite(actual.imag()) ||
     std::abs(actual-expected)>tolerance) throw std::runtime_error(msg);
}
template<class Fn>void blocked(Fn callback,const char *reason) {
  bool rejected=false;
  try{callback();}catch(const Error&){rejected=true;}
  require(rejected,reason);
}
ComPhysicalFourPort pairedWires(Complex top={1,0},Complex bottom={1,0}) {
  ComPhysicalFourPort p;
  p.referenceOhm={50,50,50,50};
  p.scattering[2*4+0]=top;
  p.scattering[0*4+2]=top;
  p.scattering[3*4+1]=bottom;
  p.scattering[1*4+3]=bottom;
  return p;
}
}
int main() {
  try{
    const auto identity=toComWave({},50);
    const auto series=toComWave(seriesImpedance({25,0}),50);
    const auto shunt=toComWave(shuntAdmittance({0.01,0}),50);
    const auto c1=cascadeComScattering(identity,series);
    close(c1.s.s11,series.s.s11,1e-14,"identity left S11");
    close(c1.s.s21,series.s.s21,1e-14,"identity left S21");
    const auto c2=cascadeComScattering(series,identity);
    close(c2.s.s11,series.s.s11,1e-14,"identity right S11");
    close(c2.s.s21,series.s.s21,1e-14,"identity right S21");
    const auto cascaded=cascadeComScattering(series,shunt);
    const auto analytic=twoPortScattering(
        cascadeTwoPort(seriesImpedance({25,0}),
                           shuntAdmittance({0.01,0})),50);
    close(cascaded.s.s11,analytic.s11,1e-13,"Redheffer vs ABCD S11");
    close(cascaded.s.s21,analytic.s21,1e-13,"Redheffer vs ABCD S21");
    close(cascaded.s.s12,analytic.s12,1e-13,"Redheffer vs ABCD S12");
    close(cascaded.s.s22,analytic.s22,1e-13,"Redheffer vs ABCD S22");
    // Extreme attenuation should not overflow or invent reverse gain.
    const auto heavy=toComWave(
       lossyUniformLine(0,1,100,0,50),50);
    const auto heavy2=cascadeComScattering(heavy,heavy);
    const double expectedLoss=std::exp(-200.);
    require(std::abs(heavy2.s.s21.real()/expectedLoss-1)<1e-11,
            "200 Np transfer relative analytical oracle");
    close(heavy2.s.s12,heavy2.s.s21,0,"200 Np reciprocity");
    const auto physical=pairedWires();
    const auto mixed=extractDifferentialComNetwork(physical);
    require(mixed.differential.referenceOhm==100,
            "physical 50ohm -> differential 100ohm");
    close(mixed.differential.s.s21,{1,0},1e-14,"ideal 4-port diff thru");
    close(mixed.differential.s.s12,{1,0},1e-14,"ideal diff reverse");
    close(mixed.differential.s.s11,{0,0},1e-14,"ideal diff return");
    require(mixed.maximumModeConversion<1e-14,"ideal balanced has no conversion");
    // Phase is exactly preserved in complex mixed-mode conversion.
    auto rotated=pairedWires({0,1},{0,1});
    const auto rotatedDiff=extractDifferentialComNetwork(rotated);
    close(rotatedDiff.differential.s.s21,{0,1},1e-14,"diff phase");
    auto asymmetric=pairedWires({0.8,0},{1,0});
    blocked([&]{(void)extractDifferentialComNetwork(asymmetric,0.01);},
            "reject 10% mode conversion without explicit support");
    auto allowed=extractDifferentialComNetwork(asymmetric,0.11);
    close(allowed.differential.s.s21,{0.9,0},1e-14,
          "balanced decomposition difference mode average");
    require(std::abs(allowed.maximumModeConversion-0.1)<1e-13,
            "report exact maximum mode conversion");
    auto badRef=physical;
    badRef.referenceOhm[0]=75;
    blocked([&]{(void)extractDifferentialComNetwork(badRef);},
            "mixed physical reference must be equal");
    auto badAct=physical;
    badAct.scattering[2*4+0]={1.2,0};
    blocked([&]{(void)extractDifferentialComNetwork(badAct);},
            "reject active complete four-port");
    auto badNan=physical;
    badNan.scattering[2*4+0]={NAN,0};
    blocked([&]{(void)extractDifferentialComNetwork(badNan);},
            "reject NaN full physical matrix");
    blocked([&]{(void)extractDifferentialComNetwork(physical,-0.01);},
            "negative mode-conversion limit");
    auto active=series;
    active.s.s21={2.0,0};
    blocked([&]{(void)cascadeComScattering(active,identity);},
            "reject nonpassive two-port");
    auto ref75=toComWave({},75);
    blocked([&]{(void)cascadeComScattering(series,ref75);},
            "mismatched wave Zref");
    ComWaveTwoPort mirror;
    mirror.referenceOhm=50;
    mirror.s.s11={1,0};mirror.s.s22={1,0};
    mirror.s.s21={0,0};mirror.s.s12={0,0};
    blocked([&]{(void)cascadeComScattering(mirror,mirror);},
            "singular internal reflection loop");
    std::cout<<"012_09 four-port mixed-mode and passive Redheffer cascade "
              "analytical PASS; IEEE COM NOT QUALIFIED\n";
    return 0;
  }catch(const std::exception &e) {
    std::cerr<<"012_09 physical network FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
