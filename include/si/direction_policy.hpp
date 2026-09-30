#pragma once
#include "si/core.hpp"

namespace si {
inline bool endpointMapped(const Channel &channel, bool far) {
  const int p = far ? channel.farP : channel.nearP;
  if (p < 0) return false;
  if (!channel.differential()) return true;
  return (far ? channel.farN : channel.nearN) >= 0;
}

inline bool directionAvailable(const Job &job, bool reverse) {
  if (job.metric == Metric::RL || job.metric == Metric::TDR)
    return endpointMapped(job.victim, reverse);
  if (job.metric == Metric::IL)
    return endpointMapped(job.victim, reverse) &&
           endpointMapped(job.victim, !reverse);
  if (job.metric == Metric::NEXT || job.metric == Metric::FEXT) {
    if (!job.aggressor ||
        job.victim.differential() != job.aggressor->differential())
      return false;
    const bool responseFar = job.metric == Metric::NEXT ? reverse : !reverse;
    return endpointMapped(*job.aggressor, reverse) &&
           endpointMapped(job.victim, responseFar);
  }
  return false;
}
} // namespace si
