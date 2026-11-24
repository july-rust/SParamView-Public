#pragma once
// 012_41: bounded SYNTHETIC search qualification only; NOT_IEEE_COM.
#include "si/channel_analysis/com_eq_candidate_contract.hpp"
#include <functional>

namespace si::channel {
enum class ComEqSearchMode { Exhaustive, Local };
enum class ComEqSearchEnd {
  SearchSpaceExhausted, NoFurtherImprovement, EvaluationBudgetExceeded,
  IterationBudgetExceeded, Cancelled, InvalidInput, StaleSource,
  NumericalFailure, NoValidCandidate
};
struct ComEqSearchPosition {
  size_t ordinal = 0;
  int offset = 0;
  bool operator==(const ComEqSearchPosition&) const = default;
};
struct ComEqSearchSpec {
  ComEqSearchMode mode = ComEqSearchMode::Exhaustive;
  // Explicit ascending unique raw ordinals; phase order from 012_40.
  std::vector<size_t> ordinals;
  ComEqPhaseContract phaseContract;
  std::optional<ComEqCtleMask> mask;
  ComEqSearchPosition initial;
  size_t evaluationBudget = 0, iterationBudget = 0;
  std::string objectiveId; // explicitly synthetic; not a physical FOM
};
struct ComEqSyntheticScore {
  ComEqCandidateKey key;
  std::string objectiveId;
  double value = 0.;
};
struct ComEqSearchVisit {
  ComEqSearchPosition position;
  ComEqSyntheticScore score;
};
struct ComEqSearchQualification {
  ComEqSearchEnd end = ComEqSearchEnd::InvalidInput;
  size_t attempted = 0, rejected = 0, iterations = 0;
  std::vector<ComEqSearchPosition> evaluationOrder;
  // Only present on successful termination; no partial optimum on failure.
  std::optional<ComEqSearchVisit> initial, best;
  std::string model = "NOT_IEEE_COM_SYNTHETIC_EQ_SEARCH";
};
using ComEqLiveIdentity = std::function<SourceIdentity()>;
using ComEqSyntheticEvaluator = std::function<ComEqSyntheticScore(const ComEqCandidateKey&)>;
// Synchronous engineering-only search. Local neighborhood = exactly one
// discrete index +/-1 or phase offset +/-1; no diagonal moves. Local scans
// neighbors in canonical order and moves to the strictly highest score.
// Evaluations are cached by unique position within one immutable call.
ComEqSearchQualification qualifyComEqSearch(const SourceSnapshot& source,
    std::string_view victimId, const Provenance& provenance,
    std::string_view testCase, std::string_view referenceVersion,
    std::string_view referenceCommit, const ComEqSearchSpec& spec,
    const ComEqLiveIdentity& live, const ComEqSyntheticEvaluator& evaluate,
    const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
