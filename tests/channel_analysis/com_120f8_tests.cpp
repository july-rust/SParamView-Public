#include "si/channel_analysis/com_120f8.hpp"
#include "si/channel_analysis/com_causal_package.hpp"
#include "si/channel_analysis/package.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace si::channel;
namespace {
void demand(bool ok,const char* reason) {
  if(!ok)throw std::runtime_error(reason);
}
void close(double a,double b,const char* why) {
  demand(std::isfinite(a) && std::abs(a-b)<=1e-10*std::max(1e-18,std::abs(b)),why);
}
void closeComplex(si::Complex actual,si::Complex expected,const char* why) {
  demand(std::isfinite(actual.real()) && std::isfinite(actual.imag()) &&
         std::abs(actual-expected)<=2e-12*
             std::max(1.,std::abs(expected)),why);
}
template<class F> void reject(F call,const char* why) {
  bool caught=false;
  try{call();}catch(const si::Error&){caught=true;}
  demand(caught,why);
}
}
int main() {
try {
  const auto p=final8023ck120F8();
  close(p.symbolRateHz,53.125e9,"53.125 GBd");
  close(p.maximumStartFrequencyHz,50e6,"start <=50 MHz");
  close(p.maximumFrequencyStepHz,10e6,"step <=10 MHz");
  close(p.minimumComDb,3.,"minimum COM");
  close(p.txTransitionTimeS,7.5e-12,"Ht transition");
  close(p.txPackageLengthMm[0],13.,"Test1 TX package");
  close(p.rxPackageLengthMm[0],11.,"Test1 RX package");
  close(p.txPackageLengthMm[1],31.,"Test2 TX package");
  close(p.rxPackageLengthMm[1],29.,"Test2 RX package");
  close(p.cdFaradSingleEnded,1.2e-13,"Cd SI conversion");
  close(p.cbFaradSingleEnded,3e-14,"Cb SI conversion");
  close(p.cpFaradSingleEnded,8.7e-14,"Cp SI conversion");
  close(p.lsHenrySingleEnded,1.2e-10,"Ls SI conversion");
  close(p.primaryPackageZcOhm,87.5,"package Zc");
  close(p.secondaryPackageZcOhm,92.5,"package Zc2");
  close(p.packageA1SqrtNsPerMm,9.909e-4,"a1");
  close(p.packageA2NsPerMm,2.772e-4,"a2");
  close(p.receiverBandwidthHz,39.84375e9,"receiver 3-dB BW");
  close(p.txNoncursor[0].minimum,-0.04,"FINAL c(-3): draft -0.06 forbidden");
  close(p.txNoncursor[1].maximum,0.10,"FINAL c(-2)");
  close(p.txNoncursor[2].minimum,-0.28,"FINAL c(-1)");
  close(p.txNoncursor[3].minimum,-0.10,"FINAL c(+1)");
  close(p.minimumCursor,0.54,"FINAL minimum c0");
  close(p.ctleDcGainDb.minimum,-20.,"CTLE gDC");
  close(p.ctleDcGain2Db.minimum,-4.,"CTLE gDC2");
  close(p.ctleZeroFrequencyHz,21.25e9,"CTLE fz");
  close(p.ctlePole1FrequencyHz,21.25e9,"CTLE fp1");
  close(p.ctlePole2FrequencyHz,53.125e9,"CTLE fp2");
  close(p.ctleLowFrequencyPoleZeroHz,664.0625e6,"CTLE fLF");
  close(p.victimPeakV,0.413,"Victim amplitude");
  close(p.fextPeakV,0.413,"FEXT amplitude");
  close(p.nextPeakV,0.608,"NEXT amplitude");
  close(p.minimumRlm,0.95,"RLM");
  close(p.txSnrDb,33.,"SNRTX");
  demand(p.pamLevels==4 && p.samplesPerUi==32 && p.dfeTaps==6,
         "PAM4/UI/DFE integer contract");
  const double dfeMin[6]={0.3,0.05,-0.04,-0.04,-0.04,-0.04};
  const double dfeMax[6]={0.65,0.15,0.1,0.1,0.1,0.1};
  for(int i=0;i<6;++i) {
    close(p.dfeMin[i],dfeMin[i],"DFE min");
    close(p.dfeMax[i],dfeMax[i],"DFE max");
  }
  close(p.randomJitterRmsUi,0.01,"random jitter");
  close(p.dualDiracJitterPeakUi,0.02,"dual Dirac jitter");
  close(p.oneSidedNoiseSpectralDensityV2PerGHz,2e-8,"noise PSD");
  close(p.targetDetectorErrorRatio,1e-5,"DER");
  const auto historical1=com8023ckC2cPackageLengths(ComPackageCase::Test1);
  const auto historical2=com8023ckC2cPackageLengths(ComPackageCase::Test2);
  close(historical1.txMm,p.txPackageLengthMm[0],"physical Test1 TX drift");
  close(historical1.rxMm,p.rxPackageLengthMm[0],"physical Test1 RX drift");
  close(historical2.txMm,p.txPackageLengthMm[1],"physical Test2 TX drift");
  close(historical2.rxMm,p.rxPackageLengthMm[1],"physical Test2 RX drift");
  const auto physical=historical8023ckC2cCausalCandidate(13.);
  close(physical.cdSingleEndedFarad,p.cdFaradSingleEnded,"package Cd drift");
  close(physical.lsSingleEndedHenry,p.lsHenrySingleEnded,"package Ls drift");
  close(physical.cbSingleEndedFarad,p.cbFaradSingleEnded,"package Cb drift");
  close(physical.cpSingleEndedFarad,p.cpFaradSingleEnded,"package Cp drift");
  close(physical.a1SqrtNsPerMm,p.packageA1SqrtNsPerMm,"package a1 drift");
  close(physical.a2NsPerMm,p.packageA2NsPerMm,"package a2 drift");
  close(physical.primaryZcDifferentialOhm,p.primaryPackageZcOhm,
        "package Zc drift");
  close(physical.secondaryZcDifferentialOhm,p.secondaryPackageZcOhm,
        "package Zc2 drift");

  // Fixed independent complex oracle values from high-precision evaluation
  // of IEEE 93A CTLE and the 2019 IEEE-hosted fourth-order receiver filter.
  // These are component-level, never full numerical COM golden values.
  closeComplex(com120F8Ctle(0.,-20.,-4.),
               {0.06309573444801933,0.},"CTLE DC -24dB gain");
  closeComplex(com120F8Ctle(10e9,-20.,-4.),
               {0.3096853152707358,0.29430788329414326},
               "CTLE phase and amplitude at 10 GHz");
  closeComplex(com120F8Ctle(21.25e9,-11.,-2.),
               {0.6756332283565186,0.09287083460121599},
               "CTLE complex oracle at 21.25 GHz");
  closeComplex(com120F8ReceiverNoiseFilter(0.),{1.,0.},
               "fourth-order Butterworth DC unity");
  closeComplex(com120F8ReceiverNoiseFilter(39.84375e9),
               {-0.7071067811865476,0.},
               "Butterworth 3 dB receiver corner and phase");
  closeComplex(com120F8ReceiverTransfer(10e9,-20.,-4.),
               {0.42516619243164677,0.041868363616010984},
               "end-to-end complex CTLE and noise-filter product");
  closeComplex(com120F8ReceiverTransfer(26.5625e9,0.,0.),
               {-0.6188991934903375,-0.622029941071934},
               "Nyquist 26.5625 GHz phase-preserving complex filter");
  reject([&]{(void)com120F8Ctle(1e9,-20.5,0.);},
         "off-grid CTLE gain rejected by C++ complex solver");
  reject([&]{(void)com120F8ReceiverNoiseFilter(
      std::numeric_limits<double>::infinity());},
      "infinite-frequency Butterworth must fail closed");
  validateCom120F8Equalizer({});
  Com120F8EqualizerChoice eq;
  eq.txFfe={-0.04,0.10,-0.28,0.54,-0.10};
  eq.ctleDcGainDb=-20.;
  eq.ctleDcGain2Db=-4.;
  validateCom120F8Equalizer(eq);
  eq.txFfe[0]=-0.06;
  reject([&]{validateCom120F8Equalizer(eq);},
         "2020 draft c(-3) must not pass 2022 final");
  eq.txFfe[0]=-0.03;
  reject([&]{validateCom120F8Equalizer(eq);},
         "invalid FFE quantization must fail");
  eq.txFfe[0]=-0.04;
  eq.txFfe[3]=0.53;
  reject([&]{validateCom120F8Equalizer(eq);},
         "cursor minimum fail-closed");
  eq.txFfe[3]=0.54;
  eq.ctleDcGainDb=-20.5;
  reject([&]{validateCom120F8Equalizer(eq);},
         "invalid CTLE 1dB gain quantization");
  eq.ctleDcGainDb=-20.;
  eq.ctleDcGain2Db=std::numeric_limits<double>::quiet_NaN();
  reject([&]{validateCom120F8Equalizer(eq);},
         "CTLE NaN fail-closed");

  std::vector<double> grid;
  grid.reserve(2658);
  for(int i=0;i<=2657;++i)grid.push_back(1e7*i);
  validateCom120F8DirectGrid(grid);
  auto badGrid=grid;
  badGrid[1]=50e6;
  reject([&]{validateCom120F8DirectGrid(badGrid);},
         "sparse first interval must fail");
  badGrid=grid;
  badGrid[2]=std::numeric_limits<double>::quiet_NaN();
  reject([&]{validateCom120F8DirectGrid(badGrid);},"NaN grid must fail");
  badGrid=grid;
  badGrid.pop_back();
  for(int i=0;i<2;++i)badGrid.pop_back();
  reject([&]{validateCom120F8DirectGrid(badGrid);},
         "below Nyquist must fail");
  std::cout<<"012_09 published-final 120F-8 numeric contract, TX/CTLE "
              "bounds, source package drift and strict-grid tests PASS; "
              "normative COM solver still NOT_EVALUATED\n";
  return 0;
}catch(const std::exception& error) {
  std::cerr<<"012_09 final 120F-8 parameter contract FAIL: "
           <<error.what()<<"\n";
  return 1;
}
}
