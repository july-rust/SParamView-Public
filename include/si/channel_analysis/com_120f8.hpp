#pragma once
// Numeric contract for the PUBLISHED IEEE Std 802.3ck-2022 Annex 120F,
// Table 120F-8 (100GAUI-1/200GAUI-2/400GAUI-4 C2C).
// This file records parameter values only. It is NOT an Annex 93A COM solver
// or evidence that a measured channel qualifies for PHY PASS/FAIL.
// Primary standard: https://ieeexplore.ieee.org/document/9999414
#include "si/channel_analysis/common.hpp"
#include <array>
#include <vector>

namespace si::channel {
struct Com120F8DiscreteRange {
  double minimum, maximum, step;
};

struct Com120F8Parameters {
  double symbolRateHz = 53.125e9;
  double maximumStartFrequencyHz = 0.05e9;
  double maximumFrequencyStepHz = 0.01e9;
  double minimumComDb = 3.0;
  double txTransitionTimeS = 7.5e-12; // Annex 120F.4.1, Ht(f)
  double referenceOhmSingleEnded = 50.0;
  double receiverTerminationOhmSingleEnded = 50.0;
  double cdFaradSingleEnded = 1.2e-13;
  double lsHenrySingleEnded = 1.2e-10;
  double cbFaradSingleEnded = 3.0e-14;
  double cpFaradSingleEnded = 8.7e-14;
  std::array<double, 2> txPackageLengthMm{13.,31.};
  std::array<double, 2> rxPackageLengthMm{11.,29.};
  double secondaryPackageLengthMm = 1.8;
  double primaryPackageZcOhm = 87.5;
  double secondaryPackageZcOhm = 92.5;
  double packageA1SqrtNsPerMm = 9.909e-4;
  double packageA2NsPerMm = 2.772e-4;
  double receiverBandwidthHz = 0.75*53.125e9;
  // Ordering: pre(-3), pre(-2), pre(-1), post(+1). c(0) separately.
  std::array<Com120F8DiscreteRange,4> txNoncursor{
      Com120F8DiscreteRange{-0.04,0.0,0.02},
      Com120F8DiscreteRange{0.0,0.10,0.02},
      Com120F8DiscreteRange{-0.28,0.0,0.02},
      Com120F8DiscreteRange{-0.10,0.0,0.02}};
  double minimumCursor = 0.54;
  Com120F8DiscreteRange ctleDcGainDb{-20.,0.,1.};
  Com120F8DiscreteRange ctleDcGain2Db{-4.,0.,1.};
  double ctleZeroFrequencyHz = 53.125e9/2.5;
  double ctlePole1FrequencyHz = 53.125e9/2.5;
  double ctlePole2FrequencyHz = 53.125e9;
  double ctleLowFrequencyPoleZeroHz = 53.125e9/80.;
  double victimPeakV = 0.413;
  double fextPeakV = 0.413;
  double nextPeakV = 0.608;
  int pamLevels = 4;
  double minimumRlm = 0.95;
  double txSnrDb = 33.;
  int samplesPerUi = 32;
  int dfeTaps = 6;
  std::array<double,6> dfeMin{0.3,0.05,-0.04,-0.04,-0.04,-0.04};
  std::array<double,6> dfeMax{0.65,0.15,0.1,0.1,0.1,0.1};
  double randomJitterRmsUi = 0.01;
  double dualDiracJitterPeakUi = 0.02;
  double oneSidedNoiseSpectralDensityV2PerGHz = 2e-8;
  double targetDetectorErrorRatio = 1e-5;
};

// Immutable by value. A different PHY or revision MUST use a distinct type
// and audit; do not silently accept draft-era configuration overrides.
constexpr Com120F8Parameters final8023ck120F8() { return {}; }

struct Com120F8EqualizerChoice {
  // Index 0..4 = c(-3), c(-2), c(-1), c(0), c(+1).
  std::array<double,5> txFfe{0.,0.,0.,1.,0.};
  double ctleDcGainDb = 0.;
  double ctleDcGain2Db = 0.;
};

// Validates ONLY documented discrete TX/CTLE ranges, NOT the PHY COM result.
// Bounds and quantization are strict; no rounding of off-grid choices.
void validateCom120F8Equalizer(const Com120F8EqualizerChoice& choice);

// Checks sample identity for a direct grid (starting at/below fmin, maximum
// step and Nyquist coverage). Interpolation/extrapolation is NOT performed.
// This is a deliberately strict subset of usable COM inputs, not the complete
// reference implementation's frequency handling.
void validateCom120F8DirectGrid(const std::vector<double>& frequencyHz);

} // namespace si::channel
