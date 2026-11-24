#include "si/channel_analysis/com_eq_search_qualification.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
using E=ComEqSearchEnd;
}
int main(int argc,char** argv){
 try {
  demand(argc==2,"scratch directory required");
  namespace fs=std::filesystem;
  const fs::path root=argv[1];fs::create_directories(root);
  const auto file=root/"eq.s4p";
  {std::ofstream out(file);out<<"# GHz S RI R 50\n";
   for(int f=0;f<3;++f){out<<f;for(int s=0;s<4;++s)for(int r=0;r<4;++r)
    out<<" "<<((r==(s+2)%4)?.5:0.)<<" 0";out<<"\n";}
   demand(bool(out),"fixture write");}
  auto cache=Cache::open(file,root/"cache",nullptr,true);
  Channel ch;ch.id="victim";ch.name="SYNTHETIC";ch.nearP=0;ch.nearN=1;ch.farP=2;ch.farN=3;
  const auto source=SourceSnapshot::capture(cache,{ch},"012_41 synthetic",17,19);
  auto live=source.identity();
  Provenance p;p.sourceSha256=live.sha256;p.revision=live.revision;p.mappingIdentity=live.mappingIdentity;
  p.workspaceGeneration=live.workspaceGeneration;p.mappingGeneration=live.mappingGeneration;
  p.settingsIdentity="bounded|case=TEST1|";
  ComEqSearchSpec spec;
  spec.ordinals={0,1,2,3,4,5};
  spec.phaseContract={ComEqAnchor::Peak,ComEqPhaseOrder::FullSweep,0,0,5,10};
  ComEqCtleMask mask{};mask[0]=true;spec.mask=mask;
  spec.initial={0,0};spec.evaluationBudget=256;spec.iterationBudget=32;
  spec.objectiveId="SYNTHETIC:graph";
  std::array<double,6> values{0,2,1,4,5,3};
  ComEqSyntheticEvaluator eval=[&](const ComEqCandidateKey& k){
    return ComEqSyntheticScore{k,spec.objectiveId,values.at(static_cast<size_t>(k.indices.index[3]))};
  };
  std::atomic_bool stop{false};
  auto run=[&](std::string_view which="TEST1",std::string_view victim="victim",
      std::string_view version=comEqReferenceVersion,std::string_view commit=comEqReferenceCommit){
    return qualifyComEqSearch(source,victim,p,which,version,commit,spec,[&]{return live;},eval,&stop);
  };
  auto fail=[&](E e){auto r=run();demand(r.end==e&&!r.best&&!r.initial,"fail closed");};
  auto r=run();demand(r.end==E::SearchSpaceExhausted&&r.best&&r.initial&&
    r.best->position.ordinal==4&&r.best->score.value==5&&r.attempted==6&&r.iterations==0,"exhaustive oracle");
  demand(r.evaluationOrder==std::vector<ComEqSearchPosition>{{0,0},{1,0},{2,0},{3,0},{4,0},{5,0}},"exhaustive order");
  spec.mode=ComEqSearchMode::Local;
  r=run();demand(r.end==E::NoFurtherImprovement&&r.best&&r.best->position.ordinal==1&&
    r.best->score.value==2&&r.iterations==2&&r.attempted==3,"local nonglobal oracle");
  demand(r.evaluationOrder==std::vector<ComEqSearchPosition>{{0,0},{1,0},{2,0}},"local trace");
  spec.initial={3,0};r=run();demand(r.best&&r.best->position.ordinal==4&&r.iterations==2&&
    r.evaluationOrder==std::vector<ComEqSearchPosition>{{3,0},{2,0},{4,0},{5,0}},"second local oracle");
  spec.initial={0,0};values={1,1,2,3,4,5};r=run();
  demand(r.best&&r.best->position.ordinal==0&&r.iterations==1&&r.attempted==2,"plateau");
  spec.mode=ComEqSearchMode::Exhaustive;values={3,3,1,0,0,0};r=run();
  demand(r.best&&r.best->position.ordinal==0,"strict first tie");
  values[1]=std::nextafter(3.,4.);r=run();demand(r.best&&r.best->position.ordinal==1,"one ULP improvement");
  spec.ordinals={0,270,1620};r=run();
  demand(r.best&&r.best->position.ordinal==0&&r.rejected==2&&r.attempted==1,"cursor and mask rejection");
  spec.ordinals={270,1620};fail(E::NoValidCandidate);
  spec.ordinals={0,1,2,3,4,5};values={0,2,1,4,5,3};
  spec.evaluationBudget=2;fail(E::EvaluationBudgetExceeded);spec.evaluationBudget=256;
  spec.mode=ComEqSearchMode::Local;spec.iterationBudget=1;fail(E::IterationBudgetExceeded);
  spec.iterationBudget=32;spec.mode=ComEqSearchMode::Exhaustive;
  values[0]=std::numeric_limits<double>::quiet_NaN();fail(E::NumericalFailure);
  values[0]=std::numeric_limits<double>::infinity();fail(E::NumericalFailure);values[0]=0;
  stop=true;fail(E::Cancelled);stop=false;
  ++live.mappingGeneration;fail(E::StaleSource);live=source.identity();
  const auto original=eval;
  eval=[&](const ComEqCandidateKey& k){++live.workspaceGeneration;return original(k);};
  fail(E::StaleSource);live=source.identity();
  eval=[&](const ComEqCandidateKey& k){stop=true;return original(k);};
  fail(E::Cancelled);stop=false;
  eval=[&](const ComEqCandidateKey& k){auto v=original(k);v.key.victimId="other";return v;};
  fail(E::InvalidInput);
  eval=[&](const ComEqCandidateKey& k){auto v=original(k);v.objectiveId="other";return v;};
  fail(E::InvalidInput);
  eval=[](const ComEqCandidateKey&)->ComEqSyntheticScore{throw std::runtime_error("synthetic failure");};
  fail(E::NumericalFailure);eval=original;
  demand(run("TEST2").end==E::InvalidInput&&run("TEST1","wrong").end==E::InvalidInput&&
    run("TEST1","victim","wrong").end==E::InvalidInput&&
    run("TEST1","victim",comEqReferenceVersion,"wrong").end==E::InvalidInput,"binding failures");
  const auto test1=run();p.settingsIdentity="bounded|case=TEST2|";
  const auto test2=run("TEST2");demand(test1.best&&test2.best&&
    !(test1.best->score.key==test2.best->score.key),"case separation");
  p.settingsIdentity="bounded|case=TEST1|";
  spec.ordinals={0,0};fail(E::InvalidInput);spec.ordinals={1,0};fail(E::InvalidInput);
  spec.ordinals={0};spec.phaseContract={ComEqAnchor::Peak,ComEqPhaseOrder::Middle,-2,3,5,10};
  r=run();demand(r.best&&r.best->position.offset==0&&r.evaluationOrder==
    std::vector<ComEqSearchPosition>{{0,0},{0,-1},{0,1},{0,-2},{0,2},{0,3}},"phase order");
  spec.phaseContract.lastOffset=300;spec.phaseContract.sampleCount=400;fail(E::InvalidInput);
  std::cout<<"012_41 bounded synthetic search qualification PASS; NOT_IEEE_COM\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
