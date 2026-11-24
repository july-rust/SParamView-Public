#pragma once
#include "si/channel_analysis/com_aggressor_voltage.hpp"
#include "si/channel_analysis/com_jitter.hpp"

namespace si::channel {
// Alternative declared injection planes; neither is a qualified Annex 93A mapping.
enum class ComFixedNoisePlane { RxFilterInput, CtleInput };
struct ComFixedNoiseChoice {
  ComFixedNoisePlane plane=ComFixedNoisePlane::RxFilterInput;
  double oneSidedPsdV2PerHz=std::numeric_limits<double>::quiet_NaN();
  std::string noiseSource, receiverDecisionPlane, independenceDeclaration;
  bool independentRxRjDjDeclared=false;
  // Separate symbol-distribution assumption for the observed h_J vector.
  bool independentEquiprobablePam4Declared=false;
  double diagnosticTailProbability=1e-5;
};
struct ComFixedNoiseInput {
  Com120F8EqualizerChoice eq;
  std::array<ComFixedVictimCaseChoice,2> victim;
  std::array<ComFixedNoiseChoice,2> noise;
  std::optional<ComBoardCrosstalkInput> crosstalk;
  std::string historicalPackageAssumption;
};
// Bounded reference-style observed span; not the full h_J phase vector.
struct ComObservedReferenceJitter {
  std::array<double,13> hJVoltagePerUi{}, dualDiracTapV{};
  double randomJitterRmsV=0., dualDiracRmsV=0.;
};
struct ComFixedNoiseAggressorOwner {
  std::string id, channelId;
  ComCoupling coupling=ComCoupling::Next;
  std::array<int,2> stimulusPorts{}, responsePorts{};
  std::optional<double> observedRmsV, psdPowerV2;
};
struct ComFixedNoiseCase {
  std::string testCase, victimSettingsIdentity, noiseSource, receiverDecisionPlane;
  std::string independenceDeclaration;
  ComFixedNoisePlane injectionPlane=ComFixedNoisePlane::RxFilterInput;
  std::vector<double> localTimeSeconds, localVoltageV;
  Result<ComReceiverNoiseData> receiverNoise;
  Result<ComJitterData> jitter;
  // Alternatives, never additive to receiverNoise or local jitter.
  std::optional<Result<ComReceiverNoiseData>> referenceRightEndpointNoise;
  std::optional<ComObservedReferenceJitter> observedReferenceJitter;
  std::vector<ComFixedNoiseAggressorOwner> aggressorOwners;
  std::optional<double> residualIsiObservedRmsV;
  std::optional<double> crosstalkObservedRmsV, crosstalkPsdPowerV2;
  std::string crosstalkSettingsIdentity;
};
struct ComFixedNoiseData {
  std::array<ComFixedNoiseCase,2> cases;
  std::string model="EXPLORATORY_FIXED_EQ_LOCAL_NOISE_JITTER";
};
// No summation is performed. A requested representation cannot coexist with
// another representation of the same component. DFE state is not noise.
enum class ComNoiseRepresentation {
  PreDfeIsi, ResidualIsi, CrosstalkObserved, CrosstalkPsd,
  RxGaussian, RandomJitter, DualDirac, CombinedGaussian, JitterMixture,
  ReferenceRxRightEndpoint, ReferenceObservedRj, ReferenceObservedDd,
  NextObserved, FextObserved, NextPsd, FextPsd
};
void validateComFixedNoiseRepresentations(const std::vector<ComNoiseRepresentation>& choices);
Result<ComFixedNoiseData> calculateComFixedNoiseFromSource(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& live,const ComFixedNoiseInput& input,
    Control* control=nullptr);
} // namespace si::channel
