#include "si/channel_analysis/com_pipeline.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace si::channel {
namespace {
void require(bool valid, const char *reason) {
  if(!valid)throw Error(reason);
}
void check(const std::atomic_bool *flag) {
  if(flag && flag->load(std::memory_order_relaxed))throw Cancelled();
}
ComWaveTwoPort reversed(ComWaveTwoPort p) {
  std::swap(p.s.s11,p.s.s22);
  std::swap(p.s.s12,p.s.s21);
  return p;
}
}
Result<ComPhysicalThruData> calculateComPhysicalThru(
    const ComPhysicalThruRequest &req, Provenance provenance,
    const std::atomic_bool *cancelled) {
  check(cancelled);
  const auto q=validateFrequency(req.frequencyHz,true);
  require(req.frequencyHz.size()==req.measured.size(),
          "Physical S4P sample count does not match frequency grid");
  require(req.frequencyHz.size()<=131072,
          "Physical COM spectral case exceeds bounded memory");
  require(std::isfinite(req.symbolRateHz) && req.symbolRateHz>0 &&
          req.frequencyHz.back()>=req.symbolRateHz/2,
          "Actual symbol rate or measured source Nyquist band is missing");
  require(std::isfinite(req.maximumAllowedModeConversion) &&
          req.maximumAllowedModeConversion>=0 &&
          req.maximumAllowedModeConversion<=1,
          "Unsupported mode conversion tolerance");
  require(req.txPackage.primaryLineMm==0 &&
          req.rxPackage.primaryLineMm==0,
          "The Test1/Test2 primary package length must be selected by case");
  require(req.measured.size()>=2,
          "Physical channel needs at least two complex samples");
  ComPhysicalThruData data;
  for(size_t t=0;t<2;++t) {
    data.cases[t].packageCase=t==0?ComPackageCase::Test1:
                                         ComPackageCase::Test2;
    data.cases[t].thru.frequencyHz=req.frequencyHz;
    data.cases[t].thru.transfer.reserve(req.frequencyHz.size());
    data.cases[t].thru.parameter=req.reverse?"Sdd12":"Sdd21";
    data.cases[t].inputReflection.reserve(req.frequencyHz.size());
    data.cases[t].outputReflection.reserve(req.frequencyHz.size());
  }
  double sourceZref=0;
  for(size_t i=0;i<req.measured.size();++i) {
    if(!(i&127u))check(cancelled);
    const auto& four=req.measured[i];
    const auto extracted=extractDifferentialComNetwork(
        four,req.maximumAllowedModeConversion);
    if(i==0) {
      sourceZref=extracted.differential.referenceOhm;
      for(auto &test:data.cases)
        test.thru.stimulusOhm=test.thru.responseOhm=sourceZref;
    } else {
      require(extracted.differential.referenceOhm==sourceZref,
              "Physical source impedance varies across frequency; no implicit normalization");
    }
    ComWaveTwoPort d=extracted.differential;
    if(req.reverse)d=reversed(d);
    const double f=req.frequencyHz[i];
    for(size_t t=0;t<2;++t) {
      auto& row=data.cases[t];
      auto tx=req.txPackage,rx=req.rxPackage;
      const auto selected=com8023ckC2cPackageLengths(row.packageCase);
      tx.primaryLineMm=selected.txMm;
      rx.primaryLineMm=selected.rxMm;
      const double singleRef=sourceZref/2.;
      const auto txS=calculateBalancedComPackage(
          tx,f,ComPackageSide::TxDieToBoard,singleRef);
      const auto rxS=calculateBalancedComPackage(
          rx,f,ComPackageSide::RxBoardToDie,singleRef);
      const auto txAndBoard=cascadeComScattering(txS,d);
      const auto physical=cascadeComScattering(txAndBoard,rxS);
      row.thru.transfer.push_back(physical.s.s21);
      row.inputReflection.push_back(physical.s.s11);
      row.outputReflection.push_back(physical.s.s22);
      row.largestObservedModeConversion=std::max(
          row.largestObservedModeConversion,
          extracted.maximumModeConversion);
    }
  }
  check(cancelled);
  for(auto& row:data.cases)validateTrace(row.thru);
  Result<ComPhysicalThruData> out;
  out.status=Status::Warning;
  out.value=std::move(data);
  out.provenance=std::move(provenance);
  out.provenance.algorithm="com-physical-s4p-thru";
  out.provenance.version="012_09-physical-1";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Complex 4-port + passive TX/RX package Test1/Test2 transfer "
    "is a physical model, not the final approved COM profile, "
    "ISI/XTALK/noise optimization, or IEEE 802.3ck margin/PASS."});
  if(!q.uniform)
    out.diagnostics.push_back({Severity::Warning,"NONUNIFORM_SOURCE",
      "Nonuniform source grid preserved; no FFT or extrapolation performed."});
  return out;
}
} // namespace si::channel
