#include "si/channel_analysis/com_jitter.hpp"
#include "si/channel_analysis/com_120f8.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace si;
using namespace si::channel;
namespace {
void require(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
void close(double got, double want, double rel, double absTol,
           const char* why) {
  require(std::isfinite(got) && std::isfinite(want) &&
              std::abs(got-want) <= absTol+rel*std::abs(want), why);
}
template<class T> void reject(T&& fn, const char* why) {
  bool caught = false;
  try { fn(); } catch (const Error&) { caught = true; }
  require(caught, why);
}
Provenance source(std::string id = std::string(64,'a')) {
  Provenance p;
  p.sourceSha256 = std::move(id);
  p.revision = "synthetic-revision-1";
  p.mappingIdentity = "channel-rx-test1";
  p.settingsIdentity = "rx-cal-v1";
  p.workspaceGeneration = 17;
  p.mappingGeneration = 23;
  return p;
}
Result<ComReceiverNoiseData> noise(
    const Provenance& provenance, double psd = 4e-8) {
  ComReceiverNoiseInput in;
  in.oneSidedPsdV2PerHz = psd;
  in.oneSidedTailProbability = 1e-5;
  in.frequencyHz = {0.,5000.,10000.};
  in.noiseTransfer = {{1,0},{1,0},{1,0}};
  return calculateComReceiverNoise(in,provenance);
}
ComJitterInput sample(const Provenance& p) {
  ComJitterInput in;
  in.symbolRateBd = 1e9;
  in.randomJitterRmsUi = 0.1;
  in.dualDiracPeakUi = 0.2;
  in.timeSeconds = {0,1e-9,2e-9};
  in.receiverVoltageV = {0,1,2}; // calibrated V -> slope 1e9 V/s
  in.decisionTimeSeconds = 1e-9;
  in.voltageReferencePlane = "synthetic-physical-RX-pin";
  in.voltageCalibrationIdentity = "unit-amplitude-voltage-calibration";
  in.receiverNoise = noise(p);
  in.oneSidedTailProbability = 1e-5;
  in.queryVoltageV = {-0.1,0,0.1,0.2};
  return in;
}
} // namespace

int main() {
  try {
    const auto provenance = source();
    const auto baseline = calculateComJitter(sample(provenance),provenance);
    require(baseline.usable() && baseline.status == Status::Warning &&
                baseline.value->hasContinuousDensity,
            "Jitter must be a usable explicitly non-normative result");
    require(baseline.provenance.sourceSha256 == provenance.sourceSha256 &&
                baseline.provenance.algorithm ==
                    "com-local-linear-rj-dual-dirac-exploratory",
            "Provenance must be retained and clearly exploratory");
    const auto& d = *baseline.value;
    close(d.unitIntervalSeconds,1e-9,1e-15,0,"UI independent oracle");
    close(d.randomJitterRmsSeconds,1e-10,1e-15,0,"RJ UI->s");
    close(d.dualDiracPeakSeconds,2e-10,1e-15,0,"DJ peak UI->s");
    close(d.decisionSlopeVPerSecond,1e9,1e-15,0,"local V/s");
    close(d.jitterGaussianRmsV,0.1,1e-14,0,"timing RJ->V");
    close(d.dualDiracVoltagePeakV,0.2,1e-14,0,"peak NOT peak-to-peak");
    close(d.receiverNoiseRmsV,0.02,1e-14,0,"012_15 RMS preserved");
    close(d.combinedGaussianRmsV,std::sqrt(0.0104),1e-14,0,
          "independent Gaussian variance sum");
    close(d.totalVoltageVarianceV2,0.0504,1e-14,0,
          "two-atom variance included exactly once");
    // Independent, high-precision analytic Gaussian-mixture fixture
    // calculated from 0.5*N(+/-0.2,0.0104); not produced by C++ kernel.
    close(d.oneSidedQuantileV,0.61888237820898782678,1e-10,0,
          "fixed independent RJ+DJ+RX quantile golden");
    close(d.verifiedTailProbability,1e-5,1e-6,0,
          "quantile reverse one-sided tail");
    require(d.pdfPerV.size()==4 && d.cdf.size()==4,"bounded PDF samples");
    close(d.pdfPerV[1],0.57175727625975764927,1e-12,0,
          "independent mixture PDF at zero");
    close(d.pdfPerV[2],1.23522923373394581224,1e-12,0,
          "independent mixture PDF(0.1)");
    close(d.cdf[1],0.5,0,1e-15,"mixture symmetry at zero");
    close(d.cdf[2],0.58088396269182394317,1e-12,0,
          "fixed independent mixture CDF");
    close(d.cdf[0]+d.cdf[2],1.0,0,1e-14,
          "CDF symmetry at symmetric points");
    close(d.cdf[3],0.74997807514406102442,1e-12,0,
          "independent mixture CDF(peak)");

    auto pure = sample(provenance);
    pure.dualDiracPeakUi = 0;
    const auto pureResult = calculateComJitter(pure,provenance);
    const double pureSigma = std::sqrt(0.0104);
    close(pureResult.value->oneSidedQuantileV/pureSigma,
          4.264890793922825,1e-10,0,
          "reuse existing independently qualified Gaussian quantile");
    require(pureResult.value->dualDiracVoltagePeakV==0,"No DJ peak");

    auto rjOnly = sample(provenance);
    rjOnly.receiverNoise = noise(provenance,0);
    rjOnly.dualDiracPeakUi = 0;
    const auto rj = calculateComJitter(rjOnly,provenance);
    close(rj.value->combinedGaussianRmsV,0.1,1e-14,0,
          "RJ-only variance");
    close(rj.value->oneSidedQuantileV,0.4264890793922825,
          1e-10,0,"RJ-only Gaussian golden");

    auto djOnly = sample(provenance);
    djOnly.receiverNoise = noise(provenance,0);
    djOnly.randomJitterRmsUi = 0;
    const auto dj = calculateComJitter(djOnly,provenance);
    require(dj.usable() && !dj.value->hasContinuousDensity &&
                dj.value->pdfPerV.empty() && dj.value->cdf.empty() &&
                std::isnan(dj.value->oneSidedQuantileV),
            "DJ-only two-atom distribution MUST NOT invent continuous PDF/Q");
    close(dj.value->totalVoltageVarianceV2,0.04,1e-14,0,
          "two-atom exact variance");
    djOnly.dualDiracPeakUi = 0;
    const auto none = calculateComJitter(djOnly,provenance);
    require(none.usable() && !none.value->hasContinuousDensity &&
                none.value->totalVoltageVarianceV2==0,
            "fully zero distribution is degenerate");

    auto flat = sample(provenance);
    flat.receiverVoltageV={1,1,1};
    const auto stationary = calculateComJitter(flat,provenance);
    require(stationary.value->jitterGaussianRmsV==0 &&
                stationary.value->dualDiracVoltagePeakV==0,
            "zero slope eliminates local jitter voltage error");
    close(stationary.value->oneSidedQuantileV/0.02,
          4.264890793922825,1e-10,0,
          "flat waveform keeps only receiver noise");

    auto negative = sample(provenance);
    negative.receiverVoltageV={2,1,0};
    const auto neg=calculateComJitter(negative,provenance);
    require(neg.value->decisionSlopeVPerSecond < 0,"negative slope retained");
    close(neg.value->oneSidedQuantileV,d.oneSidedQuantileV,
          1e-12,0,"slope sign invariance for symmetric DJ/RJ");

    auto scaled=sample(provenance);
    scaled.receiverVoltageV={0,2,4};
    scaled.receiverNoise=noise(provenance,16e-8);
    for(double& v:scaled.queryVoltageV)v*=2;
    const auto twice=calculateComJitter(scaled,provenance);
    close(twice.value->oneSidedQuantileV,2*d.oneSidedQuantileV,
          1e-12,0,"voltage scaling metamorphic test");
    close(twice.value->pdfPerV[1],d.pdfPerV[1]/2,
          1e-12,0,"PDF scales inversely with voltage");

    auto standard=sample(provenance);
    const auto profile=final8023ck120F8();
    standard.symbolRateBd=profile.symbolRateHz;
    standard.randomJitterRmsUi=profile.randomJitterRmsUi;
    standard.dualDiracPeakUi=profile.dualDiracJitterPeakUi;
    const auto profileResult=calculateComJitter(standard,provenance);
    close(profileResult.value->unitIntervalSeconds,1./53.125e9,
          1e-14,0,"published 120F-8 UI value conversion");
    close(profileResult.value->randomJitterRmsSeconds,0.01/53.125e9,
          1e-14,0,"published 120F-8 RJ UI conversion");
    close(profileResult.value->dualDiracPeakSeconds,0.02/53.125e9,
          1e-14,0,"published 120F-8 DJ peak UI conversion");

    auto invalid=sample(provenance);
    invalid.symbolRateBd=0;
    reject([&]{calculateComJitter(invalid,provenance);},"zero rate rejected");
    invalid=sample(provenance);
    invalid.randomJitterRmsUi=-0.1;
    reject([&]{calculateComJitter(invalid,provenance);},"negative RJ");
    invalid=sample(provenance);
    invalid.dualDiracPeakUi=std::numeric_limits<double>::quiet_NaN();
    reject([&]{calculateComJitter(invalid,provenance);},"NaN DJ");
    invalid=sample(provenance);
    invalid.randomJitterRmsUi=2;
    reject([&]{calculateComJitter(invalid,provenance);},"budget >1 UI");
    invalid=sample(provenance);
    invalid.receiverVoltageV={0,1};
    reject([&]{calculateComJitter(invalid,provenance);},"voltage size mismatch");
    invalid=sample(provenance);
    invalid.timeSeconds={0,1e-9,1e-9};
    reject([&]{calculateComJitter(invalid,provenance);},"duplicate time");
    invalid=sample(provenance);
    invalid.decisionTimeSeconds=0;
    reject([&]{calculateComJitter(invalid,provenance);},"no extrapolation");
    invalid=sample(provenance);
    invalid.voltageCalibrationIdentity="";
    reject([&]{calculateComJitter(invalid,provenance);},"calibration mandatory");
    invalid=sample(provenance);
    invalid.receiverNoise.status=Status::Stale;
    reject([&]{calculateComJitter(invalid,provenance);},"STALE RX rejected");
    invalid=sample(provenance);
    invalid.receiverNoise.value->rmsV*=2;
    reject([&]{calculateComJitter(invalid,provenance);},"bad RX RMS/variance");
    invalid=sample(provenance);
    invalid.queryVoltageV={std::numeric_limits<double>::infinity()};
    reject([&]{calculateComJitter(invalid,provenance);},"nonfinite voltage");
    invalid=sample(provenance);
    invalid.queryVoltageV.resize(129,0);
    reject([&]{calculateComJitter(invalid,provenance);},"query budget");
    invalid=sample(provenance);
    invalid.oneSidedTailProbability=1e-20;
    reject([&]{calculateComJitter(invalid,provenance);},"tail budget");
    invalid=sample(provenance);
    invalid.receiverVoltageV[1]=std::numeric_limits<double>::infinity();
    reject([&]{calculateComJitter(invalid,provenance);},"nonfinite V");
    invalid=sample(provenance);
    invalid.timeSeconds={0,std::numeric_limits<double>::denorm_min(),2e-9};
    invalid.decisionTimeSeconds=1e-9;
    reject([&]{calculateComJitter(invalid,provenance);},
           "subnormal time step");

    const auto different=source(std::string(64,'b'));
    reject([&]{calculateComJitter(sample(provenance),different);},
           "mixed Test1/Test2 SourceSnapshot rejected");
    const auto second=calculateComJitter(sample(different),different);
    require(second.provenance.sourceSha256==different.sourceSha256 &&
                second.provenance.sourceSha256!=baseline.provenance.sourceSha256,
            "separate Test1/Test2 remain separate");
    std::atomic_bool cancelled{true};
    bool stopped=false;
    try {calculateComJitter(sample(provenance),provenance,&cancelled);}
    catch(const Cancelled&) {stopped=true;}
    require(stopped,"cancellation must propagate");
    std::cout<<"012_16 local-linear jitter mathematical and fail-closed tests PASS\n";
  } catch(const std::exception& ex) {
    std::cerr<<"012_16 FAIL: "<<ex.what()<<"\n";
    return 1;
  }
  return 0;
}
