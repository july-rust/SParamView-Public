#include "si/channel_analysis/eye.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void check(bool pass, const char *message) {
  if(!pass) throw std::runtime_error(message);
}
void close(double actual, double expected, double tolerance, const char *what) {
  if(!std::isfinite(actual) || std::abs(actual-expected)>tolerance)
    throw std::runtime_error(what);
}
template<class Fn> void reject(Fn fn,const char *why) {
  bool caught=false;
  try {fn();} catch(const Error&) {caught=true;}
  check(caught,why);
}
ComplexTrace thru(double fmax=16e9, double delaySeconds=0,
                  double gain=1, bool differential=true) {
  ComplexTrace t;
  t.stimulusOhm=t.responseOhm=differential?100:50;
  t.parameter=differential?"Sdd21":"S21";
  for(int i=0;i<=16;++i) {
    const double f=fmax*double(i)/16;
    t.frequencyHz.push_back(f);
    t.transfer.push_back(gain*std::polar(1.,-2*std::acos(-1.)*f*delaySeconds));
  }
  return t;
}
ComplexTrace gridAlignedDelay(const EyeSettings &s, double delaySeconds) {
  ComplexTrace t;
  t.stimulusOhm=t.responseOhm=100;
  t.parameter="Sdd21";
  const double fs=s.bitRateHz*double(s.samplesPerUI);
  for(size_t k=0;k<=s.fftLength/2;++k) {
    const double f=fs*double(k)/double(s.fftLength);
    t.frequencyHz.push_back(f);
    t.transfer.push_back(std::polar(1.,-2*std::acos(-1.)*f*delaySeconds));
  }
  return t;
}
EyeSettings exact() {
  EyeSettings s;
  s.profile=EyeProfile::Custom;
  s.mode=EyeMode::Differential;
  s.pattern=EyePattern::Prbs7;
  s.bitRateHz=4e9;
  s.referenceFrequencyHz=2e9;
  s.analysisStopHz=16e9;
  s.fftLength=2048;
  s.samplesPerUI=8;
  s.electrical.txLowV=-0.2;
  s.electrical.txHighV=+0.2;
  s.electrical.txSourceOhm=100;
  s.electrical.rxTerminationOhm=100;
  s.electrical.rxThresholdV=0;
  s.electrical.rxInputMinimumV=-0.5;
  s.electrical.rxInputMaximumV=0.5;
  return s;
}
}
int main() {
  try {
    const auto mipi=eyeProfile(EyeProfile::MipiDphyHs);
    close(mipi.bitRateHz,4.5e9,1,"MIPI 4.5 Gb/s");
    close(mipi.referenceFrequencyHz,2.25e9,1,"MIPI reference 2.25 GHz");
    close(mipi.analysisStopHz,10e9,1,"MIPI cap 10 GHz");
    check(std::isnan(mipi.electrical.txLowV) &&
          std::isnan(mipi.electrical.rxTerminationOhm) &&
          std::isnan(mipi.electrical.rxThresholdV),
          "MIPI electrical values must not be invented");
    for(const EyeProfile p:{EyeProfile::Ddr2,EyeProfile::Ddr3,EyeProfile::Ddr4,
                            EyeProfile::Ddr5,EyeProfile::Lvds,EyeProfile::Slvs,
                            EyeProfile::SlvsEc,EyeProfile::Custom}) {
      const auto profile=eyeProfile(p);
      check(std::isnan(profile.bitRateHz) &&
            std::isnan(profile.electrical.txHighV),
            "other profiles must require explicit bit rate and electrical values");
    }
    reject([&]{(void)calculateEye(thru(),mipi);},
           "MIPI must fail closed on missing electrical profile");
    auto s=exact();
    auto ideal=calculateEye(thru(),s);
    check(ideal.usable() && ideal.value->simulated,"ideal deterministic model");
    close(ideal.value->eyeHeightV,0.4,1e-10,"ideal eye height oracle");
    close(ideal.value->eyeWidthUI,1.0,1e-12,"ideal width oracle");
    close(ideal.value->unitIntervalSeconds,250e-12,1e-18,"4 Gb/s UI");
    check(ideal.value->overlayV.size()==64 &&
          ideal.value->overlayPhaseUI.size()==16,"two-UI overlay layout");
    for(size_t i=0;i<ideal.value->receivedV.size();++i)
      close(ideal.value->receivedV[i],ideal.value->transmittedV[i],1e-10,
            "identity channel discrete Fourier oracle");
    // Exact integer-sample phase delay is an independent analytic DFT golden.
    const double dt=1.0/(s.bitRateHz*double(s.samplesPerUI));
    auto delayed=calculateEye(gridAlignedDelay(s,4*dt),s);
    close(delayed.value->estimatedDelaySeconds,4*dt,1e-15,"integer delay");
    close(delayed.value->eyeHeightV,0.4,1e-10,"delayed ideal eye height");
    for(size_t i=0;i<ideal.value->receivedV.size();++i) {
      const size_t delayedIndex=(i+4)%ideal.value->receivedV.size();
      close(delayed.value->receivedV[delayedIndex],
            ideal.value->transmittedV[i],1e-10,"phase preserved (not |H|)");
    }
    auto attenuation=calculateEye(thru(16e9,0,0.5),s);
    close(attenuation.value->eyeHeightV,0.2,1e-10,"half-gain eye voltage");
    auto higher=s; higher.electrical.txLowV=-0.4;
    higher.electrical.txHighV=0.4;
    higher.electrical.rxInputMinimumV=-1;
    higher.electrical.rxInputMaximumV=1;
    auto doubled=calculateEye(thru(),higher);
    close(doubled.value->eyeHeightV,0.8,1e-10,"explicit TX amplitude scaling");
    // User-selectable reference frequency MUST update bit rate/UI, not just a label.
    const auto moved=eyeWithReferenceFrequency(s,3e9);
    const auto reff=calculateEye(thru(),moved);
    close(reff.value->bitRateHz,6e9,1,"NRZ new 6 Gb/s for 3 GHz reference");
    close(reff.value->unitIntervalSeconds,1.0/6e9,1e-18,
          "reference adjustment must change actual waveform UI");
    auto mismatched=s; mismatched.referenceFrequencyHz=7e9;
    reject([&]{(void)calculateEye(thru(),mismatched);},
           "incoherent NRZ reference vs data rate must be rejected");
    // 10 GHz capped simulation: no extrapolation, explicit brick-wall warning.
    auto capped=s;capped.analysisStopHz=10e9;
    auto band=calculateEye(thru(),capped);
    check(band.usable() && band.value->bandLimited &&
          band.status==Status::Warning,
          "band-limited simulated eye must stay marked non-qualified");
    close(band.value->analysisStopHz,10e9,1,"selected cap");
    // 4.5 Gb/s / 2.25 GHz / 10 GHz with fully supplied user electrical input.
    auto m=mipi;
    m.electrical=s.electrical;
    auto example=calculateEye(thru(16e9),m);
    check(example.usable() && example.value->bitRateHz==4.5e9 &&
          example.value->referenceFrequencyHz==2.25e9 &&
          example.value->analysisStopHz==10e9,
          "MIPI configured numerical example");
    // No false standard PASS; the only output is simulated/warning.
    check(example.status==Status::Warning,"no MIPI conformance claim");
    auto reversed=thru();reversed.parameter="Sdd12";
    check(calculateEye(reversed,s).usable(),"reverse THRU supported");
    auto single=s; single.mode=EyeMode::SingleEnded;
    single.electrical.txSourceOhm=50;
    single.electrical.rxTerminationOhm=50;
    check(calculateEye(thru(16e9,0,1,false),single).usable(),
          "single-ended THRU with matched impedance");
    // Fail-closed gate matrix: no implicit voltage/termination/DC/bandfill.
    auto bad=s;bad.electrical.rxThresholdV=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)calculateEye(thru(),bad);},"missing RX threshold");
    bad=s;bad.electrical.rxTerminationOhm=50;
    reject([&]{(void)calculateEye(thru(),bad);},"unmatched RX impedance");
    bad=s;bad.electrical.txSourceOhm=50;
    reject([&]{(void)calculateEye(thru(),bad);},"unmatched TX impedance");
    bad=s;bad.electrical.txHighV=bad.electrical.txLowV;
    reject([&]{(void)calculateEye(thru(),bad);},"zero TX voltage swing");
    bad=s;bad.electrical.rxInputMinimumV=0.01;
    reject([&]{(void)calculateEye(thru(),bad);},"RX threshold out of range");
    bad=s;bad.electrical.rxInputMinimumV=-0.1;
    bad.electrical.rxInputMaximumV=0.1;
    reject([&]{(void)calculateEye(thru(),bad);},"RX out-of-linear-range signal");
    bad=s;bad.analysisStopHz=17e9;
    reject([&]{(void)calculateEye(thru(),bad);},"analysis above sampling Nyquist");
    bad=s;bad.referenceFrequencyHz=17e9;
    reject([&]{(void)calculateEye(thru(),bad);},"marker above cap");
    bad=s;bad.bitRateHz=0;
    reject([&]{(void)calculateEye(thru(),bad);},"zero bit rate");
    bad=s;bad.fftLength=2000;
    reject([&]{(void)calculateEye(thru(),bad);},"non-radix-2 FFT");
    bad=s;bad.pattern=EyePattern::CustomBits;
    bad.customBits.assign(16,1);
    reject([&]{(void)calculateEye(thru(),bad);},"all-one custom pattern");
    bad=s;bad.pattern=EyePattern::CustomBits;
    bad.customBits={1,0,2,1,0,0,1,1,0,1,0,1,0,1,0,0};
    reject([&]{(void)calculateEye(thru(),bad);},"non-binary custom bits");
    auto noDc=thru();noDc.frequencyHz.erase(noDc.frequencyHz.begin());
    noDc.transfer.erase(noDc.transfer.begin());
    reject([&]{(void)calculateEye(noDc,s);},"missing measured DC");
    reject([&]{(void)calculateEye(thru(10e9),s);},"source cap incomplete");
    auto unknown=thru();unknown.parameter="Sdd11";
    reject([&]{(void)calculateEye(unknown,s);},"reflection is not THRU");
    auto mixed=thru();mixed.responseOhm=50;
    reject([&]{(void)calculateEye(mixed,s);},"different port reference impedances");
    auto corrupted=thru();corrupted.transfer[4]={NAN,0};
    reject([&]{(void)calculateEye(corrupted,s);},"nonfinite complex spectrum");
    std::atomic_bool cancelled{true};
    bool cancelledFlag=false;
    try {(void)calculateEye(thru(),s,{},&cancelled);}
    catch(const Cancelled&) {cancelledFlag=true;}
    check(cancelledFlag,"cancellation must propagate");
    std::cout<<"012_07 Eye deterministic golden, phase and fail-closed PASS\n";
    return 0;
  }catch(const std::exception &e) {
    std::cerr<<"012_07 Eye FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
