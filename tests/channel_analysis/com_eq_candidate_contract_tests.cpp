#include "si/channel_analysis/com_eq_candidate_contract.hpp"
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
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const si::Error&){bad=true;}demand(bad,"expected rejection");}
template<class F> void cancels(F f){bool bad=false;try{f();}catch(const si::Cancelled&){bad=true;}demand(bad,"expected cancellation");}
}
int main(int argc,char** argv){
 try {
  demand(argc==2,"scratch directory required");
  // Independent integer oracle: nested signed coefficient hundredths,
  // not mixed-radix decode or existing C++ profile enumeration.
  size_t raw=0,valid=0,excluded=0;
  for(int g=-20;g<=0;++g)for(int h=-4;h<=0;++h)
  for(int a=-4;a<=0;a+=2)for(int b=0;b<=10;b+=2)
  for(int c=-28;c<=0;c+=2)for(int d=-10;d<=0;d+=2){
    auto x=comEqCandidateAt(raw++);
    int cur=100-std::abs(a)-std::abs(b)-std::abs(c)-std::abs(d);
    demand(x.indices.index==std::array<int,6>{(a+4)/2,b/2,(c+28)/2,(d+10)/2,g+20,h+4},"candidate order/indices");
    demand(!x.ctleMaskSupplied,"unknown mask must stay explicit");
    if(cur<54){++excluded;demand(!x.choice && x.rejection==ComEqRejection::CursorBelowMinimum,"cursor rejection");continue;}
    ++valid;
    demand(x.choice && x.rejection==ComEqRejection::None,"valid candidate");
    demand(x.choice->txFfe==std::array<double,5>{a/100.,b/100.,c/100.,cur/100.,d/100.},"candidate coefficients");
    demand(x.choice->ctleDcGainDb==g && x.choice->ctleDcGain2Db==h,"CTLE coefficients");
    demand(comEqIndicesFor(*x.choice)==x.indices,"round trip");
  }
  demand(raw==170100 && raw==comEqRawCandidateCount() && valid==168525 && excluded==1575,"frozen oracle counts");
  demand(comEqCandidateAt(0).choice->txFfe[3]==.58,"first candidate");
  demand(comEqCandidateAt(270).rejection==ComEqRejection::CursorBelowMinimum,"first rejected ordinal");
  rejects([&]{comEqCandidateAt(raw);});
  ComEqCtleMask mask{};mask[0]=true;mask[104]=true;
  size_t masked=0;
  for(size_t i=0;i<raw;++i)if(comEqCandidateAt(i,mask).choice)++masked;
  demand(masked==3210 && comEqCandidateAt(0,mask).ctleMaskSupplied,"explicit sparse mask");
  auto bad=*comEqCandidateAt(0).choice;bad.txFfe[0]=-.03;
  rejects([&]{comEqIndicesFor(bad);});
  bad=*comEqCandidateAt(0).choice;bad.ctleDcGainDb=-19.5;
  rejects([&]{comEqIndicesFor(bad);});
  bad=*comEqCandidateAt(0).choice;bad.txFfe[3]=.59;
  rejects([&]{comEqIndicesFor(bad);});
  ComEqPhaseContract pc{ComEqAnchor::MuellerMuller,ComEqPhaseOrder::FullSweep,-2,3,5,10};
  const auto phases=comEqPhases(pc,6);
  demand(phases.size()==6,"phase count");
  for(size_t i=0;i<6;++i){
    demand(phases[i].offset==static_cast<int>(i)-2 && phases[i].zeroBased==i+2,"one/zero based index");
    // Rational oracle sample period 1/1700000000000 seconds.
    demand(std::abs(phases[i].secondsFromFirstSample-(i+2)/1700000000000.)<1e-26,"sample time");
  }
  auto middle=pc;middle.order=ComEqPhaseOrder::Middle;
  auto ordered=comEqPhases(middle,6);
  const std::array<int,6> expected{0,-1,1,-2,2,3};
  for(size_t i=0;i<6;++i)demand(ordered[i].offset==expected[i],"middle order");
  rejects([&]{comEqPhases(pc,5);});
  auto wrong=pc;wrong.firstOffset=-5;rejects([&]{comEqPhases(wrong,100);});
  wrong=pc;wrong.lastOffset=6;rejects([&]{comEqPhases(wrong,100);});
  wrong=pc;wrong.anchorOneBased=0;rejects([&]{comEqPhases(wrong,100);});
  wrong=pc;wrong.anchorMode=static_cast<ComEqAnchor>(99);rejects([&]{comEqPhases(wrong,100);});
  std::array<double,13> norm{0.,0.,0.,1.,.2,.3,-.1,.1,.04,.2,0.,0.,0.};
  demand(comEqDfe(norm,0.,false)==std::array<double,6>{.3,.15,-.04,.1,.04,.1},"DFE oracle limits");
  rejects([&]{comEqDfe(norm,.01,false);});rejects([&]{comEqDfe(norm,0.,true);});
  auto invalid=norm;invalid[3]=0.;rejects([&]{comEqDfe(invalid,0.,false);});
  invalid=norm;invalid[4]=std::numeric_limits<double>::quiet_NaN();rejects([&]{comEqDfe(invalid,0.,false);});
  demand(comEqImproves(1.,-std::numeric_limits<double>::infinity()),"initial best");
  demand(!comEqImproves(1.,1.) && !comEqImproves(.9,1.) && comEqImproves(std::nextafter(1.,2.),1.),"strict tie");
  rejects([&]{comEqImproves(std::numeric_limits<double>::infinity(),1.);});
  std::atomic_bool stop{true};cancels([&]{comEqCandidateAt(0,{},&stop);});cancels([&]{comEqPhases(pc,6,&stop);});
  namespace fs=std::filesystem;
  const fs::path root=argv[1];fs::create_directories(root);
  const auto file=root/"eq.s4p";
  {std::ofstream out(file);out<<"# GHz S RI R 50\n";
   for(int f=0;f<3;++f){out<<f;for(int s=0;s<4;++s)for(int r=0;r<4;++r)out<<" "<<((r==(s+2)%4)?.5:0.)<<" 0";out<<"\n";}
   demand(bool(out),"fixture write");}
  auto cache=Cache::open(file,root/"cache",nullptr,true);
  Channel ch;ch.id="victim";ch.name="SYNTHETIC";ch.nearP=0;ch.nearN=1;ch.farP=2;ch.farN=3;
  const auto source=SourceSnapshot::capture(cache,{ch},"012_40 synthetic",17,19);
  const auto live=source.identity();
  Provenance p;p.sourceSha256=live.sha256;p.revision=live.revision;p.mappingIdentity=live.mappingIdentity;
  p.workspaceGeneration=live.workspaceGeneration;p.mappingGeneration=live.mappingGeneration;p.settingsIdentity="bounded|case=TEST1|";
  auto bind=[&](const SourceIdentity& id,const Provenance& prov,std::string_view which,std::string_view ver,std::string_view commit,size_t ordinal,const ComEqPhase& phase,std::string_view victim="victim"){
    return bindComEqCandidate(source,victim,id,prov,which,ver,commit,ordinal,mask,pc,phase);
  };
  const auto key=bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]);
  demand(key==bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]),"stable key");
  demand(!(key==bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,1,phases[0])),"EQ key");
  demand(!(key==bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[1])),"phase key");
  auto p2=p;p2.settingsIdentity="bounded|case=TEST2|";
  demand(!(key==bind(live,p2,"TEST2",comEqReferenceVersion,comEqReferenceCommit,0,phases[0])),"case key");
  auto stale=live;++stale.mappingGeneration;
  rejects([&]{bind(stale,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]);});
  rejects([&]{bind(live,p,"TEST2",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]);});
  rejects([&]{bind(live,p,"TEST1","4.18.0",comEqReferenceCommit,0,phases[0]);});
  rejects([&]{bind(live,p,"TEST1",comEqReferenceVersion,"bad",0,phases[0]);});
  rejects([&]{bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,270,phases[0]);});
  rejects([&]{bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,1620,phases[0]);});
  rejects([&]{bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0],"unknown");});
  auto forged=phases[0];++forged.zeroBased;
  rejects([&]{bind(live,p,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,forged);});
  p2=p;p2.settingsIdentity+="|case=TEST2|";rejects([&]{bind(live,p2,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]);});
  p2=p;p2.sourceSha256="";rejects([&]{bind(live,p2,"TEST1",comEqReferenceVersion,comEqReferenceCommit,0,phases[0]);});
  std::cout<<"012_40 bounded EQ contract PASS: 170100 raw, 168525 cursor-valid, 3210 mask-valid; NOT_IEEE_COM\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
