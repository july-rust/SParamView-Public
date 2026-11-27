#pragma once
// 012_09: historical-source causal package + real physical S4P channel
// (diagnostic circuit only; not normative 802.3ck COM dB).
#include "si/channel_analysis/com_causal_package.hpp"
#include "si/channel_analysis/com_physical_channel.hpp"

namespace si::channel {
struct ComCausalPhysicalInput {
  std::vector<double> frequencyHz;
  std::vector<ComPhysicalFourPort> boardS4p;
  double maxAllowedModeConversion = 0.0;
};

// Apply both historical 802.3ck C2C reference package lengths (TX 13/31,
// RX 11/29 mm) to the full, bidirectional complex S4P board network.
// This explicitly differs from the legacy attenuation-only diagnostic path:
// causal gamma contains a1 phase and a2 log dispersion, and package Zc is
// already DIFFERENTIAL. Result.status=Warning, diagnostic NOT_IEEE_COM.
// No numeric COM dB or standard PASS/FAIL is produced.
Result<ComPhysicalChannelData> calculateHistoricalCausalC2cCases(
    const ComCausalPhysicalInput &input, Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);

// Read all 16 physical Sij from SHA-pinned SourceSnapshot and map to both
// historical causal package cases. Caller must verify snapshot still matches
// live workspace identity before displaying/caching result.
Result<ComPhysicalChannelData> calculateHistoricalCausalC2cFromSource(
    const SourceSnapshot &source, std::string_view victimChannelId,
    double maxAllowedModeConversion = 0,
    Control *control = nullptr);
} // namespace si::channel
