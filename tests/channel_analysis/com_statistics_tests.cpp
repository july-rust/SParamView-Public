#include "si/channel_analysis/com_statistics.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace si;
using namespace si::channel;
namespace {
void require(bool ok, const char *reason) {
  if(!ok)throw std::runtime_error(reason);
}
void close(double got, double want, double tol, const char *reason) {
  if(!std::isfinite(got) || std::abs(got-want)>tol)
    throw std::runtime_error(reason);
}
template<class F> void reject(F f, const char *msg) {
  bool caught=false;
  try {f();}catch(const Error &){caught=true;}
  require(caught,msg);
}
double tail(double q,double mean,double sigma) {
  return 0.5 * std::erfc((q-mean)/(sigma*std::sqrt(2.)));
}
}
int main() {
  try{
    ComStatisticalNoiseInput in;
    in.gaussianRmsV=0.020;
    in.oneSidedTailProbability=1e-5;
    const auto pure=calculatePam4StatisticalNoise(in);
    require(pure.usable() && pure.status==Status::Warning,
            "noise math must be explicitly exploratory");
    require(pure.value->exactFiniteState &&
            pure.value->statesEvaluated==1,"Gaussian-only state");
    close(pure.value->oneSidedNoiseQuantileV/0.020,
          4.264890793922825,1e-10,
          "Independent analytical standard normal inverse survival 1e-5");
    close(pure.value->analyticTotalVarianceV2,0.020*0.020,
          1e-15,"Gaussian variance");
    // Adversarial very small but physically finite voltage units must not
    // change the dimensionless Gaussian quantile or mis-evaluate tiny p.
    ComStatisticalNoiseInput nano;
    nano.gaussianRmsV=1e-10;
    nano.oneSidedTailProbability=1e-5;
    const auto scaled=calculatePam4StatisticalNoise(nano);
    require(scaled.usable(),"1e-10 V Gaussian input must remain usable");
    close(scaled.value->oneSidedNoiseQuantileV/nano.gaussianRmsV,
          4.264890793922825,1e-10,
          "nanovolt Gaussian scale-independent quantile");
    nano.gaussianRmsV=1e-15;
    nano.oneSidedTailProbability=1e-12;
    const auto pico=calculatePam4StatisticalNoise(nano);
    require(pico.usable(),"very small 1e-15 V Gaussian input must work");
    require(pico.value->oneSidedNoiseQuantileV/nano.gaussianRmsV>7.0,
            "1e-12 tail requires >7 Gaussian standard deviations");
    require(std::abs(pico.value->verifiedTailProbability-
                     nano.oneSidedTailProbability)/
                     nano.oneSidedTailProbability < 1e-6,
            "tiny target p must be verified to relative probability");
    // Discrete tails must also preserve quantiles under voltage scaling.
    nano.oneSidedTailProbability=1e-8;
    nano.pam4TapV={1.5e-15,0.7e-15};
    const auto microscopic=calculatePam4StatisticalNoise(nano);
    ComStatisticalNoiseInput unitScaled;
    unitScaled.gaussianRmsV=0.01;
    unitScaled.oneSidedTailProbability=1e-8;
    unitScaled.pam4TapV={0.015,0.007};
    const auto macroscopic=calculatePam4StatisticalNoise(unitScaled);
    close(microscopic.value->oneSidedNoiseQuantileV/nano.gaussianRmsV,
          macroscopic.value->oneSidedNoiseQuantileV/
              unitScaled.gaussianRmsV,1e-10,
          "sub-femtovolt PAM4 mixture must preserve normalized quantile");

    require(pure.diagnostics[0].code=="NOT_IEEE_COM",
            "must not claim IEEE COM");
    // Independent oracle: a four-component equally weighted Gaussian
    // mixture has tail = sum_a Q((q-a*h)/sigma)/4.
    in.pam4TapV={0.03};
    const auto one=calculatePam4StatisticalNoise(in);
    require(one.value->statesEvaluated==4,"one PAM4 tap = four states");
    const double q=one.value->oneSidedNoiseQuantileV;
    const double expected=(tail(q,-.03,.02)+tail(q,-.01,.02)+
                           tail(q,.01,.02)+tail(q,.03,.02))/4.;
    close(expected,1e-5,1e-11,"independent four-Gaussian mixture CDF");
    close(one.value->analyticTotalVarianceV2,
          0.020*0.020+(.03*.03)*5./9.,1e-15,
          "PAM4 exact variance E[level^2]=5/9");
    // Two independent PAM4 taps -> 16 discrete states.
    in.pam4TapV={.03,.01};
    const auto two=calculatePam4StatisticalNoise(in);
    require(two.value->statesEvaluated==16,"two PAM4 taps");
    require(two.value->oneSidedNoiseQuantileV>q,
            "added discrete noise increases extreme tail quantile");
    // PAM4 distribution is symmetric: flipping signs or reordering
    // independent tap weights must not affect the noise quantile.
    in.pam4TapV={-.03,-.01};
    const auto neg=calculatePam4StatisticalNoise(in);
    close(neg.value->oneSidedNoiseQuantileV,
          two.value->oneSidedNoiseQuantileV,1e-13,
          "tap sign invariance");
    in.pam4TapV={.01,.03};
    const auto reordered=calculatePam4StatisticalNoise(in);
    close(reordered.value->oneSidedNoiseQuantileV,
          two.value->oneSidedNoiseQuantileV,1e-13,
          "independent tap permutation invariance");
    in.gaussianRmsV=.04;
    in.pam4TapV={.02,.06};
    const auto doubleSize=calculatePam4StatisticalNoise(in);
    close(doubleSize.value->oneSidedNoiseQuantileV,
          2*two.value->oneSidedNoiseQuantileV,1e-13,
          "amplitude scale invariance");
    in.gaussianRmsV=.02;
    in.pam4TapV={.03,.01};
    in.oneSidedTailProbability=1e-8;
    const auto stricter=calculatePam4StatisticalNoise(in);
    require(stricter.value->oneSidedNoiseQuantileV>
            two.value->oneSidedNoiseQuantileV,
            "lower target BER increases noise quantile");
    in.oneSidedTailProbability=1e-5;
    in.pam4TapV.assign(8,.003);
    const auto maxStates=calculatePam4StatisticalNoise(in);
    require(maxStates.value->statesEvaluated==65536 &&
            maxStates.value->exactFiniteState,
            "max model exactly 4^8 deterministic states");
    auto bad=in;
    bad.pam4TapV.assign(9,.003);
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "nine taps exceed exact model bound");
    bad=in;bad.gaussianRmsV=0;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "zero RMS unsupported");
    bad=in;bad.gaussianRmsV=NAN;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "missing RMS unsupported");
    bad=in;bad.oneSidedTailProbability=0;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "zero error target unsupported");
    bad=in;bad.oneSidedTailProbability=0.5;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "unbounded tail unsupported");
    bad=in;bad.pam4TapV[0]=INFINITY;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "nonfinite tap unsupported");
    bad=in;bad.pam4TapV[0]=1e308;
    reject([&]{(void)calculatePam4StatisticalNoise(bad);},
           "noise/ISI overflow fail closed");
    std::atomic_bool cancelled{true};
    bool caught=false;
    try{(void)calculatePam4StatisticalNoise(in,{},&cancelled);}
    catch(const Cancelled&){caught=true;}
    require(caught,"cancelled statistical job must propagate");
    std::cout<<"012_09 exact finite PAM4/Gaussian mixture analytical PASS; "
                "NOT standard COM\n";
    return 0;
  }catch(const std::exception &e) {
    std::cerr<<"012_09 COM probability kernel FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
