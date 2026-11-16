#pragma once
// 012_09: reference-equation causal package physics (not a COM margin).
// Distinguishes differential characteristic impedance Zc from the
// *single-ended* die capacitance and inductance values.
#include "si/channel_analysis/s_network.hpp"

namespace si::channel {
enum class ComCausalPackageSide { TxDieToBoard, RxBoardToDie };
struct ComCausalPackageParameters {
  double cdSingleEndedFarad = 0;
  double lsSingleEndedHenry = 0;
  double cbSingleEndedFarad = 0;
  double cpSingleEndedFarad = 0;
  double primaryLengthMm = 0, secondaryLengthMm = 0;
  double primaryZcDifferentialOhm = 0, secondaryZcDifferentialOhm = 0;
  // Causal-by-design gamma(f), with f_GHz expressed numerically:
  // gamma = gamma0 + a1*(1+j)*sqrt(f_GHz)
  //   + [a2*(1-j*(2/pi)*log(f_GHz)) + j*2*pi*tau_ns]*f_GHz.
  // Units: gamma0 1/mm; a1 sqrt(ns)/mm; a2 ns/mm; tau ns/mm.
  // At f=0, gamma=gamma0. NOT the older attenuation-only model.
  double gamma0PerMm = 0, a1SqrtNsPerMm = 0, a2NsPerMm = 0;
  double tauNsPerMm = 0;
};

// Exact causal model used by Annex 93A reference package transmission-line
// studies. Zc and Zref are both DIFFERENTIAL ohms for this function.
// Returns complex propagation per mm and rejects non-finite/non-passive input.
Complex comCausalGammaPerMm(const ComCausalPackageParameters &p,
                             double frequencyHz);

// Analytic reciprocal wave scattering of the causal line, including
// mismatched-Zc reflections and arbitrary complex phase. This routine
// avoids high-loss cosh cancellation and is NOT COM dB.
ComWaveTwoPort comCausalLine(const ComCausalPackageParameters &p,
                             double frequencyHz, double lineLengthMm,
                             double lineZcDifferentialOhm,
                             double zrefDifferentialOhm = 100.0);

// Package die->board or board->die ladder, with Cd/Ls/Cb and Cp supplied
// PER SINGLE-ENDED LEG, but both transmission-line impedances supplied
// DIFFERENTIALLY. The die resistor Rd is outside this package network.
ComWaveTwoPort calculateComCausalPackage(
    const ComCausalPackageParameters &p,
    double frequencyHz, ComCausalPackageSide side,
    double physicalSingleEndedReferenceOhm = 50.0);

// Historical IEEE-hosted 2022 802.3ck SA 120F C2C parameter candidate.
// Cross-check with Ansys published COM3.40 config and IEEE working-group
// slides only; NOT verified complete final 802.3ck-2022 Table120F-8.
// Must never be used to label output as a qualified COM or PHY verdict.
ComCausalPackageParameters historical8023ckC2cCausalCandidate(
    double primaryLengthMm = 0.0);
} // namespace si::channel
