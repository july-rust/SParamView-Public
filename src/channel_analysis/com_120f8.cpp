#include "si/channel_analysis/com_120f8.hpp"
#include "si/channel_analysis/com_causal_channel.hpp"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <utility>
#include <string_view>

namespace si::channel {
namespace {
void require(bool ok, const char* message) {
  if (!ok) throw Error(message);
}
void gridValue(double value, Com120F8DiscreteRange range,
               const char* message) {
  require(std::isfinite(value), message);
  // Preserve exact discrete integer step index, with only floating point
  // representation tolerance. An arbitrary middle coefficient is rejected.
  const double index = (value-range.minimum)/range.step;
  require(value >= range.minimum-1e-12 &&
          value <= range.maximum+1e-12 &&
          std::abs(index-std::round(index)) <= 1e-9, message);
}
}
void validateCom120F8Equalizer(const Com120F8EqualizerChoice& choice) {
  const auto p=final8023ck120F8();
  for(size_t i=0;i<p.txNoncursor.size();++i) {
    const size_t coefficientIndex = i==3 ? 4 : i;
    gridValue(choice.txFfe[coefficientIndex],p.txNoncursor[i],
              "Final IEEE 120F-8 TX FFE tap outside discrete bounds");
  }
  // Annex 93A TX FFE c(0) is NOT an independent optimization
  // coefficient. Its value is exactly 1-sum(|c(i)|, i != 0), and the
  // published final 120F-8 lower cursor bound applies to that derived value.
  const double cursor=choice.txFfe[3];
  double noncursorMagnitude=0.;
  for(size_t i=0;i<choice.txFfe.size();++i)
    if(i!=3) noncursorMagnitude+=std::abs(choice.txFfe[i]);
  const double derivedCursor=1.-noncursorMagnitude;
  require(std::isfinite(cursor) &&
          std::isfinite(derivedCursor) &&
          derivedCursor>=p.minimumCursor-1e-12 &&
          derivedCursor<=1. &&
          std::abs(cursor-derivedCursor)<=1e-12,
          "Final IEEE 120F-8 FFE cursor must equal 1-sum(abs(non-cursor taps)) and respect minimum");
  gridValue(choice.ctleDcGainDb,p.ctleDcGainDb,
            "Final IEEE 120F-8 CTLE DC gain outside discrete bounds");
  gridValue(choice.ctleDcGain2Db,p.ctleDcGain2Db,
            "Final IEEE 120F-8 CTLE second DC gain outside discrete bounds");
}
double com120F8TxTransitionFilter(double frequencyHz) {
  require(std::isfinite(frequencyHz) && frequencyHz>=0. &&
          frequencyHz<=1e12,
          "IEEE 120F-8 transmitter transition frequency out of range");
  // Gaussian 93A-46, beta=1 (not the beta=2 variant for older PHYs).
  // Tr=7.5ps from 120F.4.1. This transfer is a single component only;
  // it does NOT replace X(f), complete pulse response or COM statistics.
  const double a=std::numbers::pi*frequencyHz*
                 final8023ck120F8().txTransitionTimeS/1.6832;
  const double value=std::exp(-a*a);
  require(std::isfinite(value) && value>=0. && value<=1.,
          "Invalid COM TX Gaussian transition transfer");
  return value;
}

Complex com120F8Ctle(double frequencyHz,double gDcDb,double gDc2Db) {
  require(std::isfinite(frequencyHz) && frequencyHz>=0,
          "IEEE 120F-8 CTLE invalid frequency");
  const auto p=final8023ck120F8();
  gridValue(gDcDb,p.ctleDcGainDb,"IEEE CTLE gDC not on final discrete range");
  gridValue(gDc2Db,p.ctleDcGain2Db,
            "IEEE CTLE gDC2 not on final discrete range");
  const Complex j{0,1};
  const Complex numerator=
      (std::pow(10.,gDcDb/20.)+j*(frequencyHz/p.ctleZeroFrequencyHz))*
      (std::pow(10.,gDc2Db/20.)+j*(frequencyHz/p.ctleLowFrequencyPoleZeroHz));
  const Complex denominator=
      (Complex{1,0}+j*(frequencyHz/p.ctlePole1FrequencyHz))*
      (Complex{1,0}+j*(frequencyHz/p.ctlePole2FrequencyHz))*
      (Complex{1,0}+j*(frequencyHz/p.ctleLowFrequencyPoleZeroHz));
  require(std::isfinite(std::abs(denominator)) &&
          std::abs(denominator)>1e-15,
          "IEEE CTLE denominator numerical singularity");
  const Complex output=numerator/denominator;
  require(std::isfinite(output.real()) && std::isfinite(output.imag()),
          "IEEE CTLE nonfinite complex response");
  return output;
}
Complex com120F8ReceiverNoiseFilter(double frequencyHz) {
  require(std::isfinite(frequencyHz) && frequencyHz>=0,
          "IEEE 120F-8 receiver noise filter invalid frequency");
  const double x=frequencyHz/final8023ck120F8().receiverBandwidthHz;
  require(std::isfinite(x) && x<=1e6,
          "IEEE 120F-8 receiver noise filter frequency out of range");
  // IEEE 802.3ck working-group formula for fourth-order Butterworth
  // low-pass. The transfer is complex, not an attenuation-only magnitude.
  const double x2=x*x;
  const Complex denominator{
      1-(2+std::sqrt(2.))*x2+x2*x2,
      std::sqrt(4+2*std::sqrt(2.))*(x-x2*x)};
  require(std::isfinite(std::abs(denominator)) &&
          std::abs(denominator)>1e-15,
          "IEEE receiver noise filter denominator numerical singularity");
  const Complex output=Complex{1,0}/denominator;
  require(std::isfinite(output.real()) && std::isfinite(output.imag()),
          "IEEE receiver noise filter nonfinite complex response");
  return output;
}
Complex com120F8ReceiverTransfer(double frequencyHz,
                                  double gDcDb,double gDc2Db) {
  return com120F8Ctle(frequencyHz,gDcDb,gDc2Db)*
         com120F8ReceiverNoiseFilter(frequencyHz);
}
Result<ComPreparedData> prepareCom120F8ReceiverSpectra(
    ComInput input,const Com120F8EqualizerChoice& choice,
    Provenance provenance,const std::atomic_bool* cancelled) {
  if(cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
  require(input.rxFilter.frequencyHz.empty() &&
          input.rxFilter.transfer.empty(),
          "Final-table receiver model must not override external RX filter");
  validateCom120F8Equalizer(choice);
  validateTrace(input.thru);
  validateCom120F8DirectGrid(input.thru.frequencyHz);
  const auto p=final8023ck120F8();
  ComSettings settings;
  settings.symbolRateHz=p.symbolRateHz;
  settings.referenceOhm=2*p.referenceOhmSingleEnded;
  settings.txFfeTaps.assign(choice.txFfe.begin(),choice.txFfe.end());
  input.rxFilter.parameter="Sdd21";
  input.rxFilter.stimulusOhm=input.rxFilter.responseOhm=settings.referenceOhm;
  input.rxFilter.frequencyHz=input.thru.frequencyHz;
  input.rxFilter.transfer.reserve(input.thru.frequencyHz.size());
  for(size_t i=0;i<input.thru.frequencyHz.size();++i) {
    if(!(i&1023u) && cancelled &&
       cancelled->load(std::memory_order_relaxed))throw Cancelled();
    input.rxFilter.transfer.push_back(
      com120F8ReceiverTransfer(input.thru.frequencyHz[i],
                               choice.ctleDcGainDb,choice.ctleDcGain2Db));
  }
  auto out=prepareComSpectra(
      input,settings,std::move(provenance),cancelled);
  // The generic FIR kernel interprets slot k as a k-UI delay, whereas
  // Annex 120F-8 slots are relative to the main cursor c(0) at index 3:
  // c(-3),c(-2),c(-1),c(0),c(+1).
  // Remove the common three-UI placement delay from ALL returned complex
  // spectra, preserving cursor-relative pre/post timing and cross-talk phase.
  // Do not shift RX filter source data: only the final FFE coordinate changes.
  require(out.usable() && out.value.has_value(),
          "COM 120F-8 prepared spectra unavailable");
  for(size_t i=0;i<out.value->frequencyHz.size();++i) {
    if(!(i&1023u) && cancelled &&
       cancelled->load(std::memory_order_relaxed))throw Cancelled();
    const double angle=2.*std::numbers::pi*
        (out.value->frequencyHz[i]/p.symbolRateHz)*3.;
    const Complex cursorAlignment=std::polar(1.,angle);
    out.value->txFfeResponse[i]*=cursorAlignment;
    out.value->conditionedThru[i]*=cursorAlignment;
    for(auto& a:out.value->aggressors)
      a.conditionedTransfer[i]*=cursorAlignment;
  }
  out.provenance.algorithm="com-120f8-ffe-ctle-rx-exploratory";
  out.provenance.version="012_09-final-numeric-component-1";
  return out;
}
void validateCom120F8DirectGrid(const std::vector<double>& frequencyHz) {
  const auto p=final8023ck120F8();
  require(frequencyHz.size()>=3 && frequencyHz.size()<=65536,
          "IEEE 120F-8 direct grid missing/over budget");
  require(frequencyHz.front()==0.0,
          "Direct-grid contract requires measured DC; no silent extrapolation");
  require(frequencyHz[1]<=p.maximumStartFrequencyHz,
          "IEEE 120F-8 first positive frequency exceeds fmin");
  for(size_t i=0;i<frequencyHz.size();++i) {
    const double f=frequencyHz[i];
    require(std::isfinite(f) && f>=0,
            "IEEE 120F-8 nonfinite or negative frequency");
    if(i) {
      const double prev=frequencyHz[i-1];
      require(f>prev &&
              f-prev<=p.maximumFrequencyStepHz+
                      1e-10*p.maximumFrequencyStepHz,
              "IEEE 120F-8 direct-grid frequency step exceeds 10 MHz");
    }
  }
  require(frequencyHz.back()>=p.symbolRateHz/2.,
          "IEEE 120F-8 direct grid stops below 26.5625 GHz Nyquist");
}

Result<Com120F8PulseDiagnostics> calculateCom120F8BandlimitedPulse(
    const ComPreparedData& prepared, Provenance provenance,
    const std::atomic_bool* cancelled) {
  auto checkCancelled = [&] {
    if (cancelled && cancelled->load(std::memory_order_relaxed))
      throw Cancelled();
  };
  checkCancelled();
  const auto& frequencies = prepared.frequencyHz;
  const auto& transfer = prepared.conditionedThru;
  const size_t bins = frequencies.size();
  require(bins >= 3 && bins <= 16385 && transfer.size() == bins,
          "COM pulse: missing spectrum or bounded radix-2 positive bins");
  const size_t n = 2*(bins-1);
  require(std::has_single_bit(n), "COM pulse: positive bins must form radix-2 IFFT");
  validateCom120F8DirectGrid(frequencies);
  const double df = frequencies[1];
  require(std::isfinite(df) && df>0, "COM pulse: invalid frequency spacing");
  for (size_t k=0; k<bins; ++k) {
    if (!(k&1023u)) checkCancelled();
    const double expected = static_cast<double>(k)*df;
    require(std::abs(frequencies[k]-expected) <=
              1e-10*std::max(1., std::abs(expected)),
            "COM pulse: nonuniform grid cannot be implicitly resampled");
    require(std::isfinite(transfer[k].real()) &&
            std::isfinite(transfer[k].imag()),
            "COM pulse: nonfinite complex spectrum");
  }
  // DC and Nyquist samples of an actual real impulse must be real. Do not
  // silently discard a physically inconsistent imaginary component.
  for (const auto& endpoint : {transfer.front(),transfer.back()}) {
    require(std::abs(endpoint.imag()) <=
              1e-10*std::max(1.,std::abs(endpoint.real())),
            "COM pulse: non-Hermitian DC/Nyquist endpoint");
  }
  std::vector<Complex> full(n);
  full[0]={transfer.front().real(),0.};
  full[n/2]={transfer.back().real(),0.};
  for (size_t k=1; k<n/2; ++k) {
    full[k]=transfer[k];
    full[n-k]=std::conj(transfer[k]);
  }
  checkCancelled();
  fft(full,true);
  checkCancelled();
  const double dt=1./(static_cast<double>(n)*df);
  const double ui=1./final8023ck120F8().symbolRateHz;
  const double period=static_cast<double>(n)*dt;
  require(std::isfinite(dt) && dt>0 && std::isfinite(period) &&
          std::isfinite(ui) && period>12*ui,
          "COM pulse: invalid finite analysis window");
  size_t peakIndex=0;
  double peakMagnitude=0.;
  for (size_t k=0; k<n; ++k) {
    if (!(k&1023u)) checkCancelled();
    require(std::isfinite(full[k].real()) && std::isfinite(full[k].imag()) &&
            std::abs(full[k].imag())<1e-9*
              std::max(1.,std::abs(full[k].real())),
            "COM pulse: invalid real-valued inverse transform");
    const double magnitude=std::abs(full[k].real());
    if (magnitude>peakMagnitude) {
      peakMagnitude=magnitude;
      peakIndex=k;
    }
  }
  require(peakMagnitude>=1e-15,
          "COM pulse: no resolvable main cursor");
  const double cursorTime=static_cast<double>(peakIndex)*dt;
  // Circular FFT impulses carry periodic aliasing. Refuse to interpret
  // wrapped pre/post-cursors as real channel ISI contributions.
  require(cursorTime>=3*ui && cursorTime+9*ui<period,
          "COM pulse: cursor too close to circular window boundary");
  Com120F8PulseDiagnostics data;
  data.samplePeriodS=dt;
  data.mainCursorTimeS=cursorTime;
  data.mainCursorAmplitude=full[peakIndex].real();
  data.mainCursorSample=peakIndex;
  data.uiTapOffsets.reserve(13);
  data.uiTapAmplitudes.reserve(13);
  // Band-limited Fourier-series evaluation directly at arbitrary UI phases.
  // Linear interpolation of coarse IFFT samples would bias tap amplitudes.
  for (int offset=-3; offset<=9; ++offset) {
    checkCancelled();
    const double t=cursorTime+static_cast<double>(offset)*ui;
    const double x=t/dt;
    double value=transfer.front().real()+
                 transfer.back().real()*std::cos(std::numbers::pi*x);
    for (size_t k=1; k<n/2; ++k) {
      if (!(k&2047u)) checkCancelled();
      const double phase=2.*std::numbers::pi*
                         static_cast<double>(k)*x/static_cast<double>(n);
      value+=2.*(transfer[k]*std::polar(1.,phase)).real();
    }
    value/=static_cast<double>(n);
    require(std::isfinite(value),
            "COM pulse: nonfinite UI-sampled impulse");
    data.uiTapOffsets.push_back(static_cast<double>(offset));
    data.uiTapAmplitudes.push_back(value);
  }
  require(std::abs(data.uiTapAmplitudes[3]-data.mainCursorAmplitude) <=
            1e-8*std::max(1.,peakMagnitude),
          "COM pulse: inconsistent FFT and direct DFT cursor");
  Result<Com120F8PulseDiagnostics> result;
  result.status=Status::Warning;
  result.value=std::move(data);
  result.provenance=std::move(provenance);
  result.provenance.algorithm="com-120f8-bandlimited-pulse-exploratory";
  result.provenance.version="012_09_03-component-1";
  result.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Band-limited periodic impulse and UI taps only. No standard "
    "93A pulse extrapolation/ISI/DFE/jitter/noise/XTALK optimization, "
    "normative COM dB or PASS/FAIL is calculated."});
  return result;
}


Result<Com120F8LoadedTestCaseSpectra> prepareCom120F8LoadedTestCases(
    const ComPhysicalChannelData& physical,
    const Com120F8EqualizerChoice& choice,
    Provenance provenance, const std::atomic_bool* cancelled) {
  auto check = [&] {
    if(cancelled && cancelled->load(std::memory_order_relaxed))
      throw Cancelled();
  };
  check();
  const auto p=final8023ck120F8();
  // Do NOT silently change a caller's physical network into the IEEE
  // standard: this bridge currently accepts only the explicitly historical
  // causal package engine, and emits a warning diagnostic.
  require(physical.model &&
          std::string_view(physical.model)==
            "EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P",
          "COM 120F-8 bridge: unrecognized physical package provenance");
  validateCom120F8Equalizer(choice);
  validateCom120F8DirectGrid(physical.frequencyHz);
  require(physical.referenceOhmDifferential==
            2.*p.referenceOhmSingleEnded,
          "COM 120F-8 physical bridge requires 100-ohm differential Zref");
  require(std::isfinite(physical.maxObservedModeConversion) &&
          physical.maxObservedModeConversion >= 0. &&
          physical.maxObservedModeConversion <= 1.,
          "COM 120F-8 invalid mode-conversion diagnostic");
  const size_t n=physical.frequencyHz.size();
  require(physical.test1.packageCase==ComPackageCase::Test1 &&
          physical.test2.packageCase==ComPackageCase::Test2 &&
          physical.test1.loadedSdd.size()==n &&
          physical.test2.loadedSdd.size()==n,
          "COM 120F-8 Test1/Test2 selector or sample count mismatch");
  for(size_t caseIndex=0;caseIndex<2;++caseIndex) {
    const auto& selected=caseIndex==0?physical.test1:physical.test2;
    require(std::abs(selected.txPrimaryLineMm-
              p.txPackageLengthMm[caseIndex])<=1e-12 &&
            std::abs(selected.rxPrimaryLineMm-
              p.rxPackageLengthMm[caseIndex])<=1e-12,
            "COM 120F-8 physical test package lengths mismatch");
  }
  auto toComInput=[&](const ComPhysicalChannelCase& channel) {
    ComInput out;
    out.thru.parameter="Sdd21";
    out.thru.stimulusOhm=out.thru.responseOhm=
        physical.referenceOhmDifferential;
    out.thru.frequencyHz=physical.frequencyHz;
    out.thru.transfer.reserve(n);
    for(size_t k=0;k<n;++k) {
      if(!(k&1023u))check();
      const auto& s=channel.loadedSdd[k];
      const Complex values[]{s.s11,s.s21,s.s12,s.s22};
      for(const Complex& v:values)
        require(std::isfinite(v.real()) && std::isfinite(v.imag()),
                "COM 120F-8 nonfinite loaded physical scattering");
      // For a passive two-port, the maximum eigenvalue of S^H*S is <= 1.
      // A magnitude-only S21 check would miss active reflected networks.
      const double a=std::norm(s.s11)+std::norm(s.s21);
      const double d=std::norm(s.s12)+std::norm(s.s22);
      const Complex b=std::conj(s.s11)*s.s12+
                      std::conj(s.s21)*s.s22;
      const double discriminant=(a-d)*(a-d)+4.*std::norm(b);
      const double lambda=(a+d+std::sqrt(discriminant))*0.5;
      require(std::isfinite(lambda) && lambda<=1.+1e-7,
              "COM 120F-8 loaded 2-port is nonpassive");
      out.thru.transfer.push_back(s.s21);
    }
    return out;
  };
  auto one=prepareCom120F8ReceiverSpectra(
      toComInput(physical.test1),choice,provenance,cancelled);
  check();
  auto two=prepareCom120F8ReceiverSpectra(
      toComInput(physical.test2),choice,provenance,cancelled);
  require(one.usable() && two.usable(),
          "COM 120F-8 loaded receiver transfer is unavailable");
  check();
  Com120F8LoadedTestCaseSpectra data;
  data.test1=std::move(*one.value);
  data.test2=std::move(*two.value);
  // Apply the same phase-preserving, real Gaussian TX transition
  // response to both physically distinct package test cases.
  // The full Annex 93A rectangular input pulse X(f) remains missing;
  // DO NOT mistake these conditioned spectra for COM or a pulse.
  for(size_t k=0;k<n;++k) {
    if(!(k&1023u))check();
    const double tx=com120F8TxTransitionFilter(physical.frequencyHz[k]);
    data.test1.conditionedThru[k]*=tx;
    data.test2.conditionedThru[k]*=tx;
  }
  Result<Com120F8LoadedTestCaseSpectra> result;
  result.status=Status::Warning;
  result.value=std::move(data);
  result.provenance=std::move(provenance);
  result.provenance.algorithm="com-120f8-loaded-physical-ht-ffe-rx-exploratory";
  result.provenance.version="012_09_03-bridge-1";
  result.diagnostics.push_back({
    Severity::Warning,"NOT_IEEE_COM",
    "Historical causal physical S4P Test1/Test2 loaded channels were "
    "separately conditioned through C++ 93A-46 Ht, FFE/CTLE/RX. This is an "
    "exploratory complex transfer: no complete Annex 93A pulse/ISI, "
    "crosstalk/jitter/noise/DFE optimization, COM dB or IEEE PASS/FAIL."});
  return result;
}


Result<Com120F8RectangularPulseData> calculateCom120F8RectangularPulse(
    const ComPreparedData& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance,
    const std::atomic_bool* cancelled) {
  auto check = [&] {
    if(cancelled && cancelled->load(std::memory_order_relaxed))
      throw Cancelled();
  };
  check();
  const auto& f=conditioned.frequencyHz;
  const auto& h=conditioned.conditionedThru;
  validateCom120F8DirectGrid(f);
  const size_t n=f.size();
  require(h.size()==n && n<=32769,
          "COM rectangular pulse requires a complete bounded THRU spectrum");
  require(!queryTimeSeconds.empty() && queryTimeSeconds.size()<=512 &&
          n<=4000000/queryTimeSeconds.size(),
          "COM rectangular pulse numerical operation budget exceeded");
  double largestStep=0.;
  for(size_t k=0;k<n;++k) {
    if(!(k&1023u))check();
    require(std::isfinite(h[k].real()) && std::isfinite(h[k].imag()),
            "COM rectangular pulse source has nonfinite spectrum");
    if(k)largestStep=std::max(largestStep,f[k]-f[k-1]);
  }
  require(std::abs(h.front().imag()) <=
            1e-10*std::max(1.,std::abs(h.front().real())),
          "COM rectangular pulse DC must be real");
  require(std::isfinite(largestStep) && largestStep>0,
          "COM rectangular pulse invalid integration step");
  // A nonuniform positive-frequency grid combined with plain trapezoids
  // creates spectral comb aliases (false late ISI/postcursors). The COM
  // engine does not yet qualify a phase-aware nonuniform quadrature or a
  // controlled resampler, so explicitly reject ALL such grids. The
  // caller may supply a separately checked uniform-grid source, never
  // have this numerical primitive silently interpolate/extrapolate it.
  const double uniformStep=f[1]-f[0];
  for(size_t k=2;k<n;++k) {
    const double actual=f[k]-f[k-1];
    require(std::abs(actual-uniformStep)<=
              1e-9*uniformStep,
            "COM rectangular pulse nonuniform frequency grid requires "
            "independently qualified phase-aware resampling");
  }
  for(size_t j=0;j<queryTimeSeconds.size();++j) {
    const double t=queryTimeSeconds[j];
    // A quadrature grid aliases oscillatory exp(j*2*pi*f*t) if the
    // requested absolute time exceeds its inverse-frequency support.
    // Fail CLOSED instead of silently wrapping an unqualified result.
    require(std::isfinite(t) && t>=0 && t*largestStep<=0.45,
            "COM rectangular pulse time exceeds measured-grid delay support");
    if(j)require(t>queryTimeSeconds[j-1],
                  "COM rectangular pulse query times must be increasing");
  }
  const double ui=1./final8023ck120F8().symbolRateHz;
  require(std::isfinite(ui) && ui>0,
          "COM rectangular pulse invalid symbol period");
  std::vector<double> weights(n),excitation(n);
  for(size_t k=0;k<n;++k) {
    if(!(k&1023u))check();
    // Trapezoidal quadrature on the validated STRICTLY UNIFORM
    // measured frequency grid. No extrapolated DC or upper stop band.
    weights[k]=(k==0?(f[1]-f[0])*0.5:
                k+1==n?(f[n-1]-f[n-2])*0.5:
                (f[k+1]-f[k-1])*0.5);
    const double argument=std::numbers::pi*f[k]*ui;
    const double sinc=(k==0 ? 1. : std::sin(argument)/argument);
    // X(f) = UI*sinc(pi*f*UI)*exp(-j*pi*f*UI).
    excitation[k]=ui*sinc;
    require(std::isfinite(weights[k]) && weights[k]>0 &&
            std::isfinite(excitation[k]),
            "COM rectangular pulse integration weights overflow");
  }
  Com120F8RectangularPulseData data;
  data.timeSeconds=queryTimeSeconds;
  data.symbolPeriodSeconds=ui;
  data.unitSymbolPulse.reserve(queryTimeSeconds.size());
  for(size_t j=0;j<queryTimeSeconds.size();++j) {
    check();
    const double t=queryTimeSeconds[j];
    double sum=0.,compensation=0.;
    for(size_t k=0;k<n;++k) {
      if(!(k&1023u))check();
      // Fold X(f)'s half-UI phase into the Fourier exponent to preserve
      // original THRU/FFE/CTLE delay and signed pre/postcursor polarity.
      const double angle=2.*std::numbers::pi*f[k]*(t-ui*0.5);
      const double realValue=(h[k]*std::polar(1.,angle)).real();
      const double term=2.*weights[k]*excitation[k]*realValue;
      require(std::isfinite(term),
              "COM rectangular pulse quadrature term overflow");
      const double corrected=term-compensation;
      const double next=sum+corrected;
      compensation=(next-sum)-corrected;
      sum=next;
    }
    require(std::isfinite(sum),
            "COM rectangular pulse nonfinite integrated signal");
    data.maximumAbsolutePulse=std::max(data.maximumAbsolutePulse,
                                      std::abs(sum));
    data.unitSymbolPulse.push_back(sum);
  }
  Result<Com120F8RectangularPulseData> out;
  out.status=Status::Warning;
  out.value=std::move(data);
  out.provenance=std::move(provenance);
  out.provenance.algorithm="com-120f8-rectangular-symbol-band-quadrature";
  out.provenance.version="012_09_03-pulse-1";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Unit-symbol rectangular excitation convolved with the supplied "
    "complex measured-band transfer; unmeasured high frequencies are "
    "not inferred. No calibrated voltage, 93A COM equalizer optimum, "
    "full ISI/DFE/XTALK/jitter/noise, COM dB, or PHY PASS/FAIL."});
  return out;
}

Result<Com120F8RectangularPulseCases>
calculateCom120F8RectangularPulseCases(
    const Com120F8LoadedTestCaseSpectra& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance,const std::atomic_bool* cancelled) {
  if(cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
  require(conditioned.model &&
          std::string_view(conditioned.model)==
            "EXPLORATORY_C2C_PACKAGE_HT_FFE_CTLE_RX",
          "COM rectangular pulse: unrecognized Test1/Test2 provenance");
  require(conditioned.test1.frequencyHz==
            conditioned.test2.frequencyHz,
          "COM rectangular pulse: Test1/Test2 frequency grids disagree");
  const auto one=calculateCom120F8RectangularPulse(
      conditioned.test1,queryTimeSeconds,provenance,cancelled);
  const auto two=calculateCom120F8RectangularPulse(
      conditioned.test2,queryTimeSeconds,provenance,cancelled);
  require(one.usable() && two.usable(),
          "COM rectangular pulse: Test1/Test2 result unavailable");
  Result<Com120F8RectangularPulseCases> out;
  out.status=Status::Warning;
  out.value=Com120F8RectangularPulseCases{
      std::move(*one.value),std::move(*two.value)};
  out.provenance=std::move(provenance);
  out.provenance.algorithm="com-120f8-two-package-symbol-pulse-exploratory";
  out.provenance.version="012_09_03-pulse-1";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Test1/Test2 pulse arrays are band-limited unit-symbol diagnostics. "
    "No IEEE COM ratio, independent golden correlation or PASS/FAIL."});
  return out;
}

} // namespace si::channel
