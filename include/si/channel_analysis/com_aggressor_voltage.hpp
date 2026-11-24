#pragma once
// 012_24 — exploratory NEXT/FEXT receiver-equivalent voltage (NOT IEEE COM).
#include "si/channel_analysis/com_interference.hpp"
#include "si/channel_analysis/com_victim_isi.hpp"
#include <array>
#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

struct ComAggressorVoltageEvidence {
  std::string aggressorId, testCase;
  ComCoupling coupling = ComCoupling::Next;
  double voltsPerUnitSymbol = std::numeric_limits<double>::quiet_NaN();
  std::string transmitterReferencePlane, receiverDecisionPlane;
  std::string normalizationIdentity, calibrationIdentity, calibrationSourceIdentity;
  std::string txEqualizerIdentity, rxEqualizerIdentity;
};

struct ComAggressorVoltageCaseInput {
  std::vector<Result<ComAggressorVoltageEvidence>> calibrations;
};

struct ComAggressorVoltageInput {
  Result<ComInterferenceTestCases> interference;
  Result<ComScaledVoltageCases> victim;
  ComAggressorVoltageCaseInput test1, test2;
};

struct ComAggressorVoltagePulse {
  std::string id, testCase;
  ComCoupling coupling = ComCoupling::Next;
  std::array<double,13> unitSymbolPulse{}, equivalentPulseV{};
  double voltsPerUnitSymbol = 0., maximumAbsoluteEquivalentV = 0.;
  std::string transmitterReferencePlane, receiverDecisionPlane;
  std::string normalizationIdentity, calibrationIdentity, calibrationSourceIdentity;
  std::string txEqualizerIdentity, rxEqualizerIdentity;
};

struct ComAggressorVoltageCaseData {
  std::string testCase, receiverDecisionPlane;
  std::vector<double> timeSeconds;
  double symbolPeriodSeconds = 0.;
  std::vector<ComAggressorVoltagePulse> aggressors;
};

struct ComAggressorVoltageCasesData {
  ComAggressorVoltageCaseData test1, test2;
  std::string model = "EXPLORATORY_TWO_CASE_AGGRESSOR_RECEIVER_VOLTAGE";
};

// The 'live' identity MUST be read from the current active SourceSnapshot at
// invocation time. Neither identities nor caller-authored calibration metadata
// prove physical voltage calibration, TX conditioning or independence.
// Output is Warning/NOT_IEEE_COM; no stochastic NEXT/FEXT or COM verdict.
// Throws si::Error for unqualified input and si::Cancelled for cancellation.
Result<ComAggressorVoltageCasesData> calculateComAggressorReceiverVoltage(
    const ComAggressorVoltageInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled = nullptr);

// 012_35 bounded matched-board diagnostic. No package/die-plane equivalence.
struct ComBoardAggressorCaseChoice {
  std::array<double,5> txFfe{0.,0.,0.,1.,0.};
  double relativeDelaySeconds=0.;
  std::optional<double> declaredVoltsPerUnitSymbol;
  std::string calibrationSource;
};
struct ComBoardAggressorChoice {
  std::string id, channelId;
  ComCoupling coupling=ComCoupling::Next;
  bool repeatedPathDeclared=false;
  std::array<ComBoardAggressorCaseChoice,2> cases;
};
struct ComBoardCrosstalkCaseChoice {
  std::string testCase;
  double decisionTimeSeconds=std::numeric_limits<double>::quiet_NaN();
  double ctleDcGainDb=0.,ctleDcGain2Db=0.;
};
struct ComBoardCrosstalkInput {
  bool inventoryConfirmed=false;
  bool independentEquiprobablePam4Declared=false;
  std::string statisticalAssumptionSource;
  std::array<ComBoardCrosstalkCaseChoice,2> cases;
  std::vector<ComBoardAggressorChoice> aggressors;
};
struct ComBoardAggressorResult {
  ComInterferencePulse pulse;
  std::string channelId, calibrationSource;
  std::array<int,2> stimulusPorts{},responsePorts{};
  std::optional<std::array<double,13>> declaredEquivalentV;
  std::optional<double> observedPam4RmsV;
};
struct ComBoardCrosstalkCaseResult {
  std::string testCase;
  std::vector<double> timeSeconds;
  std::vector<ComBoardAggressorResult> aggressors;
  std::optional<double> observedIndependentPam4RmsV;
};
struct ComBoardCrosstalkResult {
  std::array<ComBoardCrosstalkCaseResult,2> cases;
  std::string model="EXPLORATORY_MATCHED_BOARD_XTALK_NOT_DIE_PLANE";
};
// Both cases use actual mapped board Sdd at the forward victim far endpoint.
// Cases label explicit timing/EQ, NOT qualified package networks. No PSD,
// die-plane aggregate, unobserved tail or official COM output is generated.
Result<ComBoardCrosstalkResult> calculateComMatchedBoardCrosstalkFromSource(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& observedLiveIdentity,const ComBoardCrosstalkInput& input,
    Control* control=nullptr);

} // namespace si::channel
