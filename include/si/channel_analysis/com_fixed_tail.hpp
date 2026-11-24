#pragma once
#include "si/channel_analysis/com_fixed_fom.hpp"
#include "si/channel_analysis/com_joint_tail.hpp"

namespace si::channel {
// Only explicit synthetic mathematical distributions, not a PHY reference.
enum class ComFixedTailRole { ResidualIsi, Next, Fext, SampledDj };
struct ComFixedTailTap {
  ComFomCaseIdentity identity;
  ComFixedTailRole role;
  std::string owner, evidence;
  double voltageV;
};
struct ComFixedTailInput {
  ComFixedFomInput fom;
  std::string calibrationIdentity, rjOwner, rjEvidence;
  double rjRmsV=std::numeric_limits<double>::quiet_NaN();
  bool rjExplicitlyInactive=false;
  bool independentGaussianRxTxRjDeclared=false;
  std::vector<ComFixedTailTap> taps;
  double oneSidedProbability=std::numeric_limits<double>::quiet_NaN();
};
struct ComFixedTailData {
  ComFomCaseIdentity identity;
  double oneSidedProbability=0.;
  double txRmsV=0., gaussianRmsV=0.;
  ComJointTailData tail;
  // Deliberately no official A_ni, DER, or COM output.
  std::string model="SYNTHETIC_CONTINUOUS_TAIL_NOT_IEEE_COM";
};
Result<ComFixedTailData> calculateComFixedTailSynthetic(
    const ComFixedTailInput&,const Provenance&,const std::atomic_bool* cancelled=nullptr);
Result<ComFixedTailData> assessComFixedTailSource(
    const Result<ComFixedNoiseData>&,std::string_view,Control* control=nullptr);

// Standalone diagnostic for an already supplied complete synthetic grid of
// probability masses. Reference-style strict lower-CDF crossing only; no
// claim that masses were produced by the reference PDF generation procedure.
struct ComTailGridCrossing {
  size_t index=0;
  double signedVoltageV=0., magnitudeV=0., cdfBefore=0., cdfAt=0.;
  std::string model="SYNTHETIC_STRICT_LOWER_CDF_NOT_IEEE_COM";
};
ComTailGridCrossing syntheticStrictLowerCdfCrossing(
    const std::vector<double>& masses,int firstBin,double binV,double probability,
    const std::atomic_bool* cancelled=nullptr);
} // namespace si::channel
