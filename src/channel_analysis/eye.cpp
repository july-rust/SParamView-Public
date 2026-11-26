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
// H(f)=G*j*f/(fc+j*f). A real signed gain is appropriate only over a
// sufficiently observed low-frequency band. Use independent alternating
// samples for fit/validation. Fail closed for unobserved poles, phase delay,
// non-one-pole behavior, noisy ratios or incompatible real measured DC.
bool observedHighPass(const ComplexTrace &trace, size_t first, EyeDcInfo &dc) {
  const double f0=trace.frequencyHz[first];
  const auto z0=trace.transfer[first];
  if(std::abs(z0.real())<1e-12) return false;
  const double seed=f0*z0.imag()/z0.real();
  if(!positive(seed) || seed<2*f0 || seed>trace.frequencyHz.back()/2)
    return false;
  std::vector<double> poles;
  size_t end=first, below=0, above=0, central=0;
  while(end<trace.transfer.size() && trace.frequencyHz[end]/seed<=4.) {
    const double f=trace.frequencyHz[end];
    const auto z=trace.transfer[end];
    if(f<seed*0.75) ++below;
    if(f>seed*1.25) ++above;
    if(f/seed>=0.5 && f/seed<=2.) {
      ++central;
      if((end-first)%2==0 && std::abs(z.real())>1e-12) {
        const double p=f*z.imag()/z.real();
        if(!positive(p)) return false;
        poles.push_back(p);
      }
    }
    ++end;
  }
  if(end-first<8 || below<2 || above<2 || central<4 || poles.size()<2)
    return false;
  std::sort(poles.begin(),poles.end());
  const double pole=poles[poles.size()/2];
  if(std::abs(seed/pole-1.)>0.05 || pole<2*f0) return false;
  for(double p:poles) if(std::abs(p/pole-1.)>0.05) return false;
  double dot=0, norm=0;
  for(size_t i=first;i<end;i+=2) {
    const Complex x{0,trace.frequencyHz[i]/pole};
    const Complex b=x/(Complex{1,0}+x);
    dot+=std::real(std::conj(b)*trace.transfer[i]);
    norm+=std::norm(b);
  }
  if(!positive(norm)) return false;
  const double gain=dot/norm;
  if(!std::isfinite(gain) || std::abs(gain)<0.01 || std::abs(gain)>1.0001)
    return false;
  double maxResidual=0;
  for(size_t i=first;i<end;++i) {
    const Complex x{0,trace.frequencyHz[i]/pole};
    const Complex expected=gain*x/(Complex{1,0}+x);
    const double err=std::abs(expected-trace.transfer[i]);
    // Provisional engineering gates, not a standard or measurement uncertainty.
    // Absolute 0.001 ceiling retained; relative gate protects the low-band fit.
    if(!std::isfinite(err) || err>0.001 || err>0.02*std::abs(expected))
      return false;
    maxResidual=std::max(maxResidual,err);
  }
  // A nonzero real observed DC may be genuine. Do not overwrite it.
  const bool original=first!=0;
  if(original && closeReal(trace.transfer.front()) &&
     std::abs(trace.transfer.front().real())>1e-9) return false;
  dc.acModel=true;
  dc.acPoleHz=pole;
  dc.acGain=gain;
  dc.acFitMaxResidual=maxResidual;
  dc.modelSpreadAvailable=false; // residual is distinct from legacy model spread
  dc.value=original && closeReal(trace.transfer.front()) ?
    trace.transfer.front().real():0.;
  dc.status=original?(closeReal(trace.transfer.front())?"ORIGINAL DC":"CORRECTED DC"):
    "EXTRAPOLATED DC";
  dc.method="Observed one-pole RC high-pass (AC-like); alternating-sample fit and holdout";
  if(!original || !closeReal(trace.transfer.front())) {
    dc.effectiveStartHz=f0;
    dc.requiresConfirmation=f0>10000.;
  }
  dc.limited=true; // finite observed band and unqualified long-time channel response
  return true;
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
                              double sampleRate, size_t n, const EyeDcInfo &dc,
                              const std::atomic_bool *flag, bool &active) {
  std::vector<Complex> h(n, Complex{0,0});
  const double df=sampleRate / double(n);
  size_t j=0;
  for (size_t k=0; k<=n/2; ++k) {
    if (!(k&1023u)) check(flag);
    const double f=df*double(k);
    if (f > cap) break; // stated ideal zero-above-cap *simulation*; no S extrapolation
    if(k==0) {
      h[k]={dc.value,0};
    } else if(f<=dc.firstPositiveHz) {
      const size_t first=trace.frequencyHz.front()==0?1:0;
      const double a=f/dc.firstPositiveHz;
      if(dc.acModel) {
        // Anchor the model to the measured first sample, tapering only the
        // small residual to zero at DC. No modification to measured samples.
        const Complex x{0,f/dc.acPoleHz}, x0{0,dc.firstPositiveHz/dc.acPoleHz};
        const Complex b=dc.acGain*x/(Complex{1,0}+x);
        const Complex b0=dc.acGain*x0/(Complex{1,0}+x0);
        h[k]=b+a*(trace.transfer[first]-b0)+(1-a)*Complex{dc.value,0};
      } else {
        h[k]=(1-a)*Complex{dc.value,0}+a*trace.transfer[first];
      }
    } else {
      if(j==0 && trace.frequencyHz.front()==0) j=1;
      while (j+2 < trace.frequencyHz.size() && trace.frequencyHz[j+1] < f) ++j;
      require(j+1<trace.frequencyHz.size() && f<=trace.frequencyHz[j+1],
              "Eye source spectrum does not cover cap");
      const double f0=trace.frequencyHz[j], f1=trace.frequencyHz[j+1];
      const double a=(f-f0)/(f1-f0);
      h[k]=(1-a)*trace.transfer[j]+a*trace.transfer[j+1];
    }
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

std::optional<EyeDcInfo> inspectObservedAcModel(const ComplexTrace &trace) {
  validateTrace(trace);
  EyeDcInfo dc;
  const bool original=trace.frequencyHz.front()==0;
  const size_t first=original?1:0;
  dc.originalStartHz=trace.frequencyHz.front();
  dc.effectiveStartHz=dc.originalStartHz;
  dc.firstPositiveHz=trace.frequencyHz[first];
  if(observedHighPass(trace,first,dc)) return dc;
  return std::nullopt;
}

EyeSettings automaticEyeSettings(const ComplexTrace &trace, bool differential) {
  validateTrace(trace);
  EyeSettings settings;
  settings.profile=EyeProfile::Custom;
  settings.mode=differential?EyeMode::Differential:EyeMode::SingleEnded;
  settings.pattern=EyePattern::Prbs7;
  settings.analysisStopHz=std::min(10e9,trace.frequencyHz.back());
  settings.bitRateHz=std::min(4.5e9,settings.analysisStopHz/1.5);
  settings.referenceFrequencyHz=settings.bitRateHz/2.;
  settings.electrical={-0.2,0.2,trace.stimulusOhm,trace.responseOhm,0,-1,1};
  return settings;
}

EyeDcInfo inspectEyeDc(const ComplexTrace &trace) {
  validateTrace(trace);
  EyeDcInfo dc;
  const bool original=trace.frequencyHz.front()==0;
  const size_t first=original?1:0;
  dc.originalStartHz=trace.frequencyHz.front();
  dc.firstPositiveHz=trace.frequencyHz[first];
  dc.effectiveStartHz=dc.originalStartHz;
  const double f=dc.firstPositiveHz;
  // When the original DC is real but there is no nearby positive sample,
  // preserve the supplied DC (legacy results) without claiming continuity.
  // A nonreal or clearly inconsistent sparse DC cannot be safely repaired.
  if(original && f>=100000.) {
    require(closeReal(trace.transfer[0]), "Invalid DC; first positive frequency >=100 kHz; cannot correct");
    dc.value=trace.transfer[0].real();
    dc.limited=true;
    if(trace.transfer.size()>=3 &&
       std::abs(trace.transfer[1]-trace.transfer[2])<0.02 &&
       std::abs(trace.transfer[1].imag())<0.02)
      require(std::abs(trace.transfer[0]-trace.transfer[1])<=0.1,
              "Sparse DC discontinuity; cannot distinguish physical low-frequency response from corrupt DC");
    return dc;
  }
  require(f<100000., "Eye effective start >=100 kHz; DC extrapolation blocked");
  if(observedHighPass(trace,first,dc)) return dc;
  // Three positive samples: real part is even for a real impulse response.
  // Compare constant, linear, even-quadratic and alternate-point estimates.
  // These are local engineering checks, NOT a bound on unobserved poles.
  if(trace.transfer.size()-first<3) {
    require(original && closeReal(trace.transfer[0]),
            "Insufficient positive samples for DC extrapolation");
    dc.value=trace.transfer[0].real(); dc.limited=true; return dc;
  }
  const auto z=trace.transfer[first];
  const double f2=trace.frequencyHz[first+1], f3=trace.frequencyHz[first+2];
  const double r2=f/f2, r3=f/f3;
  const auto z2=trace.transfer[first+1], z3=trace.transfer[first+2];
  const bool separated=1-r2>1e-6 && 1-r3>1e-6;
  const bool sparse=f2>std::max(100000.,10*f) || f3>std::max(100000.,20*f);
  double estimate=z.real(), spread=std::numeric_limits<double>::infinity();
  if(separated) {
    const double linear=(z.real()-r2*z2.real())/(1-r2);
    const double even=(z.real()-r2*r2*z2.real())/(1-r2*r2);
    const double alternate=(z.real()-r3*r3*z3.real())/(1-r3*r3);
    estimate=even;
    spread=std::max({std::abs(even-z.real()),std::abs(even-linear),
                    std::abs(even-alternate)});
    // Imaginary DC inferred from linear continuation should be zero.
    const double imagIntercept=(z.imag()-r2*z2.imag())/(1-r2);
    spread=std::max(spread,std::abs(imagIntercept));
  }
  const bool stable=std::isfinite(estimate) && spread<=0.001 &&
    (!sparse || std::abs(z.imag())<=0.01);
  dc.modelSpreadAvailable=std::isfinite(spread);
  dc.modelSpread=dc.modelSpreadAvailable?spread:0;
  if(original && closeReal(trace.transfer[0]) &&
     ((stable && std::abs(trace.transfer[0].real()-estimate)<=0.001) ||
      (!stable && std::abs(trace.transfer[0]-z)<=0.01))) {
    dc.value=trace.transfer[0].real();
    dc.limited=sparse || !stable;
    return dc;
  }
  require(stable,"DC missing or suspect; low-frequency models disagree or data insufficient");
  // A physically real but discrepant measured DC may be a genuine unobserved
  // low-frequency pole/zero (e.g. AC coupling), not a corrupt source sample.
  // Do not overwrite it based solely on extrapolation. Nonreal DC violates
  // the real-system Hermitian contract and can be replaced if the fit is stable.
  require(!original || !closeReal(trace.transfer[0]),
          "Real DC conflicts with local extrapolation; cannot distinguish corrupt DC from an unobserved pole/zero");
  dc.effectiveStartHz=f;
  dc.value=estimate;
  dc.status=original?"CORRECTED DC":"EXTRAPOLATED DC";
  dc.method="Local even-real fit; constant/linear/alternate comparison";
  dc.requiresConfirmation=f>10000.;
  dc.limited=sparse || dc.requiresConfirmation || original;
  return dc;
}

const char *eyeProfileName(EyeProfile profile) {
  switch(profile) {
    case EyeProfile::MipiDphyHs: return "MIPI D-PHY v2.1 HS (example, not compliance)";
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
EyeSettings eyeWithReferenceFrequency(EyeSettings s, double referenceHz) {
  require(positive(referenceHz) && positive(2.0*referenceHz),
          "Invalid NRZ reference frequency");
  s.referenceFrequencyHz=referenceHz;
  s.bitRateHz=2.0*referenceHz;
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
          near(s.referenceFrequencyHz,s.bitRateHz/2.0) &&
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
  const auto dc=inspectEyeDc(trace);
  require(!dc.requiresConfirmation || s.acceptLimitedDc,
          "Eye DC extrapolation above 10 kHz requires explicit acknowledgement");
  require(s.analysisStopHz>=dc.firstPositiveHz,
          "Eye analysis cap below first positive source frequency");
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
  if(s.profile==EyeProfile::MipiDphyHs) {
    require(s.mode==EyeMode::Differential && s.bitRateHz<=4.5e9,
      "MIPI D-PHY v2.1 HS needs differential mode and rate at or below 4.5 Gb/s");
  }
  if(s.profile==EyeProfile::Lvds || s.profile==EyeProfile::Slvs ||
     s.profile==EyeProfile::SlvsEc) {
    require(s.mode==EyeMode::Differential,
      "LVDS/SLVS/SLVS-EC profile requires differential signaling");
  }
  const size_t n=s.fftLength, spp=s.samplesPerUI, bitCount=n/spp;
  const double dt=1.0/sampleRate;
  const double ui=1.0/s.bitRateHz;
  require(positive(dt) && positive(ui) && positive(sampleRate/double(n)) &&
          std::isfinite(double(n)*dt*1e12) && std::isfinite(ui*1e12),
          "Eye time grid exceeds finite display range");
  for(double v:{e.txLowV,e.txHighV,e.rxThresholdV,e.rxInputMinimumV,e.rxInputMaximumV})
    require(std::isfinite(v*1000), "Eye electrical value exceeds finite mV range");
  require(std::isfinite((e.txHighV-e.txLowV)*1000),
          "Eye voltage swing exceeds finite mV range");
  const auto bits=sequence(s,bitCount);
  bool active=false;
  auto h=spectrum(trace,s.analysisStopHz,sampleRate,n,dc,cancelled,active);
  std::vector<Complex> impulseFft=h;
  fft(impulseFft,true);
  check(cancelled);
  size_t delay=0;
  double peak=0, total=0, negativeTail=0;
  for(size_t i=0;i<n;++i) {
    if (!(i&1023u)) check(cancelled);
    require(finite(impulseFft[i]), "Nonfinite eye impulse");
    const double mag=std::norm(impulseFft[i]);
    total+=mag;
    if(mag>peak) {peak=mag;delay=i;}
    if(i>=n-2*spp) negativeTail+=mag;
  }
  require(std::isfinite(total) && std::isfinite(negativeTail) &&
          total>1e-26 && delay<=n/4,
          "Eye transfer has zero energy or unresolved circular/negative delay");
  EyeData data;
  data.settings=s;
  data.dc=dc;
  if(dc.acModel) {
    data.acTimeConstantSeconds=1./(2.*std::numbers::pi*dc.acPoleHz);
    data.acFrameDroopFraction=-std::expm1(-double(n)*dt/data.acTimeConstantSeconds);
    // Compute a finite long-run diagnostic in the observed low-band model,
    // independent of the periodic FFT. No allocation proportional to tau/UI.
    size_t run=1, maxRun=1;
    for(size_t i=1;i<bits.size();++i) {
      run=bits[i]==bits[i-1]?run+1:1;
      maxRun=std::max(maxRun,run);
    }
    // The preview repeats its finite frame, so include the boundary run.
    size_t head=1, tail=1;
    while(head<bits.size() && bits[head]==bits.front()) ++head;
    while(tail<bits.size() && bits[bits.size()-1-tail]==bits.back()) ++tail;
    if(bits.front()==bits.back()) maxRun=std::max(maxRun,head+tail);
    data.acRunBits=double(maxRun);
    data.acRunDroopFraction=-std::expm1(-data.acRunBits*ui/data.acTimeConstantSeconds);
    data.acFiveTauBits=std::ceil(5.*data.acTimeConstantSeconds/ui);
    require(positive(data.acFiveTauBits),"AC long-run duration exceeds finite range");
    data.acFiveTauRemainingFraction=std::exp(-data.acFiveTauBits*ui/data.acTimeConstantSeconds);
  }
  data.frameBitCount=bitCount;
  data.reverse=(trace.parameter=="S12" || trace.parameter=="Sdd12");
  data.profileName=eyeProfileName(s.profile);
  data.transferParameter=trace.parameter;
  data.bitRateHz=s.bitRateHz;
  data.referenceFrequencyHz=s.referenceFrequencyHz;
  data.analysisStopHz=s.analysisStopHz;
  data.unitIntervalSeconds=ui;
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
    require(std::isfinite(phaseHeights[p]), "Nonfinite eye height");
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
  data.samplingPhaseThresholdOpen=opens[bestPhase];
  require(std::isfinite(best*1000) &&
          std::isfinite(data.minimumReceivedV*1000) &&
          std::isfinite(data.maximumReceivedV*1000) &&
          std::isfinite((data.maximumReceivedV-data.minimumReceivedV)*1000),
          "Eye result exceeds finite mV range");
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
  result.diagnostics.push_back({Severity::Info,
    dc.status=="ORIGINAL DC"?"ORIGINAL_DC":
    dc.status=="CORRECTED DC"?"CORRECTED_DC":"EXTRAPOLATED_DC",
    dc.method});
  if(dc.acModel) {
    result.diagnostics.push_back({Severity::Info,"AC_LIKE_MODEL",
      "Observed low-band one-pole high-pass fit; not proof of capacitor topology"});
    result.diagnostics.push_back({Severity::Warning,"BASELINE_WANDER_NOT_QUALIFIED",
      "Analytic single-pole droop only; finite periodic eye does not qualify long-pattern channel response"});
    if(double(n)*dt < 5.*data.acTimeConstantSeconds)
      result.diagnostics.push_back({Severity::Warning,"AC_FRAME_SHORT",
        "FFT frame shorter than five estimated RC time constants; long response not resolved"});
  }
  if(dc.limited)
    result.diagnostics.push_back({Severity::Warning,"DC_QUALITY_LIMITED",
      "DC continuity, correction or extrapolation is limited by available low-frequency data; unobserved poles cannot be excluded"});
  if(dc.requiresConfirmation)
    result.diagnostics.push_back({Severity::Warning,"DC_FREQUENCY_ACKNOWLEDGED",
      "Effective start above 10 kHz; user acknowledged DC extrapolation limitations"});
  if(data.bandLimited)
    result.diagnostics.push_back({Severity::Warning,"IDEAL_BAND_LIMIT",
      "Above selected measured analysis cap, H is set to zero for simulation ONLY; unknown physical response"});
  if(negativeTail/total > 0.02)
    result.diagnostics.push_back({Severity::Warning,"PRECURSOR_ENERGY",
      "Circular impulse has significant negative-time/precursor tail; causality not qualified"});
  if(active)
    result.diagnostics.push_back({Severity::Warning,"GAIN_ABOVE_UNITY",
      "Transfer magnitude exceeds unity; passivity has not been independently verified"});
  if(!data.samplingPhaseThresholdOpen && data.eyeWidthUI>0)
    result.diagnostics.push_back({Severity::Warning,"MAX_HEIGHT_PHASE_NOT_THRESHOLD_OPEN",
      "Maximum separation phase does not satisfy the declared threshold; not a valid decision phase"});
  if(data.eyeHeightV<=0 || data.eyeWidthUI==0)
    result.diagnostics.push_back({Severity::Warning,"CLOSED_EYE",
      "Simulated eye is closed at declared RX threshold; no numerical pass/fail compliance"});
  result.diagnostics.push_back({Severity::Info,"PERIODIC_FRAME",
    "Finite repeated FFT frame; sampled width and pattern-dependent separation, not continuous-time or statistical eye"});
  result.value=std::move(data);
  return result;
}
} // namespace si::channel
