#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace si::gui::detail {
// Presentation only: finite extrema from the current result, never a fixed
// absolute padding on a linear-valued plot with arbitrary signal magnitude.
struct PlotBounds {
  double low=std::numeric_limits<double>::infinity();
  double high=-std::numeric_limits<double>::infinity();
  void include(double value) {
    if(std::isfinite(value)) {
      low=std::min(low,value);
      high=std::max(high,value);
    }
  }
  bool valid() const { return std::isfinite(low)&&std::isfinite(high)&&high>=low; }
  bool pad(double fraction,bool zeroBaseline=false,bool nonnegative=false,double minimumPadding=0.0) {
    if(!valid()) return false;
    if(zeroBaseline) include(0);
    const double span=high-low;
    const double magnitude=std::max(std::abs(low),std::abs(high));
    // A constant or all-zero series needs a finite drawable interval.
    // Relative padding otherwise keeps even very small PSD/RMS data visible.
    const double extra=std::max(minimumPadding,span>0 ? span*fraction :
        magnitude>0 ? magnitude*fraction : 1e-12);
    const double nextLow=low-extra, nextHigh=high+extra;
    if(!std::isfinite(nextLow)||!std::isfinite(nextHigh)) return false;
    low=nonnegative ? std::max(0.0,nextLow) : nextLow;
    high=nextHigh;
    return high>low;
  }
};

// Display-only focus for an isolated first-frequency sample. Never modify
// the fit, deviation, peak, RMS or exported numerical samples. A narrow
// first interval and an eightfold span reduction are both required.
inline bool firstSampleFocus(PlotBounds full, PlotBounds interior,
                             double firstInterval, double visibleSpan,
                             bool zeroBaseline, PlotBounds &focused) {
  if(!full.valid() || !interior.valid() || !(firstInterval>0) ||
     !(visibleSpan>0) || firstInterval>visibleSpan*0.01) return false;
  if(zeroBaseline) {full.include(0);interior.include(0);}
  const double span=full.high-full.low;
  const double detailSpan=std::max(0.1,interior.high-interior.low);
  if(!std::isfinite(span) || span<8*detailSpan) return false;
  if(!interior.pad(0.08,zeroBaseline,false,0.05)) return false;
  if(full.low>=interior.low && full.high<=interior.high) return false;
  focused=interior;
  return true;
}
}
