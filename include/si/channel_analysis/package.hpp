#pragma once
// C++20 device-package circuit for IEEE COM algorithm research.
// Physical model, not a fully qualified IEEE 802.3ck Annex 120F package.
#include "si/channel_analysis/s_network.hpp"

namespace si::channel {
enum class ComPackageSide { TxDieToBoard, RxBoardToDie };
struct ComPackageCircuit {
  // Values are supplied per single-ended conductor, not differential.
  // The differential equivalent of two balanced uncoupled conductors uses
  // Zdiff=2*Zsingle, Ldiff=2*Lsingle, Ydiff=Ysingle/2.
  double cdFarad = 0, cbFarad = 0, cpFarad = 0, seriesInductanceHenry = 0;
  double primaryLineMm = 0, secondaryLineMm = 0;
  double primaryLineSingleEndedOhm = 0, secondaryLineSingleEndedOhm = 0;
  double attenuationSqrtGHzPerMm = 0;
  double attenuationGHzPerMm = 0;
  double delaySecondsPerMm = 0;
};
// Applies physically explicit, balanced single-ended ladder:
//  TX: Cd -> Ls -> Cb -> primary line -> secondary line -> Cp
//  RX: Cp -> secondary line -> primary line -> Cb -> Ls -> Cd
// Result is a differential 2-port with Zref=2*physicalReferenceOhm.
// Device Rd/driver and receiver termination is EXTERNAL to this network.
// No forced 802.3ck numeric values, and no COM dB or compliance output.
ComWaveTwoPort calculateBalancedComPackage(
    const ComPackageCircuit &circuit, double frequencyHz,
    ComPackageSide side, double physicalReferenceOhm = 50.0);
} // namespace si::channel
