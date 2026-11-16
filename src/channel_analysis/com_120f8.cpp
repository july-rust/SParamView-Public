#include "si/channel_analysis/com_120f8.hpp"
#include <algorithm>
#include <cmath>

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
