#include "si/channel_analysis/com_causal_channel.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace si::channel {
namespace {
void require(bool valid, const char *reason) {
  if (!valid) throw Error(reason);
}
void check(const std::atomic_bool *flag) {
  if (flag && flag->load(std::memory_order_relaxed))
    throw Cancelled();
}
ComTwoPortS evaluate(const ComWaveTwoPort &board,
                     double frequencyHz, ComPackageCase choice,
                     double sourcePhysicalReferenceOhm) {
  const auto lengths=com8023ckC2cPackageLengths(choice);
  auto tx=historical8023ckC2cCausalCandidate(lengths.txMm);
  auto rx=historical8023ckC2cCausalCandidate(lengths.rxMm);
  const auto txWave=calculateComCausalPackage(
      tx,frequencyHz,ComCausalPackageSide::TxDieToBoard,
      sourcePhysicalReferenceOhm);
  const auto rxWave=calculateComCausalPackage(
      rx,frequencyHz,ComCausalPackageSide::RxBoardToDie,
      sourcePhysicalReferenceOhm);
  // Redheffer retains phase, forward and reverse S21/S12, and all internal
  // TX/PCB/RX multiple reflections instead of multiplying scalar Sdd21.
  return cascadeComScattering(
      cascadeComScattering(txWave,board),rxWave).s;
}
}
Result<ComPhysicalChannelData> calculateHistoricalCausalC2cCases(
    const ComCausalPhysicalInput &input, Provenance provenance,
    const std::atomic_bool *cancelled) {
  check(cancelled);
  validateFrequency(input.frequencyHz,true);
  const size_t n=input.frequencyHz.size();
  require(n<=65536 && n==input.boardS4p.size(),
          "Causal COM physical S4P and frequency grids disagree");
  require(std::isfinite(input.maxAllowedModeConversion) &&
          input.maxAllowedModeConversion>=0 &&
          input.maxAllowedModeConversion<=1,
          "Causal package mode-conversion limit invalid");
  const double physicalRef=input.boardS4p.front().referenceOhm[0];
  require(std::isfinite(physicalRef) &&
          std::abs(physicalRef-50.0)<=1e-11,
          "Historical 802.3ck C2C reference requires 50-ohm physical ports");
  ComPhysicalChannelData data;
  data.model="EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P";
  data.frequencyHz=input.frequencyHz;
  data.referenceOhmDifferential=2*physicalRef;
  data.test1.packageCase=ComPackageCase::Test1;
  data.test2.packageCase=ComPackageCase::Test2;
  const auto test1=com8023ckC2cPackageLengths(ComPackageCase::Test1);
  const auto test2=com8023ckC2cPackageLengths(ComPackageCase::Test2);
  data.test1.txPrimaryLineMm=test1.txMm;
  data.test1.rxPrimaryLineMm=test1.rxMm;
  data.test2.txPrimaryLineMm=test2.txMm;
  data.test2.rxPrimaryLineMm=test2.rxMm;
  data.test1.loadedSdd.reserve(n);
  data.test2.loadedSdd.reserve(n);
  for(size_t k=0;k<n;++k) {
    if(!(k&255u))check(cancelled);
    const auto& sample=input.boardS4p[k];
    for(double reference:sample.referenceOhm) {
      require(std::isfinite(reference) &&
              std::abs(reference-physicalRef)<=1e-12*physicalRef,
              "Causal physical package requires equal, constant physical Zref");
    }
    const auto diff=extractDifferentialComNetwork(
        sample,input.maxAllowedModeConversion);
    require(diff.differential.referenceOhm==
            data.referenceOhmDifferential,
            "Causal package reference mismatch after mixed-mode conversion");
    data.maxObservedModeConversion=std::max(
        data.maxObservedModeConversion,diff.maximumModeConversion);
    data.test1.loadedSdd.push_back(evaluate(
        diff.differential,input.frequencyHz[k],
        ComPackageCase::Test1,physicalRef));
    data.test2.loadedSdd.push_back(evaluate(
        diff.differential,input.frequencyHz[k],
        ComPackageCase::Test2,physicalRef));
  }
  check(cancelled);
  Result<ComPhysicalChannelData> out;
  out.status=Status::Warning;
  out.value=std::move(data);
  out.provenance=std::move(provenance);
  out.provenance.algorithm="com-causal-c2c-historical-physical-network";
  out.provenance.version="012_09-causal-package-1";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Full causal dispersive historical C2C package Test1 and Test2 "
    "physical S4P circuits are diagnostic only. No verified final "
    "IEEE 802.3ck-2022 Table 120F-8, Tx/Rx optimization, "
    "ISI/XTALK/noise/jitter statistics or numerical COM dB."});
  return out;
}
Result<ComPhysicalChannelData> calculateHistoricalCausalC2cFromSource(
    const SourceSnapshot &source, std::string_view victimChannelId,
    double maxAllowedModeConversion, Control *control) {
  const auto& id=source.identity();
  Provenance provenance;
  provenance.sourceSha256=id.sha256;
  provenance.revision=id.revision;
  provenance.mappingIdentity=id.mappingIdentity;
  provenance.workspaceGeneration=id.workspaceGeneration;
  provenance.mappingGeneration=id.mappingGeneration;
  // Reuse the *actual* SHA-verified 16-Sij extraction. Package parameters
  // in this temporary extraction object are not used; historical causal
  // package selection is explicit in calculateHistoricalCausalC2cCases.
  auto measured=physicalComInputFromSource(
      source,victimChannelId,{},{},maxAllowedModeConversion,control);
  ComCausalPhysicalInput input;
  input.frequencyHz=std::move(measured.frequencyHz);
  input.boardS4p=std::move(measured.boardS4p);
  input.maxAllowedModeConversion=maxAllowedModeConversion;
  return calculateHistoricalCausalC2cCases(
      input,std::move(provenance),control?&control->cancelled:nullptr);
}
} // namespace si::channel
