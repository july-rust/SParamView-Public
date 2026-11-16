#include "si/channel_analysis/com_120f8.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

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
  const double cursor=choice.txFfe[3];
  require(std::isfinite(cursor) && cursor>=p.minimumCursor && cursor<=1.,
          "Final IEEE 120F-8 TX FFE cursor violates limits");
  gridValue(choice.ctleDcGainDb,p.ctleDcGainDb,
            "Final IEEE 120F-8 CTLE DC gain outside discrete bounds");
  gridValue(choice.ctleDcGain2Db,p.ctleDcGain2Db,
            "Final IEEE 120F-8 CTLE second DC gain outside discrete bounds");
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
} // namespace si::channel
