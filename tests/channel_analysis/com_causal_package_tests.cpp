#include "si/channel_analysis/com_causal_package.hpp"
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok,const char *msg) {
  if(!ok)throw std::runtime_error(msg);
}
void near(Complex actual,Complex expected,double tol,const char *msg) {
  if(!std::isfinite(actual.real()) || !std::isfinite(actual.imag()) ||
     std::abs(actual-expected)>tol)
    throw std::runtime_error(msg);
}
template<class F>void reject(F fn,const char *msg) {
  bool did=false;
  try {fn();}catch(const Error&){did=true;}
  demand(did,msg);
}
double pi() {return std::numbers::pi;}
Complex independentGamma(double fGHz,double gamma0,double a1,
                         double a2,double tau) {
  if(fGHz==0)return {gamma0,0};
  // Independently derive the real and imaginary coefficients directly from
  // the published causal-by-design package model, not from implementation.
  return {gamma0+a1*std::sqrt(fGHz)+a2*fGHz,
          a1*std::sqrt(fGHz)+2*pi()*tau*fGHz-
              2*a2/pi()*fGHz*std::log(fGHz)};
}
ComTwoPortS oracleViaABCD(Complex gammaLength,
                          double zCharacteristic,double zReference) {
  const auto a=std::cosh(gammaLength);
  const auto b=zCharacteristic*std::sinh(gammaLength);
  const auto c=std::sinh(gammaLength)/zCharacteristic;
  const auto denominator=a+b/zReference+c*zReference+a;
  ComTwoPortS result;
  result.s21=result.s12=Complex{2,0}/denominator;
  result.s11=(b/zReference-c*zReference)/denominator;
  result.s22=result.s11;
  return result;
}
}
int main() {
  try {
    auto p=historical8023ckC2cCausalCandidate(13);
    demand(p.cdSingleEndedFarad==1.2e-13 &&
           p.lsSingleEndedHenry==1.2e-10 &&
           p.cbSingleEndedFarad==3e-14 &&
           p.cpSingleEndedFarad==8.7e-14,
           "C2C historical candidate single-ended C/L unit conversion");
    demand(p.primaryZcDifferentialOhm==87.5 &&
           p.secondaryZcDifferentialOhm==92.5 &&
           p.secondaryLengthMm==1.8,
           "C2C package impedance is differential Zc; do not double it");
    demand(p.tauNsPerMm==0.006141,"published package delay");
    const double a1=9.909e-4,a2=2.772e-4,tau=0.006141;
    for(double fGHz:{0.,0.1,1.,2.,10.,26.5625,53.125}) {
      const Complex oracle=independentGamma(fGHz,0,a1,a2,tau);
      const Complex actual=comCausalGammaPerMm(p,fGHz*1e9);
      near(actual,oracle,2e-15,
           "causal gamma must include sqrt dispersion, log dispersion, delay");
      const double l=8.7;
      auto matched=p;
      matched.primaryZcDifferentialOhm=100;
      matched.secondaryZcDifferentialOhm=100;
      const auto line=comCausalLine(matched,fGHz*1e9,l,100,100);
      near(line.s.s21,std::exp(-oracle*l),5e-14,
           "matched causal line exact attenuation and complex phase");
      near(line.s.s11,{0,0},2e-15,
           "matched causal line no spurious reflection");
      const auto mixed=comCausalLine(p,fGHz*1e9,l,87.5,100);
      const auto reference=oracleViaABCD(oracle*l,87.5,100);
      near(mixed.s.s21,reference.s21,2e-13,
           "causal line mismatched 87.5/100 independent ABCD oracle");
      near(mixed.s.s11,reference.s11,2e-13,
           "causal line reflection independent ABCD oracle");
      near(mixed.s.s12,mixed.s.s21,1e-15,"reciprocity");
      near(mixed.s.s22,mixed.s.s11,1e-15,"reflection symmetry");
    }
    // Regression for the old incomplete alpha+j omega*tau implementation:
    // at exactly 1 GHz, the a1 sqrt(f) dispersion adds +a1 rad/mm;
    // 10 GHz also requires the -a2*(2/pi)*f*log(f_GHz) term.
    const double oldPhase=2*pi()*tau*1.;
    const auto f1=comCausalGammaPerMm(p,1e9);
    demand(std::abs(f1.imag()-oldPhase-a1)<1e-14,
           "a1 causal phase dispersion cannot be silently dropped");
    const auto f10=comCausalGammaPerMm(p,10e9);
    demand(std::abs(f10.imag()-
        (a1*std::sqrt(10.)+10*(2*pi()*tau-
         2*a2/pi()*std::log(10.))))<1e-14,
         "a2 logarithmic dispersion required");
    // Causal package Zc from the published example is already differential
    // (87.5 and 92.5 ohms); the old implementation doubled it incorrectly.
    auto pure=p;
    pure.cdSingleEndedFarad=0; pure.lsSingleEndedHenry=0;
    pure.cbSingleEndedFarad=0; pure.cpSingleEndedFarad=0;
    pure.secondaryLengthMm=0;
    const auto pkg=calculateComCausalPackage(
        pure,1e9,ComCausalPackageSide::TxDieToBoard,50);
    const auto singleLine=comCausalLine(p,1e9,13,87.5,100);
    near(pkg.s.s21,singleLine.s.s21,2e-12,
         "C2C package Zc must remain differential 87.5 ohm");
    near(pkg.s.s11,singleLine.s.s11,2e-12,
         "C2C package differential Zc determines correct reflection sign");
    auto tx=calculateComCausalPackage(
        p,10e9,ComCausalPackageSide::TxDieToBoard);
    auto rx=calculateComCausalPackage(
        p,10e9,ComCausalPackageSide::RxBoardToDie);
    near(tx.s.s21,rx.s.s21,2e-12,
         "reverse package order preserves reciprocal transfer");
    near(tx.s.s11,rx.s.s22,2e-12,
         "TX/RX reversal swaps near/far reflection");
    near(tx.s.s22,rx.s.s11,2e-12,
         "TX/RX reversal swaps far/near reflection");
    const auto test2=historical8023ckC2cCausalCandidate(31);
    const auto t2=calculateComCausalPackage(
        test2,10e9,ComCausalPackageSide::TxDieToBoard);
    demand(std::abs(t2.s.s21)<std::abs(tx.s.s21),
           "longer Test2 physical package must exhibit more loss");
    // Numeric high-loss passivity/rounding: use stable exp(gamma), not cosh.
    auto veryLossy=p;veryLossy.gamma0PerMm=30;
    const auto huge=comCausalLine(veryLossy,0,10,87.5,100);
    demand(std::abs(huge.s.s21)<1e-129,
           "high attenuation preserves finite passive scattering");
    demand(std::isfinite(huge.s.s11.real()),
           "high attenuation reflection finite");
    // Negative, NaN, Inf, invalid impedance and illegal side fail closed.
    auto bad=p;bad.a1SqrtNsPerMm=-0.01;
    reject([&]{(void)comCausalGammaPerMm(bad,1e9);},
           "negative square-root loss reject");
    bad=p;bad.a2NsPerMm=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)comCausalGammaPerMm(bad,1e9);},
           "missing phase coefficient reject");
    bad=p;bad.tauNsPerMm=std::numeric_limits<double>::infinity();
    reject([&]{(void)calculateComCausalPackage(
        bad,1e9,ComCausalPackageSide::TxDieToBoard);},
        "infinite package delay reject");
    reject([&]{(void)comCausalLine(p,1e9,13,175,0);},
           "reference impedance 0 reject");
    reject([&]{(void)comCausalLine(p,-1,13,87.5,100);},
           "negative frequency reject");
    reject([&]{(void)calculateComCausalPackage(
        p,1e9,static_cast<ComCausalPackageSide>(3));},
        "invalid causal package side reject");
    std::cout<<"012_09 causal package gamma, differential Zc and "
                "analytical S oracle PASS; COM dB still NOT_EVALUATED\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"012_09 causal package TEST FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
