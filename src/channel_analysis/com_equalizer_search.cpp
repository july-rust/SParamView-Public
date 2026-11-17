#include "si/channel_analysis/com_equalizer_search.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace si::channel {
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw si::Error(message);
}
void checkCancelled(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
bool trustedSource(const Provenance& p) {
  return p.sourceSha256.size() == 64 &&
         std::all_of(p.sourceSha256.begin(), p.sourceSha256.end(),
           [](unsigned char c) { return std::isxdigit(c) != 0; }) &&
         !p.revision.empty() && !p.mappingIdentity.empty() &&
         !p.settingsIdentity.empty() && p.workspaceGeneration != 0 &&
         p.mappingGeneration != 0;
}
bool sameChoice(const Com120F8EqualizerChoice& a,
                const Com120F8EqualizerChoice& b) {
  return a.txFfe == b.txFfe &&
         a.ctleDcGainDb == b.ctleDcGainDb &&
         a.ctleDcGain2Db == b.ctleDcGain2Db;
}
double derivedCursor(const std::array<double,5>& taps) {
  double total = 0;
  for (size_t i=0; i<taps.size(); ++i)
    if (i != 3) total += std::abs(taps[i]);
  return 1. - total;
}
std::string choiceIdentity(const Com120F8EqualizerChoice& choice) {
  const auto profile = final8023ck120F8();
  std::string id = "|eq=120F8";
  const std::array<size_t,4> slot{0,1,2,4};
  for (size_t i=0; i<4; ++i) {
    const auto r = profile.txNoncursor[i];
    id += "-f" + std::to_string(static_cast<long long>(
          std::llround((choice.txFfe[slot[i]]-r.minimum)/r.step)));
  }
  id += "-g" + std::to_string(static_cast<long long>(
        std::llround((choice.ctleDcGainDb-profile.ctleDcGainDb.minimum)/
                      profile.ctleDcGainDb.step)));
  id += "-h" + std::to_string(static_cast<long long>(
        std::llround((choice.ctleDcGain2Db-profile.ctleDcGain2Db.minimum)/
                      profile.ctleDcGain2Db.step))) + "|";
  return id;
}
ComEqualizerSearchCandidate evaluate(
    const ComPhysicalChannelData& physical,
    const Com120F8EqualizerChoice& choice,
    const std::string& testCase, double decisionTimeSeconds,
    Provenance base, const std::atomic_bool* cancelled) {
  checkCancelled(cancelled);
  validateCom120F8Equalizer(choice);
  base.settingsIdentity += choiceIdentity(choice);
  const auto loaded = prepareCom120F8LoadedTestCases(
      physical, choice, base, cancelled);
  require(loaded.usable(), "012_18: conditioned physical channel unavailable");
  const auto& conditioned = testCase == "TEST1"
      ? loaded.value->test1 : loaded.value->test2;
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  std::vector<double> times;
  times.reserve(13);
  for (int k=-3; k<=9; ++k) {
    const double t = decisionTimeSeconds + static_cast<double>(k)*ui;
    require(std::isfinite(t) && t >= 0,
            "012_18: UI window before zero or nonfinite");
    times.push_back(t);
  }
  const auto pulse = calculateCom120F8RectangularPulse(
      conditioned, times, base, cancelled);
  require(pulse.usable() && pulse.value->unitSymbolPulse.size() == 13,
          "012_18: rectangular SYMBOL pulse unavailable");
  const auto& symbols = pulse.value->unitSymbolPulse;
  const double cursor = symbols[3];
  double largest = 0;
  for (double v: symbols) {
    require(std::isfinite(v), "012_18: nonfinite symbol amplitude");
    largest = std::max(largest, std::abs(v));
  }
  require(std::isnormal(cursor) && std::isnormal(largest) &&
          std::abs(cursor) > 1e-10 * largest,
          "012_18: ill-conditioned symbol cursor");
  std::array<double,13> normalized{};
  for (size_t i=0; i<normalized.size(); ++i) {
    normalized[i] = symbols[i] / cursor;
    require(std::isfinite(normalized[i]),
            "012_18: normalized symbol overflow");
  }
  const auto taps = minimizeCom120F8IdealDfeResidual(normalized);
  ComDfeInput dfeInput;
  dfeInput.symbolPulse = pulse;
  dfeInput.taps = taps;
  dfeInput.testCase = testCase;
  const auto compensated = calculateComDfe(dfeInput, base, cancelled);
  require(compensated.usable(), "012_18: DFE evaluation unavailable");
  ComEqualizerSearchCandidate candidate;
  candidate.equalizer = choice;
  candidate.dfeTaps = taps;
  candidate.cursorAmplitudeUnitSymbol =
      compensated.value->cursorAmplitudeUnitSymbol;
  candidate.precursorRss = compensated.value->precursorRssAfter;
  candidate.postcursorRss = compensated.value->postcursorRssAfter;
  candidate.residualScore = std::hypot(candidate.precursorRss,
                                       candidate.postcursorRss);
  require(std::isfinite(candidate.residualScore),
          "012_18: nonfinite objective");
  return candidate;
}
} // namespace

std::array<double,6> minimizeCom120F8IdealDfeResidual(
    const std::array<double,13>& normalized) {
  for (double v: normalized)
    require(std::isfinite(v), "012_18: nonfinite normalized DFE sample");
  require(normalized[3] == 1.,
          "012_18: requires unit-normalized main cursor index 3");
  const auto p = final8023ck120F8();
  std::array<double,6> taps{};
  for (size_t k=0; k<taps.size(); ++k)
    taps[k] = std::clamp(normalized[k+4],p.dfeMin[k],p.dfeMax[k]);
  validateCom120F8DfeTaps(taps);
  return taps;
}

Result<ComEqualizerSearchData> searchCom120F8LocalEqualizer(
    const ComPhysicalChannelData& physical,
    const Com120F8EqualizerChoice& initial,
    const std::string& testCase, double decisionTimeSeconds,
    Provenance provenance, const std::atomic_bool* cancelled) {
  checkCancelled(cancelled);
  require(testCase == "TEST1" || testCase == "TEST2",
          "012_18: only TEST1 or TEST2 accepted");
  require(trustedSource(provenance),
          "012_18: invalid or incomplete source provenance");
  const std::string requiredCase = "|case=" + testCase + "|";
  const std::string wrongCase = testCase == "TEST1"
      ? "|case=TEST2|" : "|case=TEST1|";
  require(provenance.settingsIdentity.find(requiredCase) != std::string::npos &&
          provenance.settingsIdentity.find(wrongCase) == std::string::npos &&
          provenance.settingsIdentity.find("|eq=") == std::string::npos,
          "012_18: case or equalizer identity is ambiguous");
  require(physical.model != nullptr && std::string_view(physical.model) ==
          "EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P",
          "012_18: physical source model is not qualified for this bridge");
  validateCom120F8Equalizer(initial);
  validateCom120F8DirectGrid(physical.frequencyHz);
  // Explicit, conservative budget for the 13 x 13 symbol quadratures.
  // Other existing FFE/physical conditioning work also remains O(N x 13).
  constexpr size_t maxCandidates=13, samples=13, maxQuadratureCells=6000000;
  constexpr size_t maxBins=32769;
  const size_t bins = physical.frequencyHz.size();
  require(bins <= maxBins && bins <= maxQuadratureCells /
                 (maxCandidates*samples),
          "012_18: bounded pulse search operation budget exceeded");
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  require(std::isfinite(decisionTimeSeconds) &&
          decisionTimeSeconds >= 3.*ui &&
          (decisionTimeSeconds+9.*ui) *
              (physical.frequencyHz[1]-physical.frequencyHz[0]) <= 0.45,
          "012_18: decision window invalid or beyond band delay support");
  checkCancelled(cancelled);
  ComEqualizerSearchData data;
  data.testCase = testCase;
  data.decisionTimeSeconds = decisionTimeSeconds;
  data.attempted = maxCandidates;
  data.initial = evaluate(physical,initial,testCase,decisionTimeSeconds,
                          provenance,cancelled);
  data.best = data.initial;
  data.evaluated=1;

  const auto p = final8023ck120F8();
  const std::array<size_t,4> slots{0,1,2,4};
  std::vector<Com120F8EqualizerChoice> seen{initial};
  for (size_t axis=0; axis<6; ++axis) {
    for (const int direction: {-1,+1}) {
      checkCancelled(cancelled);
      auto candidate = initial;
      Com120F8DiscreteRange r{};
      double baseValue = 0.;
      if (axis<4) {
        r = p.txNoncursor[axis];
        baseValue = candidate.txFfe[slots[axis]];
      } else if (axis==4) {
        r = p.ctleDcGainDb;
        baseValue = candidate.ctleDcGainDb;
      } else {
        r = p.ctleDcGain2Db;
        baseValue = candidate.ctleDcGain2Db;
      }
      const long long index = std::llround((baseValue-r.minimum)/r.step);
      const long long next = index + direction;
      const long long last = std::llround((r.maximum-r.minimum)/r.step);
      const std::string label = "axis=" + std::to_string(axis) +
            ",step=" + std::to_string(direction);
      if (next<0 || next>last) {
        ++data.rejected;
        data.rejectionReasons.push_back(label+":OUT_OF_RANGE");
        continue;
      }
      const double nextValue = r.minimum+static_cast<double>(next)*r.step;
      if (axis<4) {
        candidate.txFfe[slots[axis]] = nextValue;
        candidate.txFfe[3] = derivedCursor(candidate.txFfe);
      } else if (axis==4) {
        candidate.ctleDcGainDb = nextValue;
      } else {
        candidate.ctleDcGain2Db = nextValue;
      }
      if (std::any_of(seen.begin(),seen.end(),
          [&](const auto& old){return sameChoice(old,candidate);})) {
        ++data.rejected;
        data.rejectionReasons.push_back(label+":DUPLICATE");
        continue;
      }
      seen.push_back(candidate);
      try {
        validateCom120F8Equalizer(candidate);
      } catch (const si::Error&) {
        ++data.rejected;
        data.rejectionReasons.push_back(label+":INVALID_EQUALIZER");
        continue;
      }
      // The baseline must pass first. A candidate-specific numerical
      // conditioning failure is recorded; cancellation is never swallowed.
      try {
        auto evaluated = evaluate(physical,candidate,testCase,
                                  decisionTimeSeconds,provenance,cancelled);
        ++data.evaluated;
        const double tolerance = 1e-12 *
                        std::max(1.,std::abs(data.best.residualScore));
        if (evaluated.residualScore < data.best.residualScore-tolerance)
          data.best = std::move(evaluated);
      } catch (const si::Error& e) {
        ++data.rejected;
        data.rejectionReasons.push_back(label+":NUMERIC_REJECTED:"+e.what());
      }
    }
  }
  require(data.evaluated >= 1 &&
          data.evaluated + data.rejected == data.attempted,
          "012_18: inconsistent bounded candidate accounting");
  Result<ComEqualizerSearchData> result;
  result.status=Status::Warning;
  result.value=std::move(data);
  result.provenance=std::move(provenance);
  result.provenance.algorithm="com-120f8-ideal-dfe-local-isi-search-exploratory";
  result.provenance.version="012_18-component-1";
  result.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Bounded one-step, fixed-decision-phase ISI-only local equalizer search. "
    "No full 93A optimization, RX noise, jitter, XTALK probability, "
    "COM dB, BER, DER or PHY PASS/FAIL."});
  return result;
}
} // namespace si::channel
