#include "si/channel_analysis/icn.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>

namespace si::channel {
namespace {
void require(bool ok, const char *reason) { if (!ok) throw Error(reason); }
void check(const std::atomic_bool *cancel) {
  if (cancel && cancel->load(std::memory_order_relaxed)) throw Cancelled();
}
double sinc(double x) {
  return std::abs(x) < 1e-7 ? 1.0 - x*x/6.0 : std::sin(x)/x;
}
double interpolatePower(const ComplexTrace &trace, double f) {
  const auto it = std::lower_bound(trace.frequencyHz.begin(),
                                   trace.frequencyHz.end(), f);
  const size_t i = size_t(it-trace.frequencyHz.begin());
  if (i==0 || (i<trace.frequencyHz.size() && *it==f))
    return std::norm(trace.transfer[i]);
  require(i<trace.frequencyHz.size(), "ICN frequency outside measured source");
  const double a = (f-trace.frequencyHz[i-1]) /
                   (trace.frequencyHz[i]-trace.frequencyHz[i-1]);
  const Complex h = (1.0-a)*trace.transfer[i-1]+a*trace.transfer[i];
  return std::norm(h);
}
double oneSidedTxPsd(double f, const IcnSettings &s) {
  // Zero-mean, equiprobable +/- Vpp/2, independent NRZ symbols:
  // S_one-sided(f) = 2*sigma_V^2*T*sinc(pi*f*T)^2 (V^2/Hz).
  // Gaussian edge model has sigma_t = t_10_90 / 2.563103.
  const double t = 1.0/s.symbolRateHz;
  const double x = std::numbers::pi * f * t;
  const double edgeSigma = s.riseTimeSeconds/2.5631031310892007;
  const double rolloff = std::exp(-std::pow(2.0*std::numbers::pi*f*edgeSigma,2));
  const double v = 0.5*s.txPeakToPeakV;
  const double rx = 1.0/(1.0+std::pow(f/s.rxBandwidthHz,2));
  return 2.0*v*v*t*std::pow(sinc(x),2)*rolloff*rx;
}
double nonnegative(double x) {
  require(std::isfinite(x) && x>=0, "Nonfinite or negative ICN noise power");
  return x;
}
} // namespace

IcnSettings automaticIcnSettings(const ComplexTrace &trace) {
  validateTrace(trace);
  IcnSettings settings;
  // Request 0 Hz to preserve the missing-DC/band-limited provenance; the
  // existing engine intersects the integral with the measured range.
  settings.startHz=0.;
  settings.stopHz=std::min(10e9,trace.frequencyHz.back());
  require(settings.stopHz>trace.frequencyHz.front(),"No measured overlap with Auto ICN analysis cap");
  settings.symbolRateHz=std::min(4.5e9,settings.stopHz/1.5);
  settings.txPeakToPeakV=0.4;
  settings.riseTimeSeconds=30e-12;
  settings.rxBandwidthHz=settings.stopHz;
  return settings;
}

Result<IcnData> calculateIcn(const std::vector<IcnAggressorInput> &aggressors,
                             const IcnSettings &s, Provenance provenance,
                             const std::atomic_bool *cancelled) {
  check(cancelled);
  require(!aggressors.empty() && aggressors.size()<=32,
          "ICN requires 1..32 distinct aggressor paths");
  require(std::isfinite(s.startHz) && std::isfinite(s.stopHz) &&
          s.startHz>=0 && s.stopHz>s.startHz, "Invalid ICN integration band");
  require(std::isfinite(s.symbolRateHz) && s.symbolRateHz>0 &&
          std::isfinite(s.txPeakToPeakV) && s.txPeakToPeakV>0 &&
          std::isfinite(s.riseTimeSeconds) && s.riseTimeSeconds>=0 &&
          std::isfinite(s.rxBandwidthHz) && s.rxBandwidthHz>0,
          "Invalid ICN TX/RX settings; absolute Vpp, baud and bandwidth required");
  std::set<std::pair<std::string,IcnEnd>> unique;
  const auto &first = aggressors.front().transfer;
  validateTrace(first);
  for (const auto &a : aggressors) {
    check(cancelled);
    require(a.end==IcnEnd::Next || a.end==IcnEnd::Fext, "Invalid ICN path type");
    require(!a.id.empty() && unique.insert({a.id,a.end}).second,
            "Duplicate or empty ICN aggressor path");
    validateTrace(a.transfer);
    require(a.transfer.frequencyHz==first.frequencyHz,
            "ICN paths require identical qualified frequency grids");
    const double referenceError=std::abs(a.transfer.stimulusOhm-a.transfer.responseOhm);
    require(referenceError<=1e-9*std::max(a.transfer.stimulusOhm,a.transfer.responseOhm),
            "ICN source and victim impedance differ: renormalization required");
  }
  require(first.frequencyHz.size()<=262144 &&
          first.frequencyHz.size()*aggressors.size()<=2097152,
          "ICN resource budget exceeded");
  const double start=std::max(s.startHz,first.frequencyHz.front());
  const double stop=std::min(s.stopHz,first.frequencyHz.back());
  require(stop>start, "ICN selected band has no measured overlap");
  IcnData d;
  d.settings=s;
  d.actualStartHz=start;
  d.actualStopHz=stop;
  d.bandLimited = start!=s.startHz || stop!=s.stopHz || start>0;
  d.frequencyHz.push_back(start);
  for (double f:first.frequencyHz)
    if (f>start && f<stop) d.frequencyHz.push_back(f);
  d.frequencyHz.push_back(stop);
  const size_t n=d.frequencyHz.size();
  require(n>=2 && n<=262146, "Invalid ICN frequency sample count");
  d.nextPowerDb.resize(n);
  d.fextPowerDb.resize(n);
  d.nextDensityV2Hz.assign(n,0);
  d.fextDensityV2Hz.assign(n,0);
  d.nextCumulativeRmsV.assign(n,0);
  d.fextCumulativeRmsV.assign(n,0);
  d.totalCumulativeRmsV.assign(n,0);
  d.aggressors.reserve(aggressors.size());
  std::vector<double> nextSum(n,0),fextSum(n,0);
  for (const auto &a:aggressors) {
    check(cancelled);
    IcnAggressorData entry;
    entry.id=a.id;
    entry.end=a.end;
    entry.parameter=a.transfer.parameter;
    entry.spectralDensityV2Hz.reserve(n);
    entry.cumulativeRmsV.assign(n,0);
    for (size_t k=0;k<n;++k) {
      if ((k&1023u)==0u) check(cancelled);
      const double f=d.frequencyHz[k];
      const double h2=nonnegative(interpolatePower(a.transfer,f));
      const double density=nonnegative(h2*oneSidedTxPsd(f,s));
      entry.spectralDensityV2Hz.push_back(density);
      (a.end==IcnEnd::Next?nextSum:fextSum)[k]+=h2;
      (a.end==IcnEnd::Next?d.nextDensityV2Hz:d.fextDensityV2Hz)[k]+=density;
      if(k) {
        const double step=f-d.frequencyHz[k-1];
        const double incremental=nonnegative(
          (entry.spectralDensityV2Hz[k-1]+density)*0.5*step);
        const double power=entry.cumulativeRmsV[k-1]*entry.cumulativeRmsV[k-1]
                           +incremental;
        entry.cumulativeRmsV[k]=std::sqrt(nonnegative(power));
      }
    }
    entry.rmsV=entry.cumulativeRmsV.back();
    d.aggressors.push_back(std::move(entry));
  }
  double nextPower=0,fextPower=0;
  for (size_t k=0;k<n;++k) {
    if ((k&1023u)==0u) check(cancelled);
    // -300 dB is a *display floor*, never a nonzero noise-power substitution.
    d.nextPowerDb[k]=10*std::log10(std::max(nonnegative(nextSum[k]),1e-30));
    d.fextPowerDb[k]=10*std::log10(std::max(nonnegative(fextSum[k]),1e-30));
    nonnegative(d.nextDensityV2Hz[k]*1e15); // plotted mV^2/GHz must be finite
    nonnegative(d.fextDensityV2Hz[k]*1e15);
    if(k) {
      const double df=d.frequencyHz[k]-d.frequencyHz[k-1];
      nextPower+=nonnegative((d.nextDensityV2Hz[k-1]+d.nextDensityV2Hz[k])*0.5*df);
      fextPower+=nonnegative((d.fextDensityV2Hz[k-1]+d.fextDensityV2Hz[k])*0.5*df);
    }
    d.nextCumulativeRmsV[k]=std::sqrt(nonnegative(nextPower));
    d.fextCumulativeRmsV[k]=std::sqrt(nonnegative(fextPower));
    d.totalCumulativeRmsV[k]=std::sqrt(nonnegative(nextPower+fextPower));
  }
  d.nextRmsV=d.nextCumulativeRmsV.back();
  d.fextRmsV=d.fextCumulativeRmsV.back();
  d.totalRmsV=d.totalCumulativeRmsV.back();
  nonnegative(d.totalRmsV*1000); // exported/displayed mV must be finite
  for(auto &a:d.aggressors)
    a.powerPercent=nonnegative(d.totalRmsV>0 ? 100*std::pow(a.rmsV/d.totalRmsV,2):0);
  Result<IcnData> result;
  result.provenance=std::move(provenance);
  result.status=d.bandLimited?Status::Warning:Status::Valid;
  if(d.bandLimited) result.diagnostics.push_back({Severity::Warning,"MEASURED_BAND_ONLY",
    "Band-limited ICN integral; no DC or outside-band extrapolation and no full-band claim"});
  result.diagnostics.push_back({Severity::Info,"GENERIC_NRZ",
    "Generic uncorrelated IID NRZ, one-sided Vpp PSD, Gaussian edge and one-pole RX; NOT IEEE/SNIA compliance"});
  result.value=std::move(d);
  return result;
}
} // namespace si::channel
