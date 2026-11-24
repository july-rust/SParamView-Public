#include "si/channel_analysis/com_aggressor_voltage.hpp"
#include "si/channel_analysis/com_causal_package.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace si::channel {
namespace {
constexpr size_t kTapCount = 13;
constexpr size_t kMaxAggressors = 32;
void require(bool ok, const char* reason) {
  if (!ok) throw si::Error(reason);
}
void check(const std::atomic_bool* cancelled) {
  if (cancelled && cancelled->load(std::memory_order_relaxed))
    throw si::Cancelled();
}
double scaleSample(double u,double factor) {
  require(std::isfinite(u) && std::isnormal(factor) && factor>0.,
          "COM invalid unit-symbol sample or declared voltage scale");
  const double v=u*factor;
  require(std::isfinite(v) && (u==0. || std::isnormal(v)),
          "COM voltage overflow or underflow");
  return v;
}
bool bounded(const std::string& s) { return !s.empty() && s.size() <= 256; }
bool sameSource(const Provenance& a, const Provenance& b) {
  return a.sourceSha256 == b.sourceSha256 && a.revision == b.revision &&
         a.mappingIdentity == b.mappingIdentity &&
         a.settingsIdentity == b.settingsIdentity &&
         a.workspaceGeneration == b.workspaceGeneration &&
         a.mappingGeneration == b.mappingGeneration;
}
bool qualified(const Result<ComInterferenceTestCases>& r,
               const Provenance& expected) {
  if (r.status != Status::Warning || !r.value ||
      !sameSource(r.provenance, expected) ||
      r.provenance.algorithm != "com-120f8-separated-test1-test2-interference" ||
      r.provenance.version != "012_14-1") return false;
  return std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
    [](const Diagnostic& d) {
      return d.severity == Severity::Warning && d.code == "NOT_IEEE_COM";
    });
}
Provenance caseIdentity(const Provenance& pair, const char* which) {
  Provenance p = pair;
  if (!p.settingsIdentity.empty() && p.settingsIdentity.back() != '|')
    p.settingsIdentity.push_back('|');
  p.settingsIdentity += "case=";
  p.settingsIdentity += which;
  require(p.settingsIdentity.size() <= 256, "012_24: case identity too long");
  return p;
}
void validateLive(const Provenance& expected, const SourceIdentity& live) {
  require(expected.sourceSha256 == live.sha256 &&
          expected.revision == live.revision &&
          expected.mappingIdentity == live.mappingIdentity &&
          expected.workspaceGeneration == live.workspaceGeneration &&
          expected.mappingGeneration == live.mappingGeneration,
          "012_24: stale or incompatible live SourceSnapshot");
}
void validateTime(const ComInterferenceData& x,
                  const ComScaledVoltageCase& victim,
                  const std::atomic_bool* cancelled) {
  const double ui = 1. / final8023ck120F8().symbolRateHz;
  const double tol = 1e-6 * ui;
  require(x.model == "EXPLORATORY_BANDLIMITED_INDIVIDUAL_XTALK_PULSES" &&
          x.timeSeconds.size() == kTapCount &&
          std::isfinite(x.symbolPeriodSeconds) &&
          std::abs(x.symbolPeriodSeconds - ui) <= 1e-10 * ui,
          "012_24: unsupported interference model or symbol grid");
  for (size_t i = 0; i < kTapCount; ++i) {
    check(cancelled);
    const double t = x.timeSeconds[i];
    require(std::isfinite(t) && t >= 0. &&
            (i == 0 || t > x.timeSeconds[i - 1]) &&
            std::abs(t - victim.timeSeconds[i]) <= tol &&
            std::abs((t - x.timeSeconds[3]) -
                     (static_cast<double>(i) - 3.) * ui) <= tol,
            "012_24: victim/aggressor decision time or UI mismatch");
  }
}
void validateCalibration(const Result<ComAggressorVoltageEvidence>& r,
                         const ComInterferencePulse& pulse,
                         const ComScaledVoltageCase& victim,
                         const Provenance& expected, const char* which) {
  require(r.usable() && sameSource(r.provenance, expected) &&
          bounded(r.provenance.algorithm) && bounded(r.provenance.version),
          "012_24: missing/stale per-case aggressor calibration");
  const auto& v = *r.value;
  require(v.aggressorId == pulse.id && v.testCase == which &&
          v.coupling == pulse.coupling &&
          std::isnormal(v.voltsPerUnitSymbol) && v.voltsPerUnitSymbol > 0. &&
          bounded(v.transmitterReferencePlane) &&
          v.receiverDecisionPlane == victim.receiverDecisionPlane &&
          v.normalizationIdentity ==
            "EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL" &&
          v.normalizationIdentity == victim.normalizationIdentity &&
          bounded(v.calibrationIdentity) &&
          v.calibrationIdentity == victim.calibrationIdentity &&
          bounded(v.calibrationSourceIdentity) &&
          bounded(v.txEqualizerIdentity) &&
          v.rxEqualizerIdentity == victim.rxEqualizerIdentity,
          "012_24: unsupported calibration/plane/EQ or aggressor identity");
}
ComAggressorVoltageCaseData convertCase(
    const ComInterferenceData& interference,
    const ComScaledVoltageCase& victim,
    const ComAggressorVoltageCaseInput& input,
    const Provenance& expected, const char* which,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  validateTime(interference, victim, cancelled);
  require(interference.aggressors.size() <= kMaxAggressors &&
          input.calibrations.size() == interference.aggressors.size(),
          "012_24: aggressor count/calibration budget mismatch");
  std::unordered_map<std::string, const ComInterferencePulse*> pulses;
  for (const auto& p : interference.aggressors) {
    check(cancelled);
    require(bounded(p.id) &&
            (p.coupling == ComCoupling::Next ||
             p.coupling == ComCoupling::Fext) &&
            p.unitSymbolPulse.size() == kTapCount &&
            pulses.emplace(p.id, &p).second,
            "012_24: invalid/duplicate aggressor ID, coupling or pulse length");
  }
  std::unordered_set<std::string> used;
  ComAggressorVoltageCaseData out;
  out.testCase = which;
  out.receiverDecisionPlane = victim.receiverDecisionPlane;
  out.timeSeconds = interference.timeSeconds;
  out.symbolPeriodSeconds = interference.symbolPeriodSeconds;
  out.aggressors.reserve(input.calibrations.size());
  for (const auto& r : input.calibrations) {
    check(cancelled);
    require(r.usable() && bounded(r.value->aggressorId),
            "012_24: missing aggressor calibration ID");
    const auto it = pulses.find(r.value->aggressorId);
    require(it != pulses.end() && used.insert(r.value->aggressorId).second,
            "012_24: unmatched or repeated aggressor calibration");
    const auto& pulse = *it->second;
    validateCalibration(r, pulse, victim, expected, which);
    const auto& e = *r.value;
    ComAggressorVoltagePulse result;
    result.id = pulse.id;
    result.testCase = which;
    result.coupling = pulse.coupling;
    result.voltsPerUnitSymbol = e.voltsPerUnitSymbol;
    result.transmitterReferencePlane = e.transmitterReferencePlane;
    result.receiverDecisionPlane = e.receiverDecisionPlane;
    result.normalizationIdentity = e.normalizationIdentity;
    result.calibrationIdentity = e.calibrationIdentity;
    result.calibrationSourceIdentity = e.calibrationSourceIdentity;
    result.txEqualizerIdentity = e.txEqualizerIdentity;
    result.rxEqualizerIdentity = e.rxEqualizerIdentity;
    double observedInputMaximum = 0.;
    for (size_t i = 0; i < kTapCount; ++i) {
      check(cancelled);
      const double u = pulse.unitSymbolPulse[i];
      const double voltage = scaleSample(u,e.voltsPerUnitSymbol);
      result.unitSymbolPulse[i] = u;
      result.equivalentPulseV[i] = voltage;
      observedInputMaximum = std::max(observedInputMaximum, std::abs(u));
      result.maximumAbsoluteEquivalentV =
          std::max(result.maximumAbsoluteEquivalentV, std::abs(voltage));
    }
    require(std::isfinite(pulse.maximumAbsolutePulse) &&
            pulse.maximumAbsolutePulse >= 0.,
            "012_24: invalid upstream maximum");
    if (std::fpclassify(observedInputMaximum) == FP_ZERO) {
      require(std::fpclassify(pulse.maximumAbsolutePulse) == FP_ZERO,
              "012_24: zero waveform reports nonzero maximum");
    } else {
      require(pulse.maximumAbsolutePulse > 0. &&
              std::abs(pulse.maximumAbsolutePulse - observedInputMaximum) /
              std::max(pulse.maximumAbsolutePulse, observedInputMaximum) <= 1e-12,
              "012_24: tampered upstream pulse maximum");
    }
    out.aggressors.push_back(std::move(result));
  }
  std::sort(out.aggressors.begin(), out.aggressors.end(),
            [](const ComAggressorVoltagePulse& a,
               const ComAggressorVoltagePulse& b) { return a.id < b.id; });
  return out;
}
void sameAggressors(const ComAggressorVoltageCaseData& a,
                    const ComAggressorVoltageCaseData& b) {
  require(a.aggressors.size() == b.aggressors.size(),
          "012_24: Test1/Test2 aggressor count mismatch");
  for (size_t i = 0; i < a.aggressors.size(); ++i)
    require(a.aggressors[i].id == b.aggressors[i].id &&
            a.aggressors[i].coupling == b.aggressors[i].coupling,
            "012_24: Test1/Test2 aggressor/coupling set mismatch");
}
} // namespace

Result<ComAggressorVoltageCasesData> calculateComAggressorReceiverVoltage(
    const ComAggressorVoltageInput& input, const Provenance& expectedPair,
    const SourceIdentity& live, const std::atomic_bool* cancelled) {
  check(cancelled);
  validateLive(expectedPair, live);
  // 012_22 revalidates original signed 012_21 pulses, time grid and scale.
  const auto victimIsi = calculateComVictimIsi(input.victim, expectedPair, cancelled);
  require(victimIsi.usable() && victimIsi.status == Status::Warning &&
          victimIsi.provenance.algorithm == "com-victim-self-isi-exploratory" &&
          victimIsi.provenance.version == "012_22-1",
          "012_24: unqualified victim pulse source");
  require(qualified(input.interference, expectedPair),
          "012_24: missing/unqualified 012_14 interference");
  const auto p1 = caseIdentity(expectedPair, "TEST1");
  const auto p2 = caseIdentity(expectedPair, "TEST2");
  const auto& pulses = *input.interference.value;
  const auto& voltage = *input.victim.value;
  ComAggressorVoltageCasesData data;
  data.test1 = convertCase(pulses.test1, voltage.test1, input.test1,
                           p1, "TEST1", cancelled);
  data.test2 = convertCase(pulses.test2, voltage.test2, input.test2,
                           p2, "TEST2", cancelled);
  sameAggressors(data.test1, data.test2);
  check(cancelled);
  Result<ComAggressorVoltageCasesData> out;
  out.status = Status::Warning;
  out.value = std::move(data);
  out.provenance = expectedPair;
  out.provenance.algorithm = "com-aggressor-receiver-voltage-exploratory";
  out.provenance.version = "012_24-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
    "Caller-declared NEXT/FEXT voltage scaling only. Provenance consistency "
    "does not prove TX calibration, physically conditioned transfer or "
    "aggressor independence. No statistical x-talk, COM dB or PHY verdict."});
  return out;
}
namespace {
// Read all 16 physical Sij for the two selected differential endpoints.
// Other physical ports remain reference-matched, as in the 012_34 victim
// two-port reduction. Reuse passive mixed-mode and causal package cascades;
// never apply a scalar TX/RX gain to Sdd21 or claim full multiport loading.
ComplexTrace historicalPath(const SourceSnapshot& source,const Channel& victim,
    const Channel& aggressor,bool next,size_t caseIndex,const ComplexTrace& raw,
    Control* control) {
  const std::array<int,4> ports{next?aggressor.farP:aggressor.nearP,
      next?aggressor.farN:aggressor.nearN,victim.farP,victim.farN};
  require(std::unordered_set<int>(ports.begin(),ports.end()).size()==4,
          "012_35 historical path requires distinct physical endpoints");
  std::array<std::vector<Complex>,16> traces;
  const size_t n=raw.frequencyHz.size();
  for(size_t response=0;response<4;++response)
    for(size_t stimulus=0;stimulus<4;++stimulus) {
      if(control)control->check();
      auto& s=traces[4*response+stimulus];
      s=source.cache().trace({{ports[response],1.}},{{ports[stimulus],1.}},control);
      require(s.size()==n,"012_35 historical physical grid mismatch");
    }
  const auto profile=final8023ck120F8();
  const auto tx=historical8023ckC2cCausalCandidate(
      next?profile.rxPackageLengthMm[caseIndex]:profile.txPackageLengthMm[caseIndex]);
  const auto rx=historical8023ckC2cCausalCandidate(profile.rxPackageLengthMm[caseIndex]);
  ComplexTrace loaded=raw;
  for(size_t i=0;i<n;++i) {
    if(control && !(i&255u))control->check();
    ComPhysicalFourPort measured;measured.referenceOhm={50.,50.,50.,50.};
    for(size_t j=0;j<16;++j)measured.scattering[j]=traces[j][i];
    const auto board=extractDifferentialComNetwork(measured,0.).differential;
    require(std::abs(board.s.s21-raw.transfer[i])<=1e-12,
            "012_35 scalar and physical endpoint mapping disagree");
    const double f=raw.frequencyHz[i];
    const auto left=calculateComCausalPackage(tx,f,ComCausalPackageSide::TxDieToBoard);
    const auto right=calculateComCausalPackage(rx,f,ComCausalPackageSide::RxBoardToDie);
    loaded.transfer[i]=cascadeComScattering(cascadeComScattering(left,board),right).s.s21;
  }
  return loaded;
}
// For iid zero-mean equiprobable PAM4 symbols and a rectangular symbol
// waveform: one-sided time-averaged PSD = 2*(5/9)*UI*sinc^2*|H|^2*scale^2.
// Integrate only the measured band, not the sampled-window RMS or an IEEE
// detector-error variance. The DC endpoint has trapezoid half weight.
double measuredPsdPower(const std::vector<double>& f,const std::vector<Complex>& h,
    double scale,double ui,const std::atomic_bool* cancelled) {
  double sum=0.,previous=0.;
  for(size_t i=0;i<f.size();++i) {
    if(!(i&1023u))check(cancelled);
    const double z=std::numbers::pi*f[i]*ui;
    const double sinc=i?std::sin(z)/z:1.;
    const double v=scaleSample(std::abs(h[i]),scale);
    const double density=2.*(5./9.)*ui*std::pow(v*sinc,2);
    require(std::isfinite(density)&&density>=0.,"012_35 invalid PSD density");
    if(i)sum+=(f[i]-f[i-1])*(previous*.5+density*.5);
    require(std::isfinite(sum),"012_35 PSD integral overflow");
    previous=density;
  }
  return sum;
}
Result<ComBoardCrosstalkResult> calculateFixedCrosstalk(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& observedLiveIdentity,const ComBoardCrosstalkInput& input,
    Control* control,const Result<ComFixedVictimCases>* replay) {
  const auto* cancelled=control?&control->cancelled:nullptr;
  // Phase 1: qualify current source, inventory, endpoint ownership and work budget.
  check(cancelled);
  const auto stale=[] {
    Result<ComBoardCrosstalkResult> r;
    r.status=Status::Stale;
    r.diagnostics.push_back({Severity::Error,"SOURCE_STALE","012_35 stale board source"});
    return r;
  };
  if(!source.current(observedLiveIdentity)) return stale();
  require(input.inventoryConfirmed && input.aggressors.size()<=kMaxAggressors &&
          bounded(std::string(victimId)),"012_35 missing inventory or excessive aggressors");
  require(input.independentEquiprobablePam4Declared ?
          bounded(input.statisticalAssumptionSource) : input.statisticalAssumptionSource.empty(),
          "012_35 invalid statistical assumption declaration");
  const auto& victim=source.channel(victimId);
  require(victim.differential(),"012_35 differential victim required");
  for(double z:source.cache().meta().reference)
    require(z==50.,"012_35 matched 50-ohm physical references required");
  auto thru=extract(source,victimId,Metric::IL,false,{},control);
  validateCom120F8DirectGrid(thru.frequencyHz);
  const auto n=thru.frequencyHz.size();
  require(n<=32769 && std::max<size_t>(1,input.aggressors.size())<=
       (16u*1024u*1024u)/26/n,"012_35 pair numerical work budget");
  const double ui=1./final8023ck120F8().symbolRateHz;
  const double df=thru.frequencyHz[1];
  std::vector<const ComBoardAggressorChoice*> order;
  for(const auto& a:input.aggressors) order.push_back(&a);
  std::sort(order.begin(),order.end(),[](auto a,auto b){return a->id<b->id;});
  std::unordered_set<std::string> ids;
  std::unordered_map<std::string,bool> paths;
  std::vector<ComplexTrace> raw;
  std::ostringstream key;
  key.imbue(std::locale::classic());
  key<<std::hexfloat;
  const auto add=[&](std::string_view s){key<<s.size()<<':'<<s<<';';};
  add(replay?kHistoricalAggressorOverlay:"012_35-matched-board");add(victimId);
  if(replay)add(replay->provenance.settingsIdentity);
  key<<input.independentEquiprobablePam4Declared<<';';add(input.statisticalAssumptionSource);
  for(const auto* a:order) {
    check(cancelled);
    require(bounded(a->id)&&bounded(a->channelId)&&ids.insert(a->id).second &&
            a->channelId!=victimId &&
            (a->coupling==ComCoupling::Next||a->coupling==ComCoupling::Fext),
            "012_35 invalid aggressor identity/role");
    const auto& ch=source.channel(a->channelId);
    require(ch.differential(),"012_35 differential aggressor required");
    for(int v:{victim.nearP,victim.nearN,victim.farP,victim.farN})
      for(int q:{ch.nearP,ch.nearN,ch.farP,ch.farN})
        require(v!=q,"012_35 overlapping victim/aggressor ports");
    const bool next=a->coupling==ComCoupling::Next;
    const int sp=next?ch.farP:ch.nearP,sn=next?ch.farN:ch.nearN;
    const std::string path=std::to_string(std::min(sp,sn))+":"+
                           std::to_string(std::max(sp,sn));
    // Canonical physical pair also detects an alias with swapped P/N.
    const auto [prior,inserted]=paths.emplace(path,a->repeatedPathDeclared);
    require(inserted||(prior->second&&a->repeatedPathDeclared),
            "012_35 undeclared repeated path");
    auto trace=extract(source,victimId,next?Metric::NEXT:Metric::FEXT,next,
                       std::string_view(a->channelId),control);
    require(trace.frequencyHz==thru.frequencyHz && trace.stimulusOhm==100. &&
            trace.responseOhm==100.,"012_35 incompatible source grid or reference");
    for(auto z:trace.transfer)require(std::abs(z)<=1.+1e-7,"012_35 nonpassive scalar coupling");
    raw.push_back(std::move(trace));
    add(a->id);add(a->channelId);key<<next<<';'<<a->repeatedPathDeclared<<';';
  }
  ComBoardCrosstalkResult data;
  if(replay)data.model="EXPLORATORY_HISTORICAL_DIE_XTALK_MATCHED_UNUSED_PORTS";
  // Phase 2: separate per-case EQ, absolute decision grid and complex transfer.
  for(size_t k=0;k<2;++k) {
    check(cancelled);
    const auto& choice=input.cases[k];
    const char* which=k==0?"TEST1":"TEST2";
    require(choice.testCase==which && std::isfinite(choice.decisionTimeSeconds),
            "012_35 wrong test case or decision time");
    Com120F8EqualizerChoice rx;
    rx.ctleDcGainDb=choice.ctleDcGainDb;rx.ctleDcGain2Db=choice.ctleDcGain2Db;
    validateCom120F8Equalizer(rx);
    auto& out=data.cases[k];
    out.testCase=which;
    add(which);key<<choice.decisionTimeSeconds<<';'<<rx.ctleDcGainDb<<';'<<rx.ctleDcGain2Db<<';';
    for(int t=-3;t<=9;++t)out.timeSeconds.push_back(choice.decisionTimeSeconds+t*ui);
    const ComFixedVictimCaseData* victimCase=nullptr;
    if(replay) {
      victimCase=k==0?&replay->value->test1:&replay->value->test2;
      require(victimCase->testCase==which && victimCase->pulse.timeSeconds==out.timeSeconds,
              "012_35 victim decision grid mismatch");
      out.victimSettingsIdentity=victimCase->settingsIdentity;
    }
    ComPreparedData prepared;
    prepared.frequencyHz=thru.frequencyHz;
    prepared.conditionedThru=thru.transfer; // validation anchor only, never a victim decision
    std::vector<std::optional<double>> powers;
    for(size_t j=0;j<order.size();++j) {
      check(cancelled);
      const auto& a=*order[j];const auto& ac=a.cases[k];
      require(std::isfinite(ac.relativeDelaySeconds)&&ac.relativeDelaySeconds>=0. &&
              ac.relativeDelaySeconds*df<=.45,"012_35 invalid relative delay");
      if(ac.declaredVoltsPerUnitSymbol)
        require(std::isnormal(*ac.declaredVoltsPerUnitSymbol)&&
                *ac.declaredVoltsPerUnitSymbol>0.&&bounded(ac.calibrationSource),
                "012_35 invalid declared voltage calibration");
      else require(ac.calibrationSource.empty(),"012_35 calibration without voltage");
      // Board evidence must not be reused at the die plane. For a declared
      // die scale, bind the same actual source/case and victim decision-plane
      // calibration; a caller declaration still cannot establish physical gain.
      if(replay && ac.declaredVoltsPerUnitSymbol) {
        auto expected=replay->provenance;
        expected.settingsIdentity=victimCase->settingsIdentity;
        require(victimCase->voltage && ac.dieCalibrationProvenance &&
            sameSource(*ac.dieCalibrationProvenance,expected) &&
            bounded(ac.receiverDecisionPlane) && bounded(ac.calibrationIdentity) &&
            ac.receiverDecisionPlane==victimCase->voltage->before.receiverDecisionPlane &&
            ac.calibrationIdentity==victimCase->voltage->before.calibrationIdentity,
            "012_35 missing/stale die calibration or incompatible victim plane");
      } else require(!ac.dieCalibrationProvenance && ac.receiverDecisionPlane.empty() &&
                     ac.calibrationIdentity.empty(),"012_35 unexpected die calibration");
      auto eq=rx;eq.txFfe=ac.txFfe;
      validateCom120F8Equalizer(eq);
      for(double tap:ac.txFfe)key<<tap<<';';
      key<<ac.relativeDelaySeconds<<';'<<bool(ac.declaredVoltsPerUnitSymbol)<<';';
      if(ac.declaredVoltsPerUnitSymbol)key<<*ac.declaredVoltsPerUnitSymbol<<';';
      add(ac.calibrationSource);add(ac.receiverDecisionPlane);add(ac.calibrationIdentity);
      const auto trace=replay?historicalPath(source,victim,source.channel(a.channelId),
          a.coupling==ComCoupling::Next,k,raw[j],control):raw[j];
      ComInput x;x.thru=thru;x.aggressors.push_back({a.id,a.coupling,trace});
      auto r=prepareCom120F8ReceiverSpectra(std::move(x),eq,{},cancelled);
      require(r.usable()&&r.value->aggressors.size()==1,"012_35 conditioning failed");
      auto spectrum=std::move(r.value->aggressors.front());
      for(size_t i=0;i<n;++i) {
        if(!(i&1023u))check(cancelled);
        const double f=prepared.frequencyHz[i];
        spectrum.conditionedTransfer[i]*=com120F8TxTransitionFilter(f)*
          std::polar(1.,-2.*std::numbers::pi*f*ac.relativeDelaySeconds);
      }
      // The PSD calculation uses the same once-conditioned complex spectrum.
      powers.emplace_back();
      if(input.independentEquiprobablePam4Declared&&ac.declaredVoltsPerUnitSymbol)
        powers.back()=measuredPsdPower(prepared.frequencyHz,spectrum.conditionedTransfer,
                                      *ac.declaredVoltsPerUnitSymbol,ui,cancelled);
      prepared.aggressors.push_back(std::move(spectrum));
    }
    auto r=calculateComInterferencePulses(prepared,out.timeSeconds,{},cancelled);
    require(r.usable(),"012_35 pulse primitive unavailable");
    bool totalAvailable=input.independentEquiprobablePam4Declared;
    // Phase 3: reconstruct signed pulses and independently declared volts.
    // No DFE subtraction on aggressor samples, no PSD/RMS double counting.
    double total=0.,totalPower=0.;
    for(size_t j=0;j<order.size();++j) {
      const auto& a=*order[j];const auto& ac=a.cases[k];const auto& ch=source.channel(a.channelId);
      ComBoardAggressorResult one;
      one.pulse=std::move(r.value->aggressors[j]);
      require(one.pulse.id==a.id && one.pulse.unitSymbolPulse.size()==13,
              "012_35 pulse identity mismatch");
      one.channelId=a.channelId;one.calibrationSource=ac.calibrationSource;
      const bool next=a.coupling==ComCoupling::Next;
      one.stimulusPorts={next?ch.farP:ch.nearP,next?ch.farN:ch.nearN};
      one.responsePorts={victim.farP,victim.farN};
      if(replay) {
        const auto profile=final8023ck120F8();
        one.aggressorPackageMm=next?profile.rxPackageLengthMm[k]:profile.txPackageLengthMm[k];
        one.victimRxPackageMm=profile.rxPackageLengthMm[k];
        one.receiverDecisionPlane=ac.receiverDecisionPlane;
        one.calibrationIdentity=ac.calibrationIdentity;
      }
      one.measuredBandPsdPowerV2=powers[j];
      if(powers[j]) {
        totalPower+=*powers[j];
        require(std::isfinite(totalPower),"012_35 total PSD overflow");
      }
      if(ac.declaredVoltsPerUnitSymbol) {
        one.declaredEquivalentV.emplace();
        double rss=0.;
        for(size_t i=0;i<13;++i) {
          const double v=scaleSample(one.pulse.unitSymbolPulse[i],*ac.declaredVoltsPerUnitSymbol);
          (*one.declaredEquivalentV)[i]=v;rss=std::hypot(rss,v);
        }
        if(input.independentEquiprobablePam4Declared) {
          one.observedPam4RmsV=rss*std::sqrt(5./9.);
          total=std::hypot(total,*one.observedPam4RmsV);
          require(std::isfinite(total),"012_35 RSS overflow");
        }
      } else totalAvailable=false;
      out.aggressors.push_back(std::move(one));
    }
    if(totalAvailable) {
      out.observedIndependentPam4RmsV=total;
      out.independentMeasuredBandPsdPowerV2=totalPower;
    }
  }
  check(cancelled);
  if(!source.current(observedLiveIdentity))return stale();
  const auto& id=source.identity();
  Result<ComBoardCrosstalkResult> out;
  out.status=Status::Warning;out.value=std::move(data);
  out.provenance={id.sha256,id.revision,id.mappingIdentity,
      replay?"com-historical-die-fixed-crosstalk":"com-matched-board-fixed-crosstalk",
      replay?"012_35-die-1":"012_35-board-2",key.str(),
      id.workspaceGeneration,id.mappingGeneration};
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
      replay?"Historical draft package role lengths with matched unused board ports; declared voltage, observed samples and separate measured-band PSD only.":
      "Matched-board wave paths with declared conditioning, voltage and optional independent PAM4 assumption; observed 13 samples and separate measured-band PSD."});
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED",
      "Final PHY topology, physical calibration and official PSD/statistical definition unqualified. No normative COM."});
  return out;
}
} // namespace

Result<ComBoardCrosstalkResult> calculateComMatchedBoardCrosstalkFromSource(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& observedLiveIdentity,const ComBoardCrosstalkInput& input,
    Control* control) {
  return calculateFixedCrosstalk(source,victimId,observedLiveIdentity,input,control,nullptr);
}

Result<ComHistoricalFixedCrosstalkResult> calculateComHistoricalFixedCrosstalkFromSource(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& observedLiveIdentity,const ComHistoricalFixedCrosstalkInput& input,
    Control* control) {
  if(control)control->check();
  Result<ComHistoricalFixedCrosstalkResult> out;
  if(!source.current(observedLiveIdentity)){out.status=Status::Stale;return out;}
  require(input.packageAssumption==kHistoricalAggressorOverlay &&
      input.aggressors.inventoryConfirmed && input.aggressors.aggressors.size()<=kMaxAggressors,
      "012_35 historical role topology must be explicitly acknowledged");
  for(size_t k=0;k<2;++k) {
    const auto& a=input.aggressors.cases[k];const auto& v=input.victimCases[k];
    require(a.testCase==v.testCase&&a.decisionTimeSeconds==v.decisionTimeSeconds&&
            a.ctleDcGainDb==input.victimEq.ctleDcGainDb&&
            a.ctleDcGain2Db==input.victimEq.ctleDcGain2Db,
            "012_35 fixed victim timing/RX EQ mismatch");
  }
  // Replay 012_34 from source instead of accepting caller-constructed results.
  auto victim=calculateComFixedVictimFromSource(source,victimId,input.victimEq,
       observedLiveIdentity,input.victimCases[0],input.victimCases[1],0.,control);
  if(victim.status==Status::Stale){out.status=Status::Stale;return out;}
  require(victim.usable()&&victim.status==Status::Warning,"012_35 victim replay unavailable");
  auto xtalk=calculateFixedCrosstalk(source,victimId,observedLiveIdentity,
                                   input.aggressors,control,&victim);
  if(xtalk.status==Status::Stale){out.status=Status::Stale;return out;}
  require(xtalk.usable(),"012_35 historical crosstalk unavailable");
  out.status=Status::Warning;out.provenance=xtalk.provenance;
  out.diagnostics=std::move(xtalk.diagnostics);
  out.value=ComHistoricalFixedCrosstalkResult{std::move(*victim.value),std::move(*xtalk.value)};
  return out;
}
} // namespace si::channel
