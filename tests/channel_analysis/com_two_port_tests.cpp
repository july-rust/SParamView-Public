#include "si/channel_analysis/two_port.hpp"
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace si;
using namespace si::channel;
namespace {
void check(bool ok, const char *label) {
  if (!ok) throw std::runtime_error(label);
}
void near(Complex a, Complex b, double tol, const char *label) {
  if (!std::isfinite(a.real()) || !std::isfinite(a.imag()) ||
      std::abs(a-b)>tol) throw std::runtime_error(label);
}
template<class F> void rejects(F fn, const char *label) {
  bool rejected=false;
  try {fn();} catch(const Error &) {rejected=true;}
  check(rejected,label);
}
}
int main() {
  try {
    const double pi=std::numbers::pi;
    const auto identity=twoPortScattering({},50);
    near(identity.s11,{0,0},1e-14,"identity no reflection");
    near(identity.s21,{1,0},1e-14,"identity no attenuation");
    near(identity.s12,{1,0},1e-14,"identity bidirectional");
    const double z=50;
    const auto series=twoPortScattering(seriesImpedance({25,0}),z);
    near(series.s21,{0.8,0},1e-14,"series analytic S21 2Z/(2Z+R)");
    near(series.s11,{0.2,0},1e-14,"series analytic reflection R/(2Z+R)");
    near(series.s12,series.s21,1e-14,"reciprocal series");
    const auto shunt=twoPortScattering(shuntAdmittance({0.01,0}),z);
    near(shunt.s21,{0.8,0},1e-14,"shunt analytic S21 2/(2+YZ)");
    near(shunt.s11,{-0.2,0},1e-14,"shunt analytic reflection -YZ/(2+YZ)");
    const double hz=1e9, length=10, delay=6e-12, loss=0.01;
    const auto tl=lossyUniformLine(hz,length,loss,delay,z);
    const auto pass=twoPortScattering(tl,z);
    const Complex expected=std::exp(Complex{-loss*length,
                                            -2*pi*hz*length*delay});
    near(pass.s21,expected,1e-13,"lossy matched line analytic propagation");
    near(pass.s11,{0,0},1e-13,"matched line no reflection");
    near(pass.s12,pass.s21,1e-13,"reciprocal line");
    const auto doubleTl=twoPortScattering(cascadeTwoPort(tl,tl),z);
    near(doubleTl.s21,expected*expected,1e-13,"line twice cascaded");
    near(doubleTl.s11,{0,0},1e-13,"cascaded line no reflection");
    const auto noLoss=twoPortScattering(
      lossyUniformLine(hz,length,0,delay,75),z);
    check(std::abs(std::norm(noLoss.s11)+std::norm(noLoss.s21)-1.)<1e-12,
          "lossless mismatched line conserves wave power");
    check(std::abs(noLoss.s11)>1e-4,"mismatch reflection retained");
    const auto left=seriesImpedance({10,0});
    const auto right=shuntAdmittance({0.005,0});
    const auto ordered=twoPortScattering(cascadeTwoPort(left,right),z);
    const auto reversed=twoPortScattering(cascadeTwoPort(right,left),z);
    near(ordered.s21,reversed.s21,1e-14,"reciprocal cascade S21");
    check(std::abs(ordered.s11-reversed.s11)>0.001,
          "ordered network preserves directional reflection difference");
    rejects([&]{(void)seriesImpedance({-1,0});},"active negative R");
    rejects([&]{(void)shuntAdmittance({-1,0});},"active negative G");
    rejects([&]{(void)lossyUniformLine(-1,length,loss,delay,z);},"negative frequency");
    rejects([&]{(void)lossyUniformLine(hz,-1,loss,delay,z);},"negative length");
    rejects([&]{(void)lossyUniformLine(hz,length,-1,delay,z);},"negative loss");
    rejects([&]{(void)lossyUniformLine(hz,length,loss,delay,0);},"zero Zc");
    rejects([&]{(void)lossyUniformLine(hz,length,loss,-1,z);},"negative delay");
    rejects([&]{(void)lossyUniformLine(hz,40000,loss,delay,z);},"exponential overflow");
    rejects([&]{(void)twoPortScattering({},0);},"invalid ref Z");
    ComTwoPort corrupted;
    corrupted.a={NAN,0};
    rejects([&]{(void)twoPortScattering(corrupted,50);},"invalid ABCD");
    std::cout<<"012_09 independent reciprocal two-port analytic golden PASS; "
                "NOT IEEE COM validation\n";
    return 0;
  } catch(const std::exception &e) {
    std::cerr<<"012_09 two-port analytic test FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
