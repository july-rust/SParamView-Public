#include "si/channel_analysis/eye.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>

namespace si::channel {
namespace {
void require(bool ok, const char *reason) {
  if (!ok) throw Error(reason);
}
void check(const std::atomic_bool *flag) {
  if (flag && flag->load(std::memory_order_relaxed)) throw Cancelled();
}
bool positive(double x) { return std::isfinite(x) && x > 0; }
bool near(double x, double y) {
  return std::abs(x-y) <= 1e-8 * std::max({1., std::abs(x), std::abs(y)});
}
bool finite(Complex x) {
  return std::isfinite(x.real()) && std::isfinite(x.imag());
}
bool closeReal(Complex x) {
  return std::abs(x.imag()) <= 1e-9 * std::max(1., std::abs(x.real()));
}
std::vector<uint8_t> sequence(const EyeSettings &s, size_t bits) {
  std::vector<uint8_t> out(bits);
  if (s.pattern == EyePattern::Alternating) {
    for (size_t i=0; i<bits; ++i) out[i] = uint8_t(i&1u);
  } else if (s.pattern == EyePattern::Prbs7) {
    unsigned reg = 0x7f;
    for (size_t i=0; i<bits; ++i) {
      out[i] = uint8_t(reg & 1u);
      const unsigned feedback = ((reg >> 6u) ^ (reg >> 5u)) & 1u;
      reg = ((reg << 1u) | feedback) & 0x7fu;
    }
  } else if (s.pattern == EyePattern::CustomBits) {
    require(s.customBits.size() >= 16 && s.customBits.size() <= bits,
      "Eye explicit bit pattern must have 16..FFT/samplesPerUI symbols");
    for (uint8_t b : s.customBits)
      require(b==0 || b==1, "Eye pattern must be binary NRZ");
    for (size_t i=0; i<bits; ++i)
      out[i] = s.customBits[i % s.customBits.size()];
  } else {
    throw Error("Unsupported eye pattern");
  }
  const size_t ones = size_t(std::count(out.begin(),out.end(),uint8_t{1}));
  require(ones >= 8 && (bits-ones) >= 8,
    "Eye pattern needs at least 8 high and 8 low symbols");
  return out;
}
std::vector<Complex> spectrum(const ComplexTrace &trace, double cap,
                              double sampleRate, size_t n,
                              const std::atomic_bool *flag, bool &active) {
  std::vector<Complex> h(n, Complex{0,0});
  const double df=sampleRate / double(n);
  size_t j=0;
  for (size_t k=0; k<=n/2; ++k) {
    if (!(k&1023u)) check(flag);
    const double f=df*double(k);
    if (f > cap) break; // stated ideal zero-above-cap *simulation*; no S extrapolation
    while (j+1 < trace.frequencyHz.size() && trace.frequencyHz[j+1] < f) ++j;
    require(j+1<trace.frequencyHz.size(), "Eye source spectrum does not cover cap");
    const double f0=trace.frequencyHz[j], f1=trace.frequencyHz[j+1];
    const double a=(f-f0)/(f1-f0);
    h[k]=(1-a)*trace.transfer[j]+a*trace.transfer[j+1];
    require(finite(h[k]), "Invalid interpolated transfer value");
    if (std::abs(h[k]) > 1.0001) active=true;
  }
  require(closeReal(h[0]) && closeReal(h[n/2]),
          "Eye DC/Nyquist imaginary component violates Hermitian FFT");
  h[0]={h[0].real(),0};
  h[n/2]={h[n/2].real(),0};
  for(size_t k=1;k<n/2;++k) h[n-k]=std::conj(h[k]);
  return h;
}
std::vector<double> apply(const std::vector<double> &signal,
                          const std::vector<Complex> &h,
                          const std::atomic_bool *flag) {
  std::vector<Complex> values;
  values.reserve(signal.size());
  for(double x:signal) {
    require(std::isfinite(x), "Nonfinite eye stimulus");
    values.emplace_back(x,0);
  }
  check(flag);
  fft(values,false);
  check(flag);
  for(size_t k=0;k<values.size();++k) {
    if (!(k&1023u)) check(flag);
    values[k]*=h[k];
  }
  fft(values,true);
  check(flag);
  std::vector<double> result;
  result.reserve(values.size());
  for(Complex x:values) {
    require(finite(x), "Nonfinite eye convolution output");
    result.push_back(x.real());
  }
  return result;
}
} // namespace

const char *eyeProfileName(EyeProfile profile) {
  switch(profile) {
    case EyeProfile::MipiDphyHs: return "MIPI D-PHY HS (example, not compliance)";
    case EyeProfile::Ddr2: return "DDR2 (user-qualified)";
    case EyeProfile::Ddr3: return "DDR3 (user-qualified)";
    case EyeProfile::Ddr4: return "DDR4 (user-qualified)";
    case EyeProfile::Ddr5: return "DDR5 (user-qualified)";
    case EyeProfile::Lvds: return "LVDS (user-qualified)";
    case EyeProfile::Slvs: return "SLVS (user-qualified)";
    case EyeProfile::SlvsEc: return "SLVS-EC (user-qualified)";
    case EyeProfile::Custom: return "Custom (user-qualified)";
  }
  throw Error("Unknown Eye profile");
}
EyeSettings eyeProfile(EyeProfile profile) {
  (void)eyeProfileName(profile);
  EyeSettings s;
  s.profile=profile;
  if(profile == EyeProfile::MipiDphyHs) {
    s.mode=EyeMode::Differential;
    s.bitRateHz=4.5e9;
    s.referenceFrequencyHz=2.25e9; // NRZ Nyquist marker (NOT the FFT upper cap)
    s.analysisStopHz=10e9;
  }
  // In particular DO NOT assign a fabricated MIPI VOD, termination or RX threshold.
  return s;
}
Result<EyeData> calculateEye(const ComplexTrace &trace, const EyeSettings &s,
                             Provenance provenance,
                             const std::atomic_bool *cancelled) {
  check(cancelled);
  validateTrace(trace);
  require(s.mode==EyeMode::SingleEnded || s.mode==EyeMode::Differential,
          "Single-ended or differential channel mode must be explicitly selected");
  const bool differential=s.mode==EyeMode::Differential;
  require((differential && (trace.parameter=="Sdd21" || trace.parameter=="Sdd12")) ||
          (!differential && (trace.parameter=="S21" || trace.parameter=="S12")),
          "Eye requires confirmed THRU S21/S12 or Sdd21/Sdd12 matching selected mode");
  require(positive(s.bitRateHz) && positive(s.analysisStopHz) &&
          positive(s.referenceFrequencyHz) &&
          s.referenceFrequencyHz <= s.analysisStopHz &&
          s.analysisStopHz >= s.bitRateHz/2,
          "Invalid independent eye bit rate, reference marker or analysis cap");
  require(std::has_single_bit(s.fftLength) &&
          s.fftLength>=1024 && s.fftLength<=65536 &&
          std::has_single_bit(s.samplesPerUI) &&
          s.samplesPerUI>=8 && s.samplesPerUI<=128 &&
          s.fftLength/s.samplesPerUI>=32 &&
          s.fftLength % s.samplesPerUI==0,
          "Eye FFT/samples-per-UI resource or radix-2 contract violated");
  const double sampleRate=s.bitRateHz*double(s.samplesPerUI);
  require(positive(sampleRate) && s.analysisStopHz<=0.5*sampleRate,
          "Eye analysis cap exceeds simulated sampling Nyquist");
  require(trace.frequencyHz.front()==0,
          "Eye time-domain simulation requires measured DC; no DC extrapolation");
  require(trace.frequencyHz.back()>=s.analysisStopHz,
          "Eye source stops below selected analysis cap; no extrapolation");
  const auto &e=s.electrical;
  require(std::isfinite(e.txLowV) && std::isfinite(e.txHighV) &&
          e.txHighV>e.txLowV,
          "Unknown or invalid absolute TX low/high voltage");
  require(positive(e.txSourceOhm) && positive(e.rxTerminationOhm) &&
          near(e.txSourceOhm,trace.stimulusOhm) &&
          near(e.rxTerminationOhm,trace.responseOhm) &&
          near(trace.stimulusOhm,trace.responseOhm),
          "Matched TX/RX and equal real S-parameter Zref required; renormalization not implemented");
  require(std::isfinite(e.rxInputMinimumV) &&
          std::isfinite(e.rxInputMaximumV) &&
          std::isfinite(e.rxThresholdV) &&
          e.rxInputMinimumV<e.rxThresholdV &&
          e.rxThresholdV<e.rxInputMaximumV,
          "Explicit RX input linear range and threshold required");
  (void)eyeProfileName(s.profile); // reject invalid/unknown enum
  const size_t n=s.fftLength, spp=s.samplesPerUI, bitCount=n/spp;
  const double dt=1.0/sampleRate;
  const auto bits=sequence(s,bitCount);
  bool active=false;
  auto h=spectrum(trace,s.analysisStopHz,sampleRate,n,cancelled,active);
  std::vector<Complex> impulseFft=h;
  fft(impulseFft,true);
  check(cancelled);
  size_t delay=0;
  double peak=0, total=0, negativeTail=0;
  for(size_t i=0;i<n;++i) {
    if (!(i&1023u)) check(cancelled);
    const double mag=std::norm(impulseFft[i]);
    total+=mag;
    if(mag>peak) {peak=mag;delay=i;}
    if(i>=n-2*spp) negativeTail+=mag;
  }
  require(total>1e-26 && delay<=n/4,
          "Eye transfer has zero energy or unresolved circular/negative delay");
  EyeData data;
  data.profileName=eyeProfileName(s.profile);
  data.transferParameter=trace.parameter;
  data.bitRateHz=s.bitRateHz;
  data.referenceFrequencyHz=s.referenceFrequencyHz;
  data.analysisStopHz=s.analysisStopHz;
  data.unitIntervalSeconds=1.0/s.bitRateHz;
  data.timeStepSeconds=dt;
  data.estimatedDelaySeconds=double(delay)*dt;
  data.bandLimited=s.analysisStopHz<0.5*sampleRate;
  data.impulse.stepSeconds=dt;
  data.impulse.binStepHz=sampleRate/double(n);
  data.unitPulse.stepSeconds=dt;
  data.unitPulse.binStepHz=data.impulse.binStepHz;
  data.impulse.seconds.reserve(n);
  data.impulse.values.reserve(n);
  data.unitPulse.seconds.reserve(n);
  data.timeSeconds.reserve(n);
  data.transmittedV.reserve(n);
  for(size_t i=0;i<n;++i) {
    const double time=double(i)*dt;
    data.timeSeconds.push_back(time);
    data.impulse.seconds.push_back(time);
    data.unitPulse.seconds.push_back(time);
    data.impulse.values.push_back(impulseFft[i].real());
    data.transmittedV.push_back(bits[i/spp]?e.txHighV:e.txLowV);
  }
  std::vector<double> unitSymbol(n,0.);
  std::fill_n(unitSymbol.begin(),spp,1.);
  data.unitPulse.values=apply(unitSymbol,h,cancelled);
  data.receivedV=apply(data.transmittedV,h,cancelled);
  const auto minmax=std::minmax_element(data.receivedV.begin(),data.receivedV.end());
  data.minimumReceivedV=*minmax.first;
  data.maximumReceivedV=*minmax.second;
  require(data.minimumReceivedV>=e.rxInputMinimumV-1e-10 &&
          data.maximumReceivedV<=e.rxInputMaximumV+1e-10,
          "Simulated RX waveform outside declared RX linear input range");
  std::vector<double> phaseHeights(spp);
  std::vector<bool> opens(spp,false);
  double best=-std::numeric_limits<double>::infinity();
  size_t bestPhase=0;
  for(size_t p=0;p<spp;++p) {
    if (!(p&15u)) check(cancelled);
    double minHigh=std::numeric_limits<double>::infinity();
    double maxLow=-std::numeric_limits<double>::infinity();
    for(size_t bit=0;bit<bitCount;++bit) {
      const double v=data.receivedV[(bit*spp+delay+p)%n];
      if(bits[bit]) minHigh=std::min(minHigh,v);
      else maxLow=std::max(maxLow,v);
    }
    phaseHeights[p]=minHigh-maxLow;
    opens[p]=(minHigh>e.rxThresholdV && maxLow<e.rxThresholdV);
    if(phaseHeights[p]>best) {
      best=phaseHeights[p];
      bestPhase=p;
      data.minimumHighV=minHigh;
      data.maximumLowV=maxLow;
    }
  }
  data.eyeHeightV=best;
  data.samplingPhaseUI=double(bestPhase)/double(spp);
  // Largest circular interval of valid phases; sampled-time estimate only.
  size_t run=0,longest=0;
  for(size_t p=0;p<2*spp;++p) {
    run=opens[p%spp]?std::min(run+1,spp):0;
    longest=std::max(longest,run);
  }
  data.eyeWidthUI=double(longest)/double(spp);
  data.eyeWidthSeconds=data.eyeWidthUI*data.unitIntervalSeconds;
  const size_t overlayCount=std::min<size_t>(bitCount,64);
  data.overlayPhaseUI.reserve(2*spp);
  for(size_t p=0;p<2*spp;++p)
    data.overlayPhaseUI.push_back(double(p)/double(spp));
  data.overlayV.reserve(overlayCount);
  for(size_t bit=0;bit<overlayCount;++bit) {
    if (!(bit&15u)) check(cancelled);
    std::vector<double> segment;
    segment.reserve(2*spp);
    const size_t start=(bit*spp+delay)%n;
    for(size_t p=0;p<2*spp;++p)
      segment.push_back(data.receivedV[(start+p)%n]);
    data.overlayV.push_back(std::move(segment));
  }
  Result<EyeData> result;
  result.provenance=std::move(provenance);
  result.status=Status::Warning; // simulated, ideal out-of-band filter, no PHY pass
  result.diagnostics.push_back({Severity::Warning,"SIMULATED_ONLY",
     "Deterministic periodic NRZ modeled waveform, not a measured eye, BER or PHY compliance"});
  if(data.bandLimited)
    result.diagnostics.push_back({Severity::Warning,"IDEAL_BAND_LIMIT",
      "Above selected measured analysis cap, H is set to zero for simulation ONLY; unknown physical response"});
  if(negativeTail/total > 0.02)
    result.diagnostics.push_back({Severity::Warning,"PRECURSOR_ENERGY",
      "Circular impulse has significant negative-time/precursor tail; causality not qualified"});
  if(active)
    result.diagnostics.push_back({Severity::Warning,"GAIN_ABOVE_UNITY",
      "Transfer magnitude exceeds unity; passivity has not been independently verified"});
  if(data.eyeHeightV<=0 || data.eyeWidthUI==0)
    result.diagnostics.push_back({Severity::Warning,"CLOSED_EYE",
      "Simulated eye is closed at declared RX threshold; no numerical pass/fail compliance"});
  result.value=std::move(data);
  return result;
}
} // namespace si::channel
