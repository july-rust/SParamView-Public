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
}
