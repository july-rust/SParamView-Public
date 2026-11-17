#include "si/channel_analysis/com_120f8.hpp"
#include "si/channel_analysis/com_causal_package.hpp"
#include "si/channel_analysis/com_causal_channel.hpp"
#include "si/channel_analysis/package.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <atomic>
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
  eq.txFfe={-0.04,0.10,-0.20,0.56,-0.10};
  eq.ctleDcGainDb=-20.;
  eq.ctleDcGain2Db=-4.;
  validateCom120F8Equalizer(eq);
  eq.txFfe[2]=-0.28;
  reject([&]{validateCom120F8Equalizer(eq);},
         "invalid FFE sum cannot independently set c0=0.56");
  eq.txFfe[2]=-0.20;
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
  eq.txFfe[3]=0.56;
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
  // Real native C++ execution: 120F-8 fixed FFE -> complex 93A CTLE
  // -> Butterworth RX. This is an EXPLORATORY transfer, not IEEE COM dB.
  ComInput channel;
  channel.thru.parameter="Sdd21";
  channel.thru.stimulusOhm=channel.thru.responseOhm=100.;
  channel.thru.frequencyHz=grid;
  channel.thru.transfer.assign(grid.size(),{1.,0.});
  Com120F8EqualizerChoice flatEq;
  flatEq.ctleDcGainDb=-20.;
  flatEq.ctleDcGain2Db=-4.;
  const auto calculated=prepareCom120F8ReceiverSpectra(channel,flatEq);
  demand(calculated.usable() &&
         calculated.status==Status::Warning &&
         calculated.value.has_value(),
         "actual C++ FFE/CTLE/RX diagnostic must run and warn");
  demand(calculated.provenance.algorithm==
         "com-120f8-ffe-ctle-rx-exploratory",
         "transfer must never be disguised as normative margin");
  closeComplex(calculated.value->conditionedThru[0],
               {0.06309573444801933,0.},
               "actual native source->FFE->receiver DC oracle");
  // Cursor-only FFE MUST be 1+0j even at 10 GHz, not 3 UI late.
  closeComplex(calculated.value->txFfeResponse[1000],{1.,0.},
               "cursor-relative FFE removes artificial 3 UI delay");
  closeComplex(calculated.value->conditionedThru[1000],
               {0.42516619243164677,0.041868363616010984},
               "actual native cursor-aligned source->FFE->receiver oracle");
  // A signed pre/post FFE with cursor-relative advance/delay must not
  // accidentally rotate THRU or a complex FEXT aggressor by 3 UI.
  Com120F8EqualizerChoice multiEq=flatEq;
  multiEq.txFfe={-0.04,0.10,-0.20,0.56,-0.10};
  ComAggressor aggressor{"fext-golden",ComCoupling::Fext,channel.thru};
  aggressor.transfer.parameter="Sdd41";
  aggressor.transfer.transfer.assign(grid.size(),{0.,0.1});
  channel.aggressors.push_back(aggressor);
  const auto multi=prepareCom120F8ReceiverSpectra(channel,multiEq);
  demand(multi.usable() && multi.value->aggressors.size()==1,
         "full signed FIR and FEXT phase calculation");
  closeComplex(multi.value->txFfeResponse[1000],
               {0.41185514897596864,-0.006691413944004826},
               "cursor-aligned pre/post TX FFE independent oracle");
  closeComplex(multi.value->conditionedThru[1000],
               {0.17538704407559405,0.014398738145895667},
               "signed FFE and RX complex THRU independent oracle");
  closeComplex(multi.value->aggressors[0].conditionedTransfer[1000],
               {-0.0014398738145895667,0.017538704407559405},
               "signed FFE and RX complex FEXT independent oracle");
  channel.rxFilter.parameter="Sdd21";
  channel.rxFilter.frequencyHz=grid;
  channel.rxFilter.transfer.assign(grid.size(),{1.,0.});
  reject([&]{(void)prepareCom120F8ReceiverSpectra(channel,flatEq);},
         "external RX filter override must fail closed");
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

  // 012_09_03: independent analytic pure-delay Sdd21 = exp(-j2*pi*f*T)
  // oracle for a bounded real circular IFFT + direct fractional-UI DFT.
  // The 8 MHz grid, 32.768 GHz Nyquist and 64-sample delay are chosen
  // independently of the implementation's peak detector.
  ComPreparedData delayed;
  const size_t fftN=8192;
  const size_t delaySamples=64;
  const double df=8e6;
  for(size_t k=0;k<=fftN/2;++k) {
    delayed.frequencyHz.push_back(static_cast<double>(k)*df);
    delayed.conditionedThru.push_back(std::polar(1.,
      -2.*std::numbers::pi*static_cast<double>(k)*
      static_cast<double>(delaySamples)/static_cast<double>(fftN)));
  }
  const auto pulse=calculateCom120F8BandlimitedPulse(delayed);
  demand(pulse.status==si::channel::Status::Warning &&
         pulse.usable() && pulse.value.has_value(),
         "bandlimited pulse diagnostic must remain a warning");
  demand(pulse.value->mainCursorSample==delaySamples,
         "analytic pure-delay main cursor index");
  close(pulse.value->samplePeriodS,1./(fftN*df),"IFFT time step");
  close(pulse.value->mainCursorTimeS,delaySamples/(fftN*df),
        "absolute phase delay retained");
  close(pulse.value->mainCursorAmplitude,1.,"unit amplitude of pure delay");
  demand(pulse.value->uiTapOffsets.size()==13 &&
         pulse.value->uiTapAmplitudes.size()==13 &&
         pulse.value->uiTapOffsets[3]==0.,
         "signed pre/post UI taps must have explicit cursor origin");
  close(pulse.value->uiTapAmplitudes[3],1.,"fractional-UI direct DFT cursor");
  demand(pulse.provenance.algorithm==
         "com-120f8-bandlimited-pulse-exploratory" &&
         !pulse.diagnostics.empty() &&
         pulse.diagnostics[0].code=="NOT_IEEE_COM",
         "exploratory impulse cannot produce IEEE COM");
  ComPreparedData nonuniform=delayed;
  nonuniform.frequencyHz[2]+=1e6;
  reject([&]{(void)calculateCom120F8BandlimitedPulse(nonuniform);},
         "must refuse implicit nonuniform frequency resampling");
  ComPreparedData nonHermitian=delayed;
  nonHermitian.conditionedThru[0]={1.,0.1};
  reject([&]{(void)calculateCom120F8BandlimitedPulse(nonHermitian);},
         "must refuse imaginary DC in real-time impulse");
  ComPreparedData flat=delayed;
  flat.conditionedThru.assign(flat.conditionedThru.size(),{1.,0.});
  reject([&]{(void)calculateCom120F8BandlimitedPulse(flat);},
         "must refuse circularly wrapped negative pre-cursors");
  ComPreparedData missing=delayed;
  missing.conditionedThru.pop_back();
  reject([&]{(void)calculateCom120F8BandlimitedPulse(missing);},
         "must refuse mismatched spectrum arrays");
  std::atomic_bool cancelledPulse{true};
  bool pulseCancelled=false;
  try {
    (void)calculateCom120F8BandlimitedPulse(delayed,{},&cancelledPulse);
  } catch(const si::Cancelled&) {
    pulseCancelled=true;
  }
  demand(pulseCancelled,"pulse diagnostic cancellation must propagate");


  // 012_09_03 PHYSICAL_TO_RX_ORACLE: Two explicitly distinct physically
  // loaded package cases, confirmed differential Zref, identical input
  // grid, independent complex phase and attenuation. Both cases must be
  // conditioned through the SAME fixed 120F-8 FFE/CTLE/RX settings.
  ComPhysicalChannelData loaded;
  loaded.model="EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P";
  loaded.referenceOhmDifferential=100.;
  loaded.frequencyHz=grid;
  loaded.test1.packageCase=ComPackageCase::Test1;
  loaded.test2.packageCase=ComPackageCase::Test2;
  loaded.test1.txPrimaryLineMm=13.;
  loaded.test1.rxPrimaryLineMm=11.;
  loaded.test2.txPrimaryLineMm=31.;
  loaded.test2.rxPrimaryLineMm=29.;
  loaded.test1.loadedSdd.reserve(grid.size());
  loaded.test2.loadedSdd.reserve(grid.size());
  for(double f: grid) {
    ComTwoPortS t1,t2;
    t1.s11=t1.s22={0.,0.};
    t2.s11=t2.s22={0.,0.};
    t1.s21=t1.s12=std::polar(0.8,-2.*std::numbers::pi*f*20e-12);
    t2.s21=t2.s12=std::polar(0.5,-2.*std::numbers::pi*f*40e-12);
    loaded.test1.loadedSdd.push_back(t1);
    loaded.test2.loadedSdd.push_back(t2);
  }
  const auto connected=prepareCom120F8LoadedTestCases(loaded,flatEq);
  demand(connected.usable() && connected.status==Status::Warning &&
         connected.value.has_value() &&
         connected.diagnostics[0].code=="NOT_IEEE_COM",
         "both loaded physical Test1/Test2 must stay non-normative");
  demand(connected.value->test1.frequencyHz==grid &&
         connected.value->test2.frequencyHz==grid,
         "Test1/Test2 must keep the exact same measured frequency samples");
  const si::Complex rxDc=com120F8ReceiverTransfer(0.,-20.,-4.);
  closeComplex(connected.value->test1.conditionedThru[0],0.8*rxDc,
               "Test1 independent DC through physical-to-FFE-to-RX");
  closeComplex(connected.value->test2.conditionedThru[0],0.5*rxDc,
               "Test2 independent DC through physical-to-FFE-to-RX");
  // TX_93A46_GAUSSIAN: fixed, independently precomputed analytic
  // 93A-46 beta=1 samples for the published 7.5ps C2C transition.
  close(com120F8TxTransitionFilter(0.),1.,
        "IEEE TX Gaussian unity at DC");
  close(com120F8TxTransitionFilter(10e9),0.9805954881986921,
        "TX Gaussian real response at 10 GHz");
  close(com120F8TxTransitionFilter(26.5625e9),0.870874406500672,
        "TX Gaussian response at PAM4 Nyquist");
  reject([&]{(void)com120F8TxTransitionFilter(-1.);},
         "negative TX Gaussian frequency must fail");
  reject([&]{(void)com120F8TxTransitionFilter(
      std::numeric_limits<double>::infinity());},
      "infinite TX Gaussian frequency must fail");
  const si::Complex rx10=com120F8ReceiverTransfer(10e9,-20.,-4.);
  const double tx10=com120F8TxTransitionFilter(10e9);
  closeComplex(connected.value->test1.conditionedThru[1000],
               std::polar(0.8,-2.*std::numbers::pi*10e9*20e-12)*rx10*tx10,
               "Test1 independent complex delay and RX at 10 GHz");
  closeComplex(connected.value->test2.conditionedThru[1000],
               std::polar(0.5,-2.*std::numbers::pi*10e9*40e-12)*rx10*tx10,
               "Test2 independent complex delay and RX at 10 GHz");
  demand(std::abs(connected.value->test1.conditionedThru[1000]-
                  connected.value->test2.conditionedThru[1000])>0.05,
         "different loaded package cases may not be silently reused");
  auto broken=loaded;
  broken.test2.packageCase=ComPackageCase::Test1;
  reject([&]{(void)prepareCom120F8LoadedTestCases(broken,flatEq);},
         "cannot pass duplicate Test1 as Test2");
  broken=loaded;
  broken.test2.rxPrimaryLineMm=11.;
  reject([&]{(void)prepareCom120F8LoadedTestCases(broken,flatEq);},
         "cannot swap Test2 RX package length");
  broken=loaded;
  broken.referenceOhmDifferential=50.;
  reject([&]{(void)prepareCom120F8LoadedTestCases(broken,flatEq);},
         "cannot reinterpret physical Zref as differential 50 ohms");
  broken=loaded;
  broken.test1.loadedSdd[4].s11={1.,0.};
  reject([&]{(void)prepareCom120F8LoadedTestCases(broken,flatEq);},
         "active scattering with |S21|<1 must fail full S^H S passivity");
  broken=loaded;
  broken.model="UNQUALIFIED_SYNTHETIC_PHY";
  reject([&]{(void)prepareCom120F8LoadedTestCases(broken,flatEq);},
         "caller cannot silently relabel an unrecognized physical model");
  std::atomic_bool stopConnected{true};
  bool cancelledConnected=false;
  try{(void)prepareCom120F8LoadedTestCases(loaded,flatEq,{},&stopConnected);}
  catch(const si::Cancelled&){cancelledConnected=true;}
  demand(cancelledConnected,"physical-to-RX bridge cancellation");

  // REAL multiport network execution: causal Tx/board/Rx package Test1
  // and Test2 -> full complex loaded Sdd21 -> native FFE/CTLE/RX.
  // The board response is an independently defined passive 4-port, not
  // a preconstructed scalar Sdd21 or a generated normative COM margin.
  ComCausalPhysicalInput measuredS4p;
  measuredS4p.frequencyHz=grid;
  measuredS4p.boardS4p.reserve(grid.size());
  for(double f:grid) {
    ComPhysicalFourPort b;
    b.referenceOhm={50.,50.,50.,50.};
    const si::Complex g=std::polar(0.75,-2.*std::numbers::pi*f*12e-12);
    for(int c=0;c<2;++c) {
      b.scattering[4*(c+2)+c]=g;
      b.scattering[4*c+(c+2)]=g;
    }
    measuredS4p.boardS4p.push_back(b);
  }
  const auto causalCases=calculateHistoricalCausalC2cCases(measuredS4p);
  demand(causalCases.usable() && causalCases.value.has_value(),
         "causal four-port independent physical Test1/2 setup");
  const auto physicalAndRx=prepareCom120F8LoadedTestCases(
      *causalCases.value,flatEq);
  demand(physicalAndRx.usable() && physicalAndRx.value.has_value(),
         "end-to-end causal S4P/package/FFE/CTLE/RX both cases");
  closeComplex(physicalAndRx.value->test1.conditionedThru[0],
               0.75*rxDc,"real physical Test1 DC package->receiver");
  closeComplex(physicalAndRx.value->test2.conditionedThru[0],
               0.75*rxDc,"real physical Test2 DC package->receiver");
  demand(std::abs(physicalAndRx.value->test1.conditionedThru[1000]-
                  physicalAndRx.value->test2.conditionedThru[1000])>1e-5,
         "real causal physical Test1/Test2 must retain independent phase");
  demand(physicalAndRx.diagnostics[0].code=="NOT_IEEE_COM",
         "full causal cascade still must not advertise IEEE COM");

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
