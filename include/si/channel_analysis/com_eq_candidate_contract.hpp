#pragma once
// 012_40: bounded candidate contract only. NOT_IEEE_COM; no optimizer.
#include "si/channel_analysis/com_equalizer_search.hpp"
#include <optional>
#include <cstdint>
#include <string_view>

namespace si::channel {
inline constexpr std::string_view comEqReferenceVersion = "4.17.0";
inline constexpr std::string_view comEqReferenceCommit =
    "47459695a169fdf1fd10c541a5b19c69a5ae9285";
struct ComEqIndices {
  // Ascending published grid: c(-3),c(-2),c(-1),c(+1),gDC,gDC2.
  std::array<int,6> index{};
  bool operator==(const ComEqIndices&) const = default;
};
enum class ComEqRejection { None, CursorBelowMinimum, CtleExcluded };
struct ComEqCandidate {
  ComEqIndices indices;
  std::optional<Com120F8EqualizerChoice> choice;
  ComEqRejection rejection = ComEqRejection::None;
  // Even an explicitly supplied mask is engineering input, NOT PHY proof.
  bool ctleMaskSupplied = false;
};
using ComEqCtleMask = std::array<bool,105>;
size_t comEqRawCandidateCount();
ComEqCandidate comEqCandidateAt(size_t rawOrdinal,
    const std::optional<ComEqCtleMask>& mask = {},
    const std::atomic_bool* cancelled = nullptr);
ComEqIndices comEqIndicesFor(const Com120F8EqualizerChoice& initial);

enum class ComEqAnchor { MuellerMuller, Peak, MaxDv };
enum class ComEqPhaseOrder { FullSweep, Middle };
struct ComEqPhaseContract {
  // Explicit engineering configuration; no default reference assumption.
  ComEqAnchor anchorMode;
  ComEqPhaseOrder order;
  int firstOffset, lastOffset;
  uint64_t anchorOneBased, sampleCount;
  bool operator==(const ComEqPhaseContract&) const = default;
};
struct ComEqPhase {
  int offset;
  uint64_t zeroBased;
  double secondsFromFirstSample;
  bool operator==(const ComEqPhase&) const = default;
};
// Returns only explicit offsets. No waveform/anchor detection or search.
std::vector<ComEqPhase> comEqPhases(const ComEqPhaseContract& contract,
    size_t callerBudget, const std::atomic_bool* cancelled = nullptr);
// Existing bounded ideal-decision DFE helper; requires explicit supported
// no-quantization/non-floating mode, not inferred from absent settings.
std::array<double,6> comEqDfe(const std::array<double,13>& normalized,
    double dfeDelta, bool floatingDfe);
// Reference strict greater-than; first encountered exact tie is retained.
bool comEqImproves(double score, double previous);
struct ComEqCandidateKey {
  SourceIdentity source;
  std::string victimId, settingsIdentity, testCase, referenceVersion, referenceCommit;
  ComEqIndices indices;
  ComEqPhaseContract phaseContract;
  ComEqPhase phase;
  std::optional<ComEqCtleMask> ctleMask;
  bool operator==(const ComEqCandidateKey&) const = default;
};
// Pure validation helper; observed identity must come from the live workspace.
void validateComEqSource(const SourceIdentity& captured,
    const SourceIdentity& observed, const Provenance& provenance,
    std::string_view testCase, std::string_view referenceVersion,
    std::string_view referenceCommit);
// Binds only to an actual snapshot; validates case, live identity, candidate
// and phase again. Consumers must recheck live identity before use.
ComEqCandidateKey bindComEqCandidate(const SourceSnapshot& source, std::string_view victimId,
    const SourceIdentity& observed, const Provenance& provenance,
    std::string_view testCase, std::string_view referenceVersion,
    std::string_view referenceCommit, size_t rawOrdinal,
    const std::optional<ComEqCtleMask>& mask,
    const ComEqPhaseContract& phaseContract, const ComEqPhase& phase,
    const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
