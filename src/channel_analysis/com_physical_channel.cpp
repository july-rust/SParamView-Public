#include "si/channel_analysis/com_physical_channel.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

namespace si::channel {
namespace {
void require(bool ok, const char *reason) {
  if (!ok) throw Error(reason);
}
void check(const std::atomic_bool *cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw Cancelled();
}
void validatePackageSelector(const ComPackageCircuit &p) {
  require(p.primaryLineMm == 0,
          "Specify zero primary package length: selected Test1/Test2 "
          "will explicitly override it");
  // Every other package field is validated by calculateBalancedComPackage,
  // including finite positive characteristic impedances and passive loss.
}
ComTwoPortS physicalCase(const ComPhysicalChannelInput &input,
                         const ComWaveTwoPort &board,
                         size_t i, double zPhysical, ComPackageCase testCase) {
  const auto lengths = com8023ckC2cPackageLengths(testCase);
  auto tx = input.transmitter;
  auto rx = input.receiver;
  tx.primaryLineMm = lengths.txMm;
  rx.primaryLineMm = lengths.rxMm;
  auto left = calculateBalancedComPackage(tx, input.frequencyHz[i],
                                         ComPackageSide::TxDieToBoard,
                                         zPhysical);
  auto right = calculateBalancedComPackage(rx, input.frequencyHz[i],
                                          ComPackageSide::RxBoardToDie,
                                          zPhysical);
  // Redheffer products carry exact complex phase and ALL internal
  // passive reflections between die package, PCB and receive package.
  return cascadeComScattering(cascadeComScattering(left,board),right).s;
}
}
Result<ComPhysicalChannelData> calculateComPhysicalTestCases(
    const ComPhysicalChannelInput &input, Provenance provenance,
    const std::atomic_bool *cancelled) {
  check(cancelled);
  validateFrequency(input.frequencyHz,true);
  const size_t n=input.frequencyHz.size();
  require(n<=65536 && n==input.boardS4p.size(),
          "Full physical four-port source frequency count/grid mismatch");
  require(std::isfinite(input.maxAllowedModeConversion) &&
          input.maxAllowedModeConversion>=0 &&
          input.maxAllowedModeConversion<=1,
          "Invalid requested mode-conversion limit");
  validatePackageSelector(input.transmitter);
  validatePackageSelector(input.receiver);
  ComPhysicalChannelData data;
  data.frequencyHz=input.frequencyHz;
  data.test1.packageCase=ComPackageCase::Test1;
  data.test2.packageCase=ComPackageCase::Test2;
  const auto t1=com8023ckC2cPackageLengths(ComPackageCase::Test1);
  const auto t2=com8023ckC2cPackageLengths(ComPackageCase::Test2);
  data.test1.txPrimaryLineMm=t1.txMm;
  data.test1.rxPrimaryLineMm=t1.rxMm;
  data.test2.txPrimaryLineMm=t2.txMm;
  data.test2.rxPrimaryLineMm=t2.rxMm;
  data.test1.loadedSdd.reserve(n);
  data.test2.loadedSdd.reserve(n);
  const double zPhysical=input.boardS4p.front().referenceOhm[0];
  require(std::isfinite(zPhysical) && zPhysical>0 &&
          std::isfinite(zPhysical*2.0),
          "Invalid source single-ended reference impedance");
  data.referenceOhmDifferential=2.0*zPhysical;
  for(size_t i=0;i<n;++i) {
    if(!(i&255u))check(cancelled);
    const auto& source=input.boardS4p[i];
    for(double z:source.referenceOhm) {
      require(std::isfinite(z) &&
              std::abs(z-zPhysical)<=1e-12*zPhysical,
              "Mixed frequency-dependent or unequal single-ended "
              "port reference impedance unsupported");
    }
    const auto differential=extractDifferentialComNetwork(
        source,input.maxAllowedModeConversion);
    require(differential.differential.referenceOhm==
            data.referenceOhmDifferential,
            "Differential reference wave normalization mismatch");
    data.maxObservedModeConversion=
        std::max(data.maxObservedModeConversion,
                 differential.maximumModeConversion);
    data.test1.loadedSdd.push_back(physicalCase(
        input,differential.differential,i,zPhysical,ComPackageCase::Test1));
    data.test2.loadedSdd.push_back(physicalCase(
        input,differential.differential,i,zPhysical,ComPackageCase::Test2));
  }
  check(cancelled);
  Result<ComPhysicalChannelData> result;
  result.status=Status::Warning;
  result.value=std::move(data);
  result.provenance=std::move(provenance);
  result.provenance.algorithm="com-physical-4port-cascade";
  result.provenance.version="012_09-physical-pipeline-1";
  result.diagnostics.push_back({
    Severity::Warning,"NOT_IEEE_COM",
    "Complex physical S4P/ABCD/Redheffer TX/board/RX two-package test "
    "responses are exploratory. They are not IEEE 802.3ck Table 120F-8 "
    "verified package propagation, receiver noise/EQ, COM dB, or PHY "
    "conformance; the calculateCom entrypoint remains fail-closed."
  });
  return result;
}
} // namespace si::channel
