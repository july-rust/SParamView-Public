#include "si/channel_analysis/com_eq_search_qualification.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace si::channel {
namespace {
struct Stop { ComEqSearchEnd end; };
void need(bool ok, ComEqSearchEnd end) { if(!ok) throw Stop{end}; }
struct Node {
  ComEqSearchPosition position;
  ComEqCandidateKey key;
  std::optional<ComEqSyntheticScore> score;
};
bool adjacent(const Node& a,const Node& b) {
  int64_t distance=std::abs(static_cast<int64_t>(a.position.offset)-b.position.offset);
  for(size_t i=0;i<a.key.indices.index.size();++i)
    distance+=std::abs(a.key.indices.index[i]-b.key.indices.index[i]);
  return distance==1;
}
}
ComEqSearchQualification qualifyComEqSearch(const SourceSnapshot& source,
    std::string_view victimId,const Provenance& provenance,
    std::string_view testCase,std::string_view version,std::string_view commit,
    const ComEqSearchSpec& spec,const ComEqLiveIdentity& live,
    const ComEqSyntheticEvaluator& evaluate,const std::atomic_bool* cancelled) {
  ComEqSearchQualification out;
  auto check=[&] {
    need(!cancelled || !cancelled->load(std::memory_order_relaxed),ComEqSearchEnd::Cancelled);
    need(live()==source.identity(),ComEqSearchEnd::StaleSource);
    need(!cancelled || !cancelled->load(std::memory_order_relaxed),ComEqSearchEnd::Cancelled);
  };
  try {
    constexpr size_t limit=256;
    need(bool(live) && bool(evaluate) && spec.objectiveId.starts_with("SYNTHETIC:") &&
      spec.objectiveId.size()>10 && spec.objectiveId.size()<=128 &&
      !spec.ordinals.empty() && spec.ordinals.size()<=limit &&
      spec.evaluationBudget>0 && spec.evaluationBudget<=limit &&
      spec.iterationBudget>0 && spec.iterationBudget<=32 &&
      (spec.mode==ComEqSearchMode::Exhaustive || spec.mode==ComEqSearchMode::Local),
      ComEqSearchEnd::InvalidInput);
    need(std::is_sorted(spec.ordinals.begin(),spec.ordinals.end()) &&
      std::adjacent_find(spec.ordinals.begin(),spec.ordinals.end())==spec.ordinals.end() &&
      spec.ordinals.back()<comEqRawCandidateCount(),ComEqSearchEnd::InvalidInput);
    check();
    validateComEqSource(source.identity(),source.identity(),provenance,testCase,version,commit);
    (void)source.channel(victimId);
    const auto phases=comEqPhases(spec.phaseContract,limit/spec.ordinals.size(),cancelled);
    std::vector<Node> nodes;
    for(auto ordinal:spec.ordinals) {
      check();
      auto candidate=comEqCandidateAt(ordinal,spec.mask,cancelled);
      if(!candidate.choice){out.rejected+=phases.size();continue;}
      for(const auto& phase:phases) {
        check();
        nodes.push_back({{ordinal,phase.offset},bindComEqCandidate(source,victimId,
          source.identity(),provenance,testCase,version,commit,ordinal,spec.mask,
          spec.phaseContract,phase,cancelled),{}});
      }
    }
    need(!nodes.empty(),ComEqSearchEnd::NoValidCandidate);
    const auto it=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.position==spec.initial;});
    need(it!=nodes.end(),ComEqSearchEnd::InvalidInput);
    const size_t initial=static_cast<size_t>(it-nodes.begin());
    auto score=[&](size_t i) {
      check();
      auto& n=nodes[i];
      if(!n.score) {
        need(out.attempted<spec.evaluationBudget,ComEqSearchEnd::EvaluationBudgetExceeded);
        ++out.attempted;
        out.evaluationOrder.push_back(n.position);
        ComEqSyntheticScore result;
        try {result=evaluate(n.key);}
        catch(const si::Cancelled&){throw Stop{ComEqSearchEnd::Cancelled};}
        catch(const std::exception&){check();throw Stop{ComEqSearchEnd::NumericalFailure};}
        check();
        need(result.key==n.key && result.objectiveId==spec.objectiveId,
          ComEqSearchEnd::InvalidInput);
        need(std::isfinite(result.value),ComEqSearchEnd::NumericalFailure);
        n.score=std::move(result);
      }
      return n.score->value;
    };
    size_t best=initial;
    if(spec.mode==ComEqSearchMode::Exhaustive) {
      best=0;
      double bestValue=score(0);
      for(size_t i=1;i<nodes.size();++i) {
        const auto v=score(i);
        if(comEqImproves(v,bestValue)){best=i;bestValue=v;}
      }
      out.end=ComEqSearchEnd::SearchSpaceExhausted;
    } else {
      double bestValue=score(best);
      for(;;) {
        need(out.iterations<spec.iterationBudget,ComEqSearchEnd::IterationBudgetExceeded);
        ++out.iterations;
        size_t next=best;
        double nextValue=bestValue;
        for(size_t i=0;i<nodes.size();++i) {
          if(!adjacent(nodes[best],nodes[i]))continue;
          const double v=score(i);
          if(comEqImproves(v,nextValue)){next=i;nextValue=v;}
        }
        if(next==best){out.end=ComEqSearchEnd::NoFurtherImprovement;break;}
        best=next;bestValue=nextValue;
      }
    }
    check();
    out.initial=ComEqSearchVisit{nodes[initial].position,*nodes[initial].score};
    out.best=ComEqSearchVisit{nodes[best].position,*nodes[best].score};
  } catch(const Stop& s){out.end=s.end;out.initial.reset();out.best.reset();}
    catch(const si::Cancelled&){out.end=ComEqSearchEnd::Cancelled;out.initial.reset();out.best.reset();}
    catch(const si::Error&){out.end=ComEqSearchEnd::InvalidInput;out.initial.reset();out.best.reset();}
    catch(const std::exception&){out.end=ComEqSearchEnd::NumericalFailure;out.initial.reset();out.best.reset();}
  return out;
}
} // namespace si::channel
