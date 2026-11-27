#pragma once
#include "si/channel_analysis/com_fixed_tail.hpp"

namespace si::channel {
// Only a declared synthetic continuous mixture, not qualified PHY data.
struct ComFixedRatioData {
  ComFomCaseIdentity identity;
  std::string calibrationIdentity;
  double signalV=0., tailV=0., probability=0., fomDb=0., ratioDb=0.;
  double tailLowerV=0., tailUpperV=0., ratioLowerDb=0.;
  std::optional<double> ratioUpperDb;
  std::string numericalQualification;
  std::string model="SYNTHETIC_FIXED_EQ_RATIO_NOT_IEEE_COM";
};
// Recompute both upstream models from one input. No caller-supplied
// precomputed score, tail, or discrete CDF crossing may be substituted.
// Throws Error/Cancelled for invalid data. Live identity is a consistency
// check only; declared synthetic provenance does not establish calibration.
Result<ComFixedRatioData> calculateComFixedRatioSynthetic(
    const ComFixedTailInput&,const Provenance&,const SourceIdentity& live,
    const std::atomic_bool* cancelled=nullptr);
Result<ComFixedRatioData> assessComFixedRatioSource(
    const Result<ComFixedNoiseData>&,std::string_view,Control* control=nullptr);
} // namespace si::channel
