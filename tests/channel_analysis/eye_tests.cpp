#include "si/channel_analysis/eye.hpp"
#include <atomic>
#include <algorithm>
#include <complex>
#include <vector>
#include <cmath>
#include <iostream>
#include <functional>
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
    auto tooFastMipi=eyeWithReferenceFrequency(m,3e9);
    reject([&]{(void)calculateEye(thru(16e9),tooFastMipi);},
           "MIPI v2.1 must reject a 6 Gb/s named profile");
    auto wrongMipi=m;
    wrongMipi.mode=EyeMode::SingleEnded;
    wrongMipi.electrical.txSourceOhm=50;
    wrongMipi.electrical.rxTerminationOhm=50;
    reject([&]{(void)calculateEye(thru(16e9,0,1,false),wrongMipi);},
           "MIPI HS must reject a single-ended profile");
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
    // Independent two-tap circular convolution: no production FFT or metric
    // helper used for the oracle. First tap 0.75, one-UI echo 0.25.
    auto echo=s; echo.fftLength=1024; echo.pattern=EyePattern::CustomBits;
    echo.customBits={0,0,1,1,0,1,0,1,1,0,0,1,0,1,1,0};
    const size_t count=echo.fftLength, spp=echo.samplesPerUI;
    const double fs=echo.bitRateHz*double(spp);
    ComplexTrace taps; taps.parameter="Sdd21";
    taps.stimulusOhm=taps.responseOhm=100;
    const double pi=std::acos(-1.);
    for(size_t k=0;k<=count/2;++k) {
      const double f=fs*double(k)/double(count);
      taps.frequencyHz.push_back(f);
      taps.transfer.push_back(0.75+0.25*std::polar(1.,-2*pi*double(k*spp)/double(count)));
    }
    auto echoed=calculateEye(taps,echo);
    const auto &ed=*echoed.value;
    for(size_t i=0;i<count;++i) {
      const double x=echo.customBits[(i/spp)%echo.customBits.size()]?0.2:-0.2;
      const size_t prev=(i+count-spp)%count;
      const double xp=echo.customBits[(prev/spp)%echo.customBits.size()]?0.2:-0.2;
      close(ed.transmittedV[i],x,1e-12,"custom pattern");
      close(ed.receivedV[i],0.75*x+0.25*xp,1e-10,"two-tap direct convolution");
      close(ed.impulse.values[i],i==0?0.75:i==spp?0.25:0,1e-10,"two-tap impulse");
      const double pulse=(i<spp?0.75:0)+(i>=spp && i<2*spp?0.25:0);
      close(ed.unitPulse.values[i],pulse,1e-10,"two-tap pulse");
    }
    close(ed.eyeHeightV,0.2,1e-10,"two-tap worst separation");
    close(ed.eyeWidthUI,1,1e-12,"two-tap threshold width");
    check(ed.frameBitCount==count/spp && ed.overlayV.size()==64 &&
          ed.settings.customBits==echo.customBits && ed.samplingPhaseThresholdOpen,
          "immutable settings and frame/overlay distinction");
    auto closed=echo; closed.electrical.rxThresholdV=0.15;
    const auto shut=calculateEye(taps,closed);
    close(shut.value->eyeHeightV,0.2,1e-10,"height independent of threshold");
    close(shut.value->eyeWidthUI,0,1e-12,"closed threshold width");
    check(!shut.value->samplingPhaseThresholdOpen,"closed threshold phase");
    // Independent direct inverse DFT on a clipped, nonuniform complex trace.
    auto nonuniform=echo; nonuniform.analysisStopHz=9.3e9;
    ComplexTrace irregular; irregular.parameter="Sdd21";
    irregular.stimulusOhm=irregular.responseOhm=100;
    irregular.frequencyHz={0,0.7e9,2.1e9,4.9e9,8.3e9,11.4e9,16e9};
    irregular.transfer={{0.9,0},{0.85,-0.1},{0.7,-0.25},{0.45,-0.3},{0.25,-0.2},{0.1,-0.1},{0.05,0}};
    const auto nr=calculateEye(irregular,nonuniform);
    const auto &nd=*nr.value;
    std::vector<double> oracle(count,0);
    for(size_t k=0;k<=count/2;++k) {
      const double f=fs*double(k)/double(count);
      if(f>nonuniform.analysisStopHz)break;
      auto upper=std::upper_bound(irregular.frequencyHz.begin(),irregular.frequencyHz.end(),f);
      const size_t idx=size_t(upper-irregular.frequencyHz.begin())-1;
      const double fraction=(f-irregular.frequencyHz[idx])/(irregular.frequencyHz[idx+1]-irregular.frequencyHz[idx]);
      const auto z=irregular.transfer[idx]+fraction*(irregular.transfer[idx+1]-irregular.transfer[idx]);
      for(size_t i=0;i<count;++i)
        oracle[i]+=(k==0?z.real():2*std::real(z*std::polar(1.,2*pi*double(k*i)/double(count))))/double(count);
    }
    for(size_t i=0;i<count;++i) {
      close(nd.impulse.values[i],oracle[i],2e-12,"direct nonuniform DFT");
      double y=0,pulse=0;
      for(size_t j=0;j<count;++j) {
        const size_t at=(i+count-j)%count;
        const double x=echo.customBits[(at/spp)%echo.customBits.size()]?0.2:-0.2;
        y+=oracle[j]*x;
        if(at<spp)pulse+=oracle[j];
      }
      close(nd.receivedV[i],y,2e-11,"direct nonuniform convolution");
      close(nd.unitPulse.values[i],pulse,2e-11,"direct nonuniform pulse");
    }
    // Independently enumerate all phase metrics from oracle-aligned waveform.
    const auto delayIndex=size_t(std::max_element(oracle.begin(),oracle.end(),
      [](double a,double b){return std::abs(a)<std::abs(b);})-oracle.begin());
    close(nd.estimatedDelaySeconds,double(delayIndex)/fs,1e-18,"nonuniform delay");
    double bestHeight=-1e9; std::vector<bool> valid(spp);
    for(size_t phase=0;phase<spp;++phase) {
      double hi=1e9,lo=-1e9;
      for(size_t symbol=0;symbol<count/spp;++symbol) {
        const double v=nd.receivedV[(symbol*spp+delayIndex+phase)%count];
        if(echo.customBits[symbol%echo.customBits.size()])hi=std::min(hi,v);
        else lo=std::max(lo,v);
      }
      bestHeight=std::max(bestHeight,hi-lo); valid[phase]=hi>0 && lo<0;
    }
    size_t width=0;
    for(size_t start=0;start<spp;++start) {
      size_t len=0; while(len<spp && valid[(start+len)%spp])++len;
      width=std::max(width,len);
    }
    close(nd.eyeHeightV,bestHeight,1e-10,"independent height");
    close(nd.eyeWidthUI,double(width)/double(spp),1e-12,"independent circular width");
    close(nd.eyeWidthSeconds,double(width)/double(spp)/echo.bitRateHz,1e-18,"width seconds");
    // Independent PRBS recurrence and period check (x^7+x^6+1).
    std::vector<int> prbs(256,0); prbs[0]=1;
    for(size_t i=7;i<prbs.size();++i)prbs[i]=prbs[i-7]^prbs[i-6];
    for(size_t i=0;i<prbs.size();++i)
      close(ideal.value->transmittedV[i*s.samplesPerUI],prbs[i]?0.2:-0.2,1e-12,"PRBS recurrence");
    check(std::count(prbs.begin(),prbs.begin()+127,1)==64 &&
          std::equal(prbs.begin(),prbs.begin()+127,prbs.begin()+127),"PRBS127 period");
    for(size_t period=1;period<127;++period)
      check(!std::equal(prbs.begin(),prbs.begin()+127,prbs.begin()+period),"PRBS short period");
    auto maxGrid=s; maxGrid.fftLength=65536; maxGrid.samplesPerUI=128;
    maxGrid.analysisStopHz=16e9;
    auto maxResult=calculateEye(thru(),maxGrid);
    check(maxResult.usable() && maxResult.value->receivedV.size()==65536 &&
          maxResult.value->overlayV.size()==64,"bounded maximum FFT");
    auto tiny=s; tiny.bitRateHz=1e-310; tiny.referenceFrequencyHz=5e-311;
    tiny.analysisStopHz=2e-310;
    reject([&]{(void)calculateEye(thru(),tiny);},"nonrepresentable time grid");
    auto huge=s; huge.electrical.txHighV=1e308;
    reject([&]{(void)calculateEye(thru(),huge);},"nonrepresentable mV");
    auto imaginary=thru(); imaginary.transfer.front()={1,0.5};
    reject([&]{(void)calculateEye(imaginary,s);},"nonreal DC");
    // 012_52: exact independent gain oracle and every operating-policy edge.
    auto lowTrace=[](double start, bool withDc=false) {
      auto t=thru(16e9,0,0.5);
      t.frequencyHz.erase(t.frequencyHz.begin());
      t.transfer.erase(t.transfer.begin());
      t.frequencyHz.insert(t.frequencyHz.begin(),{start,2*start,3*start});
      t.transfer.insert(t.transfer.begin(),3,Complex{0.5,0});
      if(withDc) {t.frequencyHz.insert(t.frequencyHz.begin(),0);t.transfer.insert(t.transfer.begin(),Complex{0.5,0});}
      return t;
    };
    for(double f:{1.,1000.,5000.,9999.,10000.,10001.,50000.,99999.,100000.,100001.,1e6}) {
      auto t=lowTrace(f);
      auto approved=s; approved.acceptLimitedDc=true;
      if(f>=100000.) {
        reject([&]{(void)calculateEye(t,approved);},"blocked frequency cannot be overridden");
        continue;
      }
      auto dc=inspectEyeDc(t);
      check(dc.requiresConfirmation==(f>10000.) && dc.status=="EXTRAPOLATED DC","frequency boundary policy");
      if(f>10000.)reject([&]{(void)calculateEye(t,s);},"explicit limited DC acknowledgement required");
      const auto actual=calculateEye(t,approved);
      close(actual.value->dc.value,0.5,0.001,"analytic gain DC");
      close(actual.value->eyeHeightV,0.2,0.004,"analytic gain height");
      close(actual.value->eyeWidthUI,1,0.01,"analytic gain width");
      for(size_t i=0;i<actual.value->receivedV.size();++i)
        close(actual.value->receivedV[i],0.5*actual.value->transmittedV[i],1e-10,"independent gain waveform");
      close(t.frequencyHz.front(),f,0,"original frequency remains immutable");
      close(std::abs(t.transfer.front()-Complex{0.5,0}),0,0,"original complex sample remains immutable");
    }
    auto good=lowTrace(1000,true);
    check(inspectEyeDc(good).status=="ORIGINAL DC","original DC preserved");
    auto damaged=good;damaged.transfer.front()={0.9,0.2};
    auto repaired=calculateEye(damaged,s);
    check(repaired.value->dc.status=="CORRECTED DC","corrupt DC corrected");
    close(repaired.value->dc.value,0.5,0.001,"corrected analytical DC");
    close(std::abs(damaged.transfer.front()-Complex{0.9,0.2}),0,0,"damaged source untouched");
    // A real DC discrepancy alone is not proof of corruption: a real
    // AC-coupled channel may have a pole well below the first measured f.
    auto realSpike=good;realSpike.transfer.front()={0.9,0};
    reject([&]{(void)calculateEye(realSpike,s);},"real DC ambiguity must not be silently corrected");
    auto hiddenPole=good;
    for(size_t i=0;i<hiddenPole.transfer.size();++i) {
      const Complex x{0,hiddenPole.frequencyHz[i]/0.01};
      hiddenPole.transfer[i]=x/(Complex{1,0}+x);
    }
    reject([&]{(void)calculateEye(hiddenPole,s);},"real AC-coupled zero DC must not become unity");
    auto blocked=lowTrace(100000,true);blocked.transfer.front()={0.5,0.2};
    reject([&]{(void)calculateEye(blocked,s);},"invalid original DC cannot bypass cutoff");
    auto unstable=lowTrace(1000);unstable.transfer[1]={0.2,0.4};
    reject([&]{(void)calculateEye(unstable,s);},"unstable local fit blocked");
    // DC-sensitive waveform (nonzero mean) and analytic RC high-pass.
    auto acSettings=s;acSettings.analysisStopHz=10e9;
    acSettings.electrical.txLowV=0.1;acSettings.electrical.txHighV=0.5;
    acSettings.electrical.rxInputMinimumV=-2;acSettings.electrical.rxInputMaximumV=2;
    auto ac=lowTrace(1000,true);
    for(size_t i=0;i<ac.transfer.size();++i) {
      const Complex x{0,ac.frequencyHz[i]/1e8};
      ac.transfer[i]=x/(Complex{1,0}+x);
    }
    auto acMissing=ac;acMissing.frequencyHz.erase(acMissing.frequencyHz.begin());acMissing.transfer.erase(acMissing.transfer.begin());
    auto acRef=calculateEye(ac,acSettings);auto acEx=calculateEye(acMissing,acSettings);
    close(acEx.value->dc.value,0,0.001,"AC coupling analytic zero DC");
    double mse=0;
    for(size_t i=0;i<acRef.value->receivedV.size();++i) {
      const double e=acRef.value->receivedV[i]-acEx.value->receivedV[i];mse+=e*e;
    }
    close(std::sqrt(mse/acRef.value->receivedV.size()),0,0.004,"AC coupling waveform RMSE");
    close(acEx.value->eyeHeightV,acRef.value->eyeHeightV,0.004,"AC coupling height comparison");
    close(acEx.value->eyeWidthUI,acRef.value->eyeWidthUI,0.01,"AC coupling width comparison");
    // Analytic H(f) evaluated on the exact FFT grid, with independent direct
    // inverse DFT and explicit pattern convolution, not production interpolation.
    auto modelSettings=echo;modelSettings.analysisStopHz=10e9;
    modelSettings.electrical.rxInputMinimumV=-2;modelSettings.electrical.rxInputMaximumV=2;
    modelSettings.electrical.txLowV=0.1;modelSettings.electrical.txHighV=0.5;
    const double mfs=modelSettings.bitRateHz*modelSettings.samplesPerUI;
    const size_t mn=modelSettings.fftLength, ms=modelSettings.samplesPerUI;
    const double delay=4/mfs;
    const std::vector<std::function<Complex(double)>> models={
      [=](double f){return std::polar(1.,-2*pi*f*delay);},
      [](double f){return Complex{1,0}/Complex{1,f/1e9};},
      [](double f){const Complex x{0,f/1e8};return x/(Complex{1,0}+x);},
      [=](double f){return std::polar(0.8*std::exp(-0.15*std::sqrt(f/1e9)),-2*pi*f*delay);}
    };
    for(const auto &model:models) {
      ComplexTrace input;input.parameter="Sdd21";input.stimulusOhm=input.responseOhm=100;
      input.frequencyHz={1000,2000,3000};
      for(size_t k=1;k<=mn/2;++k)input.frequencyHz.push_back(mfs*double(k)/double(mn));
      for(double f:input.frequencyHz)input.transfer.push_back(model(f));
      auto reference=input;reference.frequencyHz.insert(reference.frequencyHz.begin(),0);
      reference.transfer.insert(reference.transfer.begin(),model(0));
      const auto result=calculateEye(input,modelSettings), original=calculateEye(reference,modelSettings);
      const auto &v=*result.value,&o=*original.value;
      close(v.dc.value,model(0).real(),0.001,"analytic model DC");
      std::vector<double> impulseOracle(mn,model(0).real()/double(mn));
      for(size_t k=1;k<mn/2;++k) {
        const double f=mfs*double(k)/double(mn);if(f>modelSettings.analysisStopHz)break;
        for(size_t i=0;i<mn;++i)
          impulseOracle[i]+=2*std::real(model(f)*std::polar(1.,2*pi*double(k*i)/double(mn)))/double(mn);
      }
      double err2=0, cumulative=0, cumulativeRef=0;
      for(size_t i=0;i<mn;++i) {
        double expected=0,pulse=0;
        for(size_t j=0;j<mn;++j) {
          const size_t at=(i+mn-j)%mn;
          const bool bit=modelSettings.customBits[(at/ms)%modelSettings.customBits.size()]!=0;
          expected+=impulseOracle[j]*(bit?0.5:0.1);
          if(at<ms)pulse+=impulseOracle[j];
        }
        close(v.impulse.values[i],impulseOracle[i],0.001,"independent model impulse");
        close(v.unitPulse.values[i],pulse,0.004,"independent model pulse");
        cumulative+=v.impulse.values[i];cumulativeRef+=impulseOracle[i];
        close(cumulative,cumulativeRef,0.001,"independent model cumulative step");
        err2+=(expected-v.receivedV[i])*(expected-v.receivedV[i]);
      }
      close(std::sqrt(err2/mn),0,0.004,"independent model waveform RMSE");
      close(v.eyeHeightV,o.eyeHeightV,0.004,"model eye height comparison");
      close(v.eyeWidthUI,o.eyeWidthUI,0.01,"model width comparison");
      close(v.estimatedDelaySeconds,o.estimatedDelaySeconds,1/mfs,"model timing alignment");
    }
    auto sparse=thru();sparse.frequencyHz.insert(sparse.frequencyHz.begin()+1,1000);
    sparse.transfer.insert(sparse.transfer.begin()+1,Complex{1,0});
    sparse.frequencyHz.erase(sparse.frequencyHz.begin());sparse.transfer.erase(sparse.transfer.begin());
    check(inspectEyeDc(sparse).limited,"sparse low-band reconstruction must disclose limitation");
    auto ambiguous=sparse;ambiguous.transfer[0]={0.5,0.4};
    reject([&]{(void)calculateEye(ambiguous,s);},"sparse ambiguous AC pole must not be invented");
    // 012_54: observed 15.863 kHz pole with low-band points on both sides.
    // Independent analytic circuit formula uses separate real/imag components.
    {
      constexpr double fc=15863., gain=0.93;
      auto measured=lowTrace(1000);
      measured.frequencyHz.clear(); measured.transfer.clear();
      for(double f:{1000.,2000.,4000.,8000.,10000.,12000.,16000.,20000.,24000.,32000.,48000.,60000.,80000.}) {
        measured.frequencyHz.push_back(f);
        const double x=f/fc;
        measured.transfer.emplace_back(gain*x*x/(1+x*x),gain*x/(1+x*x));
      }
      for(size_t k=1;k<=512;++k) {
        const double f=16e9*double(k)/512.;
        const double x=f/fc;
        measured.frequencyHz.push_back(f);
        measured.transfer.emplace_back(gain*x*x/(1+x*x),gain*x/(1+x*x));
      }
      const auto estimate=inspectEyeDc(measured);
      check(estimate.acModel && estimate.limited && !estimate.requiresConfirmation,
            "observed AC pole must be accepted with limited provenance");
      close(estimate.value,0,1e-12,"AC zero DC");
      close(estimate.acPoleHz,fc,1e-6,"analytic pole");
      close(estimate.acGain,gain,1e-12,"analytic gain");
      close(estimate.acFitMaxResidual,0,1e-12,"held-out complex response");
      auto result=calculateEye(measured,acSettings);
      const auto &eye=*result.value;
      const double tau=1./(2*pi*fc);
      close(eye.acTimeConstantSeconds,tau,1e-15,"analytic RC time constant");
      close(eye.acFrameDroopFraction,1.-std::exp(-double(eye.timeSeconds.size())*eye.timeStepSeconds/tau),
            1e-12,"analytic RC droop, not full channel step");
      // Independent brute-force run length from the input waveform, including
      // the wrap boundary; compare a time-domain state recurrence to exp().
      size_t longest=0, same=0;
      const size_t samples=eye.settings.samplesPerUI;
      const size_t count=eye.transmittedV.size()/samples;
      double previous=eye.transmittedV[(count-1)*samples];
      for(size_t i=0;i<2*count;++i) {
        const double now=eye.transmittedV[(i%count)*samples];
        same=std::abs(now-previous)<1e-12?same+1:1;
        longest=std::max(longest,same); previous=now;
      }
      close(eye.acRunBits,double(longest),0,"longest periodic NRZ run");
      const double ui=eye.unitIntervalSeconds;
      const double decay=std::exp(-ui/tau);
      double remaining=1.;
      for(size_t i=0;i<longest;++i) remaining*=decay;
      close(eye.acRunDroopFraction,1.-remaining,1e-12,"independent run droop");
      const auto longBits=size_t(std::ceil(5.*tau/ui));
      close(eye.acFiveTauBits,double(longBits),0,"five tau duration");
      remaining=1.;
      for(size_t i=0;i<longBits;++i) remaining*=decay;
      close(eye.acFiveTauRemainingFraction,remaining,1e-11,"independent long-run state recurrence");
      check(eye.acLongModelAvailable && eye.acLongModelConverged,"isolated long step grid agreement");
      const std::array<double,3> tauMultipliers{1.,3.,5.};
      for(size_t j=0;j<tauMultipliers.size();++j)
        close(eye.acLongStepVPerV[j],gain*std::exp(-tauMultipliers[j]),0.001,
              "isolated channel RC step vs independent continuous analytical response");
      check(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),
        [](const auto &d){return d.code=="BASELINE_WANDER_NOT_QUALIFIED";}),
        "periodic preview must not qualify long response");
      auto badHoldout=measured; badHoldout.transfer[5]+=Complex{0.01,0.01};
      reject([&]{(void)inspectEyeDc(badHoldout);},"held-out AC fit mismatch rejected");
      auto contradictory=measured;
      contradictory.frequencyHz.insert(contradictory.frequencyHz.begin(),0.);
      contradictory.transfer.insert(contradictory.transfer.begin(),Complex{0.5,0});
      reject([&]{(void)inspectEyeDc(contradictory);},"observed conflicting real DC rejected");
    }
    // The auto configuration does not infer a PHY; manual reproduction must
    // give the same deterministic output from the existing periodic engine.
    {
      auto autoTrace=thru(16e9,0,0.5);
      const auto automatic=automaticEyeSettings(autoTrace,true);
      auto explicitSettings=automatic;
      explicitSettings.bitRateHz=4.5e9;explicitSettings.referenceFrequencyHz=2.25e9;
      explicitSettings.analysisStopHz=10e9;
      explicitSettings.electrical={-0.2,0.2,autoTrace.stimulusOhm,autoTrace.responseOhm,0,-1,1};
      const auto autoEye=calculateEye(autoTrace,automatic);
      const auto manualEye=calculateEye(autoTrace,explicitSettings);
      close(autoEye.value->eyeHeightV,manualEye.value->eyeHeightV,1e-12,"auto/manual Eye height");
      close(autoEye.value->eyeWidthUI,manualEye.value->eyeWidthUI,1e-12,"auto/manual Eye width");
      close(autoEye.value->bitRateHz,4.5e9,0,"assumed rate cap");
      auto amplitude=automatic;amplitude.electrical.txLowV*=1.1;amplitude.electrical.txHighV*=1.1;
      close(calculateEye(autoTrace,amplitude).value->eyeHeightV,
        1.1*autoEye.value->eyeHeightV,1e-12,"Eye amplitude sensitivity");
    }
    std::cout<<"012_54 observed AC pole and analytical droop candidate PASS\n";
    std::cout<<"012_52 DC policy, gain oracle, AC coupling and damaged DC PASS\n";
    std::cout<<"012_46 Eye independent DFT, convolution, metrics, PRBS and resource PASS\n";
    std::cout<<"012_07 Eye deterministic golden, phase and fail-closed PASS\n";
    return 0;
  }catch(const std::exception &e) {
    std::cerr<<"012_07 Eye FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
