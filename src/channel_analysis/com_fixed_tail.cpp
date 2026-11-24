#include "si/channel_analysis/com_fixed_tail.hpp"
#include <cmath>
#include <set>
#include <utility>

namespace si::channel {
namespace {
void require(bool b,const char* why){if(!b)throw si::Error(why);}
void check(const std::atomic_bool* c){if(c&&c->load(std::memory_order_relaxed))throw si::Cancelled();}
bool named(const std::string& s){return !s.empty()&&s.size()<=256&&s.find_first_not_of(" \t\r\n")!=std::string::npos;}
bool nonnegative(double x){return x==0.||(std::isnormal(x)&&x>0.);}
bool closeRms(double a,double b){return a==b||(a>0.&&b>0.&&std::abs(a-b)/std::max(a,b)<=2e-12);}
}
Result<ComFixedTailData> calculateComFixedTailSynthetic(const ComFixedTailInput& in,
    const Provenance& p,const std::atomic_bool* cancelled){
  check(cancelled);
  const auto& f=in.fom;const auto& id=f.identity;
  require(id.sourceIdentity==p.sourceSha256&&id.settingsIdentity==p.settingsIdentity&&
    named(id.decisionPlane)&&named(in.calibrationIdentity)&&
    in.independentGaussianRxTxRjDeclared&&named(in.rjOwner)&&named(in.rjEvidence)&&
    nonnegative(in.rjRmsV)&&in.rjExplicitlyInactive==(in.rjRmsV==0.)&&in.taps.size()<=6,
    "012_38 invalid synthetic tail identity, model or budget");
  // Recalculate the validated synthetic FOM component contract. Do not accept
  // an externally supplied FOM result or infer a distribution from its RMS.
  const auto fom=calculateComFixedFomSynthetic(f);
  check(cancelled);
  std::array<std::set<std::string>,3> owners;
  std::array<double,3> rms{0.,in.rjRmsV,0.};
  owners[1].insert(in.rjOwner);
  const double pam4Rms=std::sqrt(5./9.);
  for(const auto& t:in.taps){
    check(cancelled);
    require(t.identity==id&&named(t.owner)&&named(t.evidence)&&std::isnormal(t.voltageV),
      "012_38 invalid signed tap or provenance");
    size_t k=0;
    switch(t.role){
      case ComFixedTailRole::ResidualIsi:k=0;break;
      case ComFixedTailRole::SampledDj:k=1;break;
      case ComFixedTailRole::Next:case ComFixedTailRole::Fext:k=2;break;
      default:throw si::Error("012_38 unsupported tap role");
    }
    require(owners[k].insert(t.owner).second,"012_38 duplicate tap owner");
    rms[k]=std::hypot(rms[k],pam4Rms*t.voltageV);
  }
  for(size_t k=0;k<3;++k){
    const auto& n=*f.noise[k];
    const std::set<std::string> expected(n.owners.begin(),n.owners.end());
    // An explicitly inactive ISI/XT group retains its declaration but has no
    // stochastic taps. Jitter always has a named RJ declaration, even at zero.
    if(k!=1&&n.explicitlyInactive)
      require(owners[k].empty(),"012_38 inactive group contains a tap");
    else require(owners[k]==expected,"012_38 distribution owner mismatch");
    require(closeRms(rms[k],n.rmsV),"012_38 distribution/RMS mismatch");
  }
  require(f.noise[3]->owners.size()==1,"012_38 RX Gaussian must have one owner");
  const double tx=fom.value->rmsV[4],rx=f.noise[3]->rmsV;
  const double rxTx=std::hypot(rx,tx);
  require(std::isnormal(rxTx),"012_38 combined RX/TX outside supported range");
  // The legacy receiver slot carries the explicitly independent RX+TX
  // Gaussian norm ONLY inside this synthetic adapter; RJ remains separate.
  // Sampled DJ uses PAM4 taps, not the legacy single Dual-Dirac peak.
  auto component=[&](ComJointComponentKind kind,double v,std::string key,bool receiver){
    Result<ComJointVoltageEvidence> out;out.status=Status::Warning;out.provenance=p;
    out.provenance.algorithm="com-fixed-tail-synthetic-input";out.provenance.version="012_38";
    out.value=ComJointVoltageEvidence{v,kind,std::move(key),id.testCase,id.decisionPlane,
      in.calibrationIdentity,true,receiver};return out;
  };
  ComJointTailInput joint;
  joint.oneSidedTailProbability=in.oneSidedProbability;
  joint.receiverGaussian=component(ComJointComponentKind::ReceiverGaussian,rxTx,"rx-plus-tx",true);
  joint.randomJitterGaussian=component(ComJointComponentKind::RandomJitterGaussian,in.rjRmsV,"rj",false);
  joint.dualDiracPeak=component(ComJointComponentKind::DualDiracPeak,0.,"legacy-dd-inactive",false);
  for(size_t k=0;k<in.taps.size();++k)
    joint.pam4Taps.push_back(component(ComJointComponentKind::IndependentPam4Tap,
      in.taps[k].voltageV,"tap-"+std::to_string(k),false));
  const auto result=calculateComJointTail(joint,id.testCase,p,cancelled);
  check(cancelled);
  Result<ComFixedTailData> out;out.status=Status::Warning;out.provenance=result.provenance;
  out.provenance.algorithm="com-fixed-tail-synthetic";out.provenance.version="012_38";
  out.diagnostics=result.diagnostics;
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED",
    "Continuous synthetic tail is not reference discrete PDF/CDF or qualified DER."});
  out.value=ComFixedTailData{id,in.oneSidedProbability,tx,std::hypot(rxTx,in.rjRmsV),*result.value};
  return out;
}
Result<ComFixedTailData> assessComFixedTailSource(const Result<ComFixedNoiseData>& replay,
    std::string_view test,Control* control){
  const auto gate=assessComFixedFomSource(replay,test,control);
  Result<ComFixedTailData> out;out.status=gate.status;out.provenance=gate.provenance;
  out.provenance.algorithm="com-fixed-tail-source-eligibility";out.provenance.version="012_38";
  out.diagnostics=gate.diagnostics;
  out.diagnostics.push_back({Severity::Warning,"REFERENCE_BLOCKED",
    "No qualified final physical PDF, sampled jitter span or DER input contract."});
  return out;
}
ComTailGridCrossing syntheticStrictLowerCdfCrossing(const std::vector<double>& masses,
    int firstBin,double binV,double probability,const std::atomic_bool* cancelled){
  check(cancelled);
  require(!masses.empty()&&masses.size()<=8192&&firstBin>=-8192&&firstBin<=8192&&
    std::isnormal(binV)&&binV>0.&&std::isfinite(probability)&&probability>=1e-12&&probability<=.1,
    "012_38 invalid synthetic grid");
  double cumulative=0.;std::optional<ComTailGridCrossing> crossing;
  for(size_t k=0;k<masses.size();++k){
    check(cancelled);
    const double mass=masses[k];
    require(std::isfinite(mass)&&mass>=0.&&mass<=1.,"012_38 invalid grid mass");
    const double v=static_cast<double>(firstBin+static_cast<int>(k))*binV;
    require(std::isfinite(v),"012_38 grid voltage overflow");
    const double before=cumulative;
    cumulative+=mass;
    if(!crossing&&cumulative>probability)
      crossing=ComTailGridCrossing{k,v,std::abs(v),before,cumulative};
  }
  require(std::abs(cumulative-1.)<=1e-12&&crossing.has_value(),"012_38 incomplete grid mass");
  check(cancelled);return *crossing;
}
} // namespace si::channel
