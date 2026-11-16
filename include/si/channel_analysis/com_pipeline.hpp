#pragma once
// 012_09: actual frequency-resolved S4P -> differential channel -> TX/RX
// package THRU physical cascade, across 802.3ck C2C package test lengths.
// This produces complex, passive matched-wave diagnostics ONLY, not COM dB,
// not the final normative package profile or IEEE 93A/120F compliance.
#include "si/channel_analysis/package.hpp"
#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace si::channel {
struct ComPhysicalThruRequest {
  std::vector<double> frequencyHz;
  // Full S4P wave matrix at each frequency, in nearP,nearN,farP,farN order.
  std::vector<ComPhysicalFourPort> measured;
  // TX and RX explicit circuit parameters. The *primary* lengths are selected
  // by the Test1/Test2 presets; both supplied primaryLineMm MUST be zero.
  ComPackageCircuit txPackage, rxPackage;
  double symbolRateHz = NaN;
  double maximumAllowedModeConversion = 0;
  bool reverse = false;
};
struct ComPhysicalTestResponse {
  ComPackageCase packageCase = ComPackageCase::Test1;
  ComplexTrace thru;
  std::vector<Complex> inputReflection, outputReflection;
  double largestObservedModeConversion = 0;
};
struct ComPhysicalThruData {
  std::array<ComPhysicalTestResponse,2> cases;
  std::string model =
    "EXPLORATORY_COMPLEX_S4P_DIFFERENTIAL_TX_BOARD_RX_PACKAGE_CASCADE";
};
// Actual physical S-parameter result. No COM dB, receiver noise/ISI,
// crosstalk or standard conformance. Not a substitute for full Annex 93A.
Result<ComPhysicalThruData> calculateComPhysicalThru(
    const ComPhysicalThruRequest &request,
    Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);
} // namespace si::channel
