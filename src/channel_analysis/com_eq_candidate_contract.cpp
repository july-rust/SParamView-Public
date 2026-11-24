#include "si/channel_analysis/com_eq_candidate_contract.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace si::channel {
namespace {
void require(bool ok,const char* message) { if(!ok) throw si::Error(message); }
void check(const std::atomic_bool* c) {
  if(c && c->load(std::memory_order_relaxed)) throw si::Cancelled();
}
auto ranges() {
  const auto p=final8023ck120F8();
  return std::array<Com120F8DiscreteRange,6>{p.txNoncursor[0],
    p.txNoncursor[1],p.txNoncursor[2],p.txNoncursor[3],
    p.ctleDcGainDb,p.ctleDcGain2Db};
}
auto sizes() {
  const auto r=ranges();
  std::array<int,6> n{};
  for(size_t i=0;i<n.size();++i)
    n[i]=1+static_cast<int>(std::llround((r[i].maximum-r[i].minimum)/r[i].step));
  return n;
}
double grid(const Com120F8DiscreteRange& r,int i) {
  // Values in the audited profile are integral hundredths; no cumulative
  // float addition. Canonicalization only for this immutable profile.
  return static_cast<double>(std::llround(r.minimum*100.)+
        i*std::llround(r.step*100.))/100.;
}
constexpr std::array<size_t,4> slots{0,1,2,4};
ComEqPhase phaseFor(const ComEqPhaseContract& c,int offset) {
  require(c.anchorMode==ComEqAnchor::MuellerMuller ||
    c.anchorMode==ComEqAnchor::Peak || c.anchorMode==ComEqAnchor::MaxDv,
    "012_40 unsupported anchor");
  require(c.order==ComEqPhaseOrder::FullSweep || c.order==ComEqPhaseOrder::Middle,
    "012_40 unsupported phase ordering");
  // Exact representability of indices in binary64 time conversion.
  require(c.sampleCount>0 && c.sampleCount<= (uint64_t{1}<<53) &&
    c.anchorOneBased>0 && c.anchorOneBased<=c.sampleCount &&
    c.firstOffset<=c.lastOffset && offset>=c.firstOffset && offset<=c.lastOffset,
    "012_40 invalid sample contract");
  const auto one=static_cast<int64_t>(c.anchorOneBased)+static_cast<int64_t>(offset);
  require(one>=1 && static_cast<uint64_t>(one)<=c.sampleCount,
    "012_40 sample index outside supplied waveform");
  const auto zero=static_cast<uint64_t>(one-1);
  const auto p=final8023ck120F8();
  return {offset,zero,static_cast<double>(zero)/(p.symbolRateHz*p.samplesPerUi)};
}
bool hexSha(const std::string& s) {
  return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
  });
}
}
size_t comEqRawCandidateCount() {
  size_t count=1;
  for(int n:sizes()) count*=static_cast<size_t>(n);
  return count;
}
ComEqCandidate comEqCandidateAt(size_t ordinal,
    const std::optional<ComEqCtleMask>& mask,const std::atomic_bool* cancelled) {
  check(cancelled);
  require(ordinal<comEqRawCandidateCount(),"012_40 ordinal out of range");
  const auto n=sizes(); const auto r=ranges();
  ComEqCandidate out;
  out.ctleMaskSupplied=mask.has_value();
  // Reference outer gDC, gDC2; Full_Grid_Matrix last FFE axis fastest.
  for(size_t axis:std::array<size_t,6>{3,2,1,0,5,4}) {
    out.indices.index[axis]=static_cast<int>(ordinal%static_cast<size_t>(n[axis]));
    ordinal/=static_cast<size_t>(n[axis]);
  }
  Com120F8EqualizerChoice choice;
  long long magnitude=0;
  for(size_t i=0;i<4;++i) {
    choice.txFfe[slots[i]]=grid(r[i],out.indices.index[i]);
    magnitude+=std::llround(std::abs(choice.txFfe[slots[i]])*100.);
  }
  const auto cursor=100-magnitude;
  if(cursor<std::llround(final8023ck120F8().minimumCursor*100.)) {
    out.rejection=ComEqRejection::CursorBelowMinimum; return out;
  }
  choice.txFfe[3]=static_cast<double>(cursor)/100.;
  choice.ctleDcGainDb=grid(r[4],out.indices.index[4]);
  choice.ctleDcGain2Db=grid(r[5],out.indices.index[5]);
  const size_t maskIndex=static_cast<size_t>(out.indices.index[4]*n[5]+out.indices.index[5]);
  if(mask && !(*mask)[maskIndex]) {
    out.rejection=ComEqRejection::CtleExcluded; return out;
  }
  validateCom120F8Equalizer(choice);
  out.choice=choice; return out;
}
ComEqIndices comEqIndicesFor(const Com120F8EqualizerChoice& initial) {
  validateCom120F8Equalizer(initial);
  const auto r=ranges();
  ComEqIndices out;
  const std::array<double,6> v{initial.txFfe[0],initial.txFfe[1],
    initial.txFfe[2],initial.txFfe[4],initial.ctleDcGainDb,initial.ctleDcGain2Db};
  for(size_t i=0;i<v.size();++i)
    out.index[i]=static_cast<int>(std::llround((v[i]-r[i].minimum)/r[i].step));
  return out;
}
std::vector<ComEqPhase> comEqPhases(const ComEqPhaseContract& c,
    size_t budget,const std::atomic_bool* cancelled) {
  check(cancelled);
  (void)phaseFor(c,c.firstOffset); (void)phaseFor(c,c.lastOffset);
  const auto count=static_cast<uint64_t>(static_cast<int64_t>(c.lastOffset)-c.firstOffset+1);
  require(count<=budget,"012_40 caller phase budget exceeded");
  std::vector<ComEqPhase> out;
  require(count<=out.max_size(),"012_40 phase allocation exceeds limit");
  out.reserve(static_cast<size_t>(count));
  for(int64_t i=c.firstOffset;i<=c.lastOffset;++i) {
    check(cancelled); out.push_back(phaseFor(c,static_cast<int>(i)));
  }
  if(c.order==ComEqPhaseOrder::Middle)
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b) {
      const auto aa=std::abs(static_cast<int64_t>(a.offset));
      const auto bb=std::abs(static_cast<int64_t>(b.offset));
      return aa==bb ? a.offset<b.offset : aa<bb;
    });
  check(cancelled);
  return out;
}
std::array<double,6> comEqDfe(const std::array<double,13>& normalized,
    double delta,bool floating) {
  require(delta==0. && !floating,"012_40 unsupported DFE configuration");
  return minimizeCom120F8IdealDfeResidual(normalized);
}
bool comEqImproves(double score,double previous) {
  require(std::isfinite(score) &&
    (std::isfinite(previous) || previous==-std::numeric_limits<double>::infinity()),
    "012_40 invalid FOM comparison");
  return score>previous;
}
void validateComEqSource(const SourceIdentity& captured,const SourceIdentity& observed,
    const Provenance& p,std::string_view testCase,std::string_view version,
    std::string_view commit) {
  require(captured==observed && hexSha(captured.sha256) &&
    !captured.revision.empty() && !captured.mappingIdentity.empty() &&
    captured.workspaceGeneration && captured.mappingGeneration &&
    captured.sha256==p.sourceSha256 && captured.revision==p.revision &&
    captured.mappingIdentity==p.mappingIdentity &&
    captured.workspaceGeneration==p.workspaceGeneration &&
    captured.mappingGeneration==p.mappingGeneration,
    "012_40 stale or mismatched source");
  require(version==comEqReferenceVersion && commit==comEqReferenceCommit,
    "012_40 reference mismatch");
  require(testCase=="TEST1" || testCase=="TEST2","012_40 invalid case");
  const std::string token="|case="+std::string(testCase)+"|";
  const auto first=p.settingsIdentity.find("|case=");
  require(first!=std::string::npos &&
    p.settingsIdentity.compare(first,token.size(),token)==0 &&
    p.settingsIdentity.find("|case=",first+1)==std::string::npos &&
    p.settingsIdentity.find("|eq=")==std::string::npos,
    "012_40 missing or conflicting case/settings identity");
}
ComEqCandidateKey bindComEqCandidate(const SourceSnapshot& source, std::string_view victimId,
    const SourceIdentity& observed,const Provenance& p,std::string_view testCase,
    std::string_view version,std::string_view commit,size_t ordinal,
    const std::optional<ComEqCtleMask>& mask,const ComEqPhaseContract& c,
    const ComEqPhase& phase,const std::atomic_bool* cancelled) {
  check(cancelled);
  validateComEqSource(source.identity(),observed,p,testCase,version,commit);
  (void)source.channel(victimId);
  const auto candidate=comEqCandidateAt(ordinal,mask,cancelled);
  require(candidate.choice.has_value(),"012_40 rejected candidate");
  (void)phaseFor(c,c.firstOffset); (void)phaseFor(c,c.lastOffset);
  require(phaseFor(c,phase.offset)==phase,"012_40 invalid phase identity");
  check(cancelled);
  return {source.identity(),std::string(victimId),p.settingsIdentity,std::string(testCase),
    std::string(version),std::string(commit),candidate.indices,c,phase,mask};
}
} // namespace si::channel
