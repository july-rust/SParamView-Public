#pragma once
#include "si/core.hpp"
#include <algorithm>

namespace si {
inline bool hasDirectionalEndpoints(const std::vector<Channel> &channels) {
  return std::any_of(channels.begin(), channels.end(), [](const Channel &c) {
    return c.farP >= 0 || c.farN >= 0;
  });
}

// PowerSI .DiffChannels is handled as explicit endpoint evidence by the caller.
// Touchstone mixed-mode order establishes P/N pairing, not Near/Far direction.
// Label-only two-ended mappings likewise need a human Near/Far confirmation.
inline bool requiresManualDirectionConfirmation(
    const Metadata &meta, const std::vector<Channel> &channels) {
  if (!meta.differentialHints.empty()) return false;
  if (!meta.mixedOrder.empty()) return true;
  return hasDirectionalEndpoints(channels);
}
} // namespace si
