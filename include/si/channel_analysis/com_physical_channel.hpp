#pragma once
// 012_09: actual S4P -> mixed-mode -> TX package -> board -> RX package
// physical channel circuit. This is NOT Annex 93A COM dB or certification.
#include "si/channel_analysis/common.hpp"
#include "si/channel_analysis/package.hpp"
#include <atomic>
#include <cstddef>
#include <vector>

namespace si::channel {

// The four physical port order is nearP,nearN,farP,farN. At every frequency
// all four reference impedances must be real, positive and identical to the
// other frequency samples. Mixed-mode conversion is rejected by default.
struct ComPhysicalChannelInput {
  std::vector<double> frequencyHz;
  std::vector<ComPhysicalFourPort> boardS4p;
  ComPackageCircuit transmitter, receiver;
  double maxAllowedModeConversion = 0.0;
};

// Each response retains forward/reverse phase and reflections. There is no
// insertion-loss-only approximation or scalar THRU multiplication.
struct ComPhysicalChannelCase {
  ComPackageCase packageCase = ComPackageCase::Test1;
  std::vector<ComTwoPortS> loadedSdd;
  double txPrimaryLineMm = 0, rxPrimaryLineMm = 0;
};
struct ComPhysicalChannelData {
  std::vector<double> frequencyHz;
  ComPhysicalChannelCase test1, test2;
  double referenceOhmDifferential = 0;
  double maxObservedModeConversion = 0;
  const char* model = "EXPLORATORY_S4P_TX_BOARD_RX_PHYSICAL_CASCADE";
};

// Use the only two distinct nominal TX/RX primary line lengths from the
// 802.3ck 120F C2C working profile; every other package component must be
// supplied EXPLICITLY, not silently inferred. Input primaryLineMm MUST be
// zero to acknowledge override by the test-case selector.
// This is an *exploratory physical network*, NOT an official IEEE package
// propagation model, COM ratio, nor a 3 dB pass/fail verdict.
Result<ComPhysicalChannelData> calculateComPhysicalTestCases(
    const ComPhysicalChannelInput &input, Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);

} // namespace si::channel
