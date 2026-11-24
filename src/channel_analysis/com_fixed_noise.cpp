#include "si/channel_analysis/com_fixed_noise.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>

namespace si::channel {
namespace {
void require(bool ok,const char* why){if(!ok)throw si::Error(why);}
void check(Control* c){if(c)c->check();}
bool bounded(const std::string& s){return !s.empty()&&s.size()<=256;}
bool sameBits(double a,double b){
  return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
}
void append(std::ostringstream& o,const std::string& s){
  constexpr char hex[]="0123456789abcdef";
  o<<s.size()<<":";
  for(unsigned char b:s)o<<hex[b>>4]<<hex[b&15];
  o<<"/";
}
Result<ComFixedNoiseData> stale(){
  Result<ComFixedNoiseData> r;r.status=Status::Stale;
  r.diagnostics.push_back({Severity::Error,"SOURCE_STALE","012_36 source is stale"});
  return r;
}
unsigned owners(ComNoiseRepresentation choice){
  switch(choice){
    case ComNoiseRepresentation::PreDfeIsi:
    case ComNoiseRepresentation::ResidualIsi:return 1u;
    case ComNoiseRepresentation::CrosstalkObserved:
    case ComNoiseRepresentation::CrosstalkPsd:return 2u|4u;
    case ComNoiseRepresentation::NextObserved:
    case ComNoiseRepresentation::NextPsd:return 2u;
    case ComNoiseRepresentation::FextObserved:
    case ComNoiseRepresentation::FextPsd:return 4u;
    case ComNoiseRepresentation::RxGaussian:
    case ComNoiseRepresentation::ReferenceRxRightEndpoint:return 8u;
    case ComNoiseRepresentation::RandomJitter:
    case ComNoiseRepresentation::ReferenceObservedRj:return 16u;
    case ComNoiseRepresentation::DualDirac:
    case ComNoiseRepresentation::ReferenceObservedDd:return 32u;
    case ComNoiseRepresentation::CombinedGaussian:return 8u|16u;
    case ComNoiseRepresentation::JitterMixture:return 8u|16u|32u;
  }
  throw si::Error("012_36 unsupported noise representation");
}

// Use the same source-backed rectangular pulse, not residual DFE taps.
// Equation 93A-28 style central difference on only the observed 13 UIs.
// A full phase vector is not inferred; its absent tails remain unqualified.
ComObservedReferenceJitter observedJitter(
    const ComPreparedData& spec,const ComFixedVictimCaseData& v,
    const Provenance& p,const std::atomic_bool* cancelled){
  const auto profile=final8023ck120F8();
  const double ui=1./profile.symbolRateHz,delta=ui/32.;
  const double scale=v.voltage->before.voltsPerUnitSymbol;
  std::vector<double> times;
  for(double t:v.pulse.timeSeconds){times.push_back(t-delta);times.push_back(t+delta);}
  const auto response=calculateCom120F8RectangularPulse(spec,times,p,cancelled);
  require(response.usable()&&response.value->unitSymbolPulse.size()==26,
    "012_36 observed reference jitter samples unavailable");
  ComObservedReferenceJitter out;double rss=0.;
  // A signed tap is V per UI. The DD vector owns the same jitter source as
  // the local two-atom diagnostic but uses the PAM4 symbol law, not two atoms.
  for(size_t k=0;k<13;++k){
    const double a=response.value->unitSymbolPulse[2*k];
    const double b=response.value->unitSymbolPulse[2*k+1];
    const double diff=b-a;
    const double width=times[2*k+1]-times[2*k];
    require(std::isfinite(width)&&width>0.&&std::abs(width/(2.*delta)-1.)<1e-6,
      "012_36 unresolved reference jitter sample spacing");
    const double unitSlope=diff*(ui/width);
    const double slope=unitSlope*scale;
    require(std::isfinite(slope)&&(std::fpclassify(diff)==FP_ZERO||
      (std::isnormal(unitSlope)&&std::isnormal(slope))),
      "012_36 reference jitter slope dynamic range");
    const double dd=slope*profile.dualDiracJitterPeakUi;
    require(std::isfinite(dd)&&(std::fpclassify(slope)==FP_ZERO||std::isnormal(dd)),
      "012_36 reference DD tap dynamic range");
    out.hJVoltagePerUi[k]=slope;out.dualDiracTapV[k]=dd;
    rss=std::hypot(rss,slope);
  }
  const double sigmaX=std::sqrt(5./9.);
  out.randomJitterRmsV=rss*sigmaX*profile.randomJitterRmsUi;
  out.dualDiracRmsV=rss*sigmaX*profile.dualDiracJitterPeakUi;
  require(std::isfinite(rss)&&std::isfinite(out.randomJitterRmsV)&&
    std::isfinite(out.dualDiracRmsV)&&(std::fpclassify(rss)==FP_ZERO||
    (std::isnormal(out.randomJitterRmsV)&&std::isnormal(out.dualDiracRmsV))),
    "012_36 reference jitter RMS dynamic range");
  return out;
}
}
void validateComFixedNoiseRepresentations(const std::vector<ComNoiseRepresentation>& choices){
  unsigned seen=0;
  for(auto c:choices){
    const auto mask=owners(c);
    require((seen&mask)==0,"012_36 duplicate physical noise ownership");
    seen|=mask;
  }
}
Result<ComFixedNoiseData> calculateComFixedNoiseFromSource(
    const SourceSnapshot& source,std::string_view victimId,
    const SourceIdentity& live,const ComFixedNoiseInput& input,Control* control){
  check(control);
  if(!source.current(live))return stale();
  const auto* cancelled=control?&control->cancelled:nullptr;
  // Replay immutable source and choices; do not accept a detached waveform.
  const auto phys=prepareCom120F8HistoricalCausalFromSource(source,victimId,input.eq,live,0.,control);
  if(phys.status==Status::Stale)return stale();
  require(phys.usable()&&phys.status==Status::Warning,"012_36 physical response unavailable");
  const auto victim=calculateComFixedVictimFromSource(source,victimId,input.eq,live,
      input.victim[0],input.victim[1],0.,control);
  if(victim.status==Status::Stale)return stale();
  require(victim.usable()&&victim.status==Status::Warning,"012_36 victim replay unavailable");
  // Optional historical crosstalk is replayed by its own typed producer.
  std::optional<Result<ComHistoricalFixedCrosstalkResult>> xtalk;
  if(input.crosstalk){
    ComHistoricalFixedCrosstalkInput x;
    x.aggressors=*input.crosstalk;x.victimEq=input.eq;x.victimCases=input.victim;
    x.packageAssumption=input.historicalPackageAssumption;
    xtalk=calculateComHistoricalFixedCrosstalkFromSource(source,victimId,live,x,control);
    if(xtalk->status==Status::Stale)return stale();
    require(xtalk->usable()&&xtalk->status==Status::Warning,"012_36 historical crosstalk unavailable");
  }else require(input.historicalPackageAssumption.empty(),"012_36 overlay without inventory");
  const auto profile=final8023ck120F8();
  const double ui=1./profile.symbolRateHz, delta=ui/32.;
  Result<ComFixedNoiseData> out;out.value.emplace();
  std::ostringstream pair;pair.imbue(std::locale::classic());
  append(pair,victim.provenance.settingsIdentity);
  for(size_t k=0;k<2;++k){
    check(control);
    const auto& v=k==0?victim.value->test1:victim.value->test2;
    const auto& spec=k==0?phys.value->test1:phys.value->test2;
    const auto& n=input.noise[k];
    auto& c=out.value->cases[k];
    require(v.voltage.has_value(),"012_36 missing declared receiver voltage");
    const auto& scale=v.voltage->before;
    require(bounded(n.noiseSource)&&bounded(n.receiverDecisionPlane)&&
      n.independentRxRjDjDeclared&&bounded(n.independenceDeclaration)&&n.receiverDecisionPlane==scale.receiverDecisionPlane,
      "012_36 missing noise source, independence declaration or wrong receiver plane");
    require(n.plane==ComFixedNoisePlane::RxFilterInput||n.plane==ComFixedNoisePlane::CtleInput,
      "012_36 unsupported injection model");
    require(std::isfinite(n.oneSidedPsdV2PerHz)&&n.oneSidedPsdV2PerHz>=0.,
      "012_36 invalid one-sided PSD");
    require(spec.frequencyHz.size()>=2&&spec.frequencyHz.size()<=8193,
      "012_36 noise integration grid budget");
    c.testCase=v.testCase;c.victimSettingsIdentity=v.settingsIdentity;
    c.noiseSource=n.noiseSource;c.receiverDecisionPlane=n.receiverDecisionPlane;
    c.independenceDeclaration=n.independenceDeclaration;c.injectionPlane=n.plane;
    c.residualIsiObservedRmsV=v.voltage->isiAfterV.pam4Rms;
    if(xtalk){
      const auto& x=xtalk->value->crosstalk.cases[k];
      require(x.victimSettingsIdentity==v.settingsIdentity&&x.timeSeconds==v.pulse.timeSeconds,
        "012_36 crosstalk victim mismatch");
      for(const auto& a:x.aggressors){
        require(a.pulse.id!=n.noiseSource&&a.channelId!=n.noiseSource,
          "012_36 noise source duplicates crosstalk identity");
        if(a.declaredEquivalentV)require(a.receiverDecisionPlane==n.receiverDecisionPlane,
          "012_36 crosstalk receiver plane mismatch");
        c.aggressorOwners.push_back({a.pulse.id,a.channelId,a.pulse.coupling,
          a.stimulusPorts,a.responsePorts,a.observedPam4RmsV,a.measuredBandPsdPowerV2});
      }
      c.crosstalkObservedRmsV=x.observedIndependentPam4RmsV;
      c.crosstalkPsdPowerV2=x.independentMeasuredBandPsdPowerV2;
      c.crosstalkSettingsIdentity=xtalk->provenance.settingsIdentity;
    }
    const double td=input.victim[k].decisionTimeSeconds;
    c.localTimeSeconds={td-delta,td,td+delta};
    require(c.localTimeSeconds[0]<td&&td<c.localTimeSeconds[2]&&
      std::abs((td-c.localTimeSeconds[0])/delta-1.)<1e-6&&
      std::abs((c.localTimeSeconds[2]-td)/delta-1.)<1e-6,
      "012_36 unresolvable local stencil");
    auto p=phys.provenance;
    std::ostringstream id;id.imbue(std::locale::classic());
    append(id,v.settingsIdentity);append(id,c.crosstalkSettingsIdentity);
    append(id,n.noiseSource);append(id,n.receiverDecisionPlane);
    append(id,n.independenceDeclaration);
    // Case identity records the extra observed-PAM4 assumption separately.
    id<<"012_36-v2/"<<static_cast<int>(n.plane)<<"/"<<n.independentEquiprobablePam4Declared<<"/"<<std::hexfloat
      <<n.oneSidedPsdV2PerHz<<"/"<<n.diagnosticTailProbability<<"/"<<delta<<"/";
    p.settingsIdentity=id.str();append(pair,p.settingsIdentity);
    const auto pulse=calculateCom120F8RectangularPulse(spec,c.localTimeSeconds,p,cancelled);
    require(pulse.usable()&&pulse.value->unitSymbolPulse.size()==3&&
      sameBits(pulse.value->unitSymbolPulse[1],v.pulse.unitSymbolPulse[3]),
      "012_36 local center differs from replayed victim");
    for(double a:pulse.value->unitSymbolPulse){
      double volts=a*scale.voltsPerUnitSymbol;
      require(std::isfinite(volts)&&(std::fpclassify(a)==FP_ZERO||std::isnormal(volts)),
        "012_36 local voltage overflow or underflow");
      c.localVoltageV.push_back(volts);
    }
    // PSD is already expressed in volts; never multiply by victim scale.
    ComReceiverNoiseInput ni;
    ni.frequencyHz=spec.frequencyHz;ni.oneSidedPsdV2PerHz=n.oneSidedPsdV2PerHz;
    ni.oneSidedTailProbability=n.diagnosticTailProbability;
    for(double f:ni.frequencyHz){
      check(control);
      ni.noiseTransfer.push_back(n.plane==ComFixedNoisePlane::RxFilterInput?
        com120F8ReceiverNoiseFilter(f):
        com120F8ReceiverTransfer(f,input.eq.ctleDcGainDb,input.eq.ctleDcGain2Db));
    }
    ni.queryVoltageV={0.};
    c.receiverNoise=calculateComReceiverNoise(ni,p,cancelled);
    require(c.receiverNoise.usable(),"012_36 noise unavailable");
    // 4.17 reference-style right-endpoint eta0 integral is meaningful only
    // for a source explicitly declared before CTLE and RX filtering.
    if(n.plane==ComFixedNoisePlane::CtleInput){
      auto rp=p;rp.settingsIdentity+="/eta0-right-endpoint/";
      ni.quadrature=ComReceiverNoiseQuadrature::RightEndpoint;
      c.referenceRightEndpointNoise=calculateComReceiverNoise(ni,rp,cancelled);
      require(c.referenceRightEndpointNoise->usable(),"012_36 reference-style RX noise unavailable");
    }
    if(n.independentEquiprobablePam4Declared)
      c.observedReferenceJitter=observedJitter(spec,v,p,cancelled);
    // The legacy local mixture remains separate; do not feed observed h_J
    // or the right-endpoint alternative back into it as additional noise.
    ComJitterInput ji;
    ji.symbolRateBd=profile.symbolRateHz;ji.randomJitterRmsUi=profile.randomJitterRmsUi;
    ji.dualDiracPeakUi=profile.dualDiracJitterPeakUi;
    ji.timeSeconds=c.localTimeSeconds;ji.receiverVoltageV=c.localVoltageV;
    ji.decisionTimeSeconds=td;ji.voltageReferencePlane=scale.receiverDecisionPlane;
    ji.voltageCalibrationIdentity=scale.calibrationIdentity;
    ji.receiverNoise=c.receiverNoise;ji.oneSidedTailProbability=n.diagnosticTailProbability;
    ji.queryVoltageV={0.};
    c.jitter=calculateComJitter(ji,p,cancelled);
    require(c.jitter.usable(),"012_36 jitter unavailable");
  }
  check(control);
  if(!source.current(live))return stale();
  out.status=Status::Warning;out.provenance=phys.provenance;
  out.provenance.settingsIdentity=pair.str();
  out.provenance.algorithm="com-fixed-eq-local-noise-jitter";
  out.provenance.version="012_36-2";
  out.diagnostics.push_back({Severity::Warning,"NOT_IEEE_COM",
    "Declared receiver-equivalent local-linear timing and finite-band noise only; no aggregate ISI/XTALK distribution or COM."});
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED",
    "Normative noise placement, full jitter statistics and final PHY calibration are unqualified."});
  return out;
}
} // namespace si::channel
