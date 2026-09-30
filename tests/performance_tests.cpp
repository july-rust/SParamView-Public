#include "si/core.hpp"
#include <barrier>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
static void expect(bool v,const char *m) { if(!v) throw si::Error(m); }
static void loadedTraceIdentity() {
  auto dir=si::fs::temp_directory_path()/"si_loaded_trace_identity";
  si::fs::create_directories(dir);
  auto path=dir/"asymmetric.s2p";
  auto write=[&](double nearReflection) {
    std::ofstream out(path);out<<"# Hz S RI R 50\n";
    // Two-port Touchstone order: S11 S21 S12 S22.
    for(int i=0;i<=32;++i) out<<i*1e8<<' '<<nearReflection<<" 0 .6 0 .7 0 .2 0\n";
  };
  write(.1);auto cache=si::Cache::open(path,dir/"cache",nullptr,true);
  si::Channel ch;ch.id=ch.name="SE";ch.nearP=0;ch.farP=1;
  si::Settings settings;
  auto original=si::loadTrace(*cache,ch,si::Metric::TDR,settings);
  auto again=si::loadTrace(*cache,ch,si::Metric::TDR,settings);
  expect(original.x==again.x && original.s==again.s,"Loaded trace roundtrip differs");
  settings.reverse=true;auto reverse=si::loadTrace(*cache,ch,si::Metric::TDR,settings);
  expect(std::abs(reverse.s.front()-.2)<1e-12,"Loaded trace cache ignored direction");
  settings.reverse=false;std::swap(ch.nearP,ch.farP);
  auto mapped=si::loadTrace(*cache,ch,si::Metric::TDR,settings);
  expect(mapped.s==reverse.s,"Loaded trace cache ignored physical mapping");
  std::swap(ch.nearP,ch.farP);
  auto open=si::loadTrace(*cache,ch,si::Metric::TDR,settings,nullptr,nullptr,si::Termination::Open);
  expect(std::abs(open.s.front()-.625)<1e-12,"Loaded trace cache ignored termination");
  si::Control cancelled;cancelled.cancelled=true;bool caught=false;
  try {si::loadTrace(*cache,ch,si::Metric::TDR,settings,nullptr,&cancelled);}catch(const si::Cancelled &){caught=true;}
  expect(caught,"Loaded trace cache hit ignored cancellation");
  write(.3);auto changed=si::Cache::open(path,dir/"cache",nullptr,true);
  expect(changed->meta().sha256!=cache->meta().sha256,"Source hash not refreshed");
  auto updated=si::loadTrace(*changed,ch,si::Metric::TDR,settings);
  expect(std::abs(updated.s.front()-.3)<1e-12,"Loaded trace cache reused overwritten source");
}
int main() {
 try {
  loadedTraceIdentity();
  si::Trace t; t.parameter="S11";
  for (int i=0;i<=8192;++i) { double f=i*20e9/8192; t.x.push_back(f); t.s.push_back(std::polar(.15,-2*3.14159265358979323846*f*1.2e-9)); }
  si::Settings s; s.tdrStopSeconds=180e-9;
  si::clearTdrCache(); auto a=si::transformTdr(t,s); auto first=si::tdrCacheStats();
  auto b=si::transformTdr(t,s);
  expect(a.time==b.time && a.impedance==b.impedance && a.reflection==b.reflection,"Cache roundtrip differs");
  expect(first.misses==1 && si::tdrCacheStats().hits==1,"Cache not reused");
  s.targetOhm=75; s.tdrReflection=true; s.tolerancePercent=2; s.markers=5;
  auto c=si::transformTdr(t,s); expect(c.reflection==a.reflection,"Display setting changed physics");
  expect(si::tdrCacheStats().misses==1,"Display settings should reuse waveform");
  t.s[128] *= .5; auto d=si::transformTdr(t,s);
  expect(d.reflection!=a.reflection && si::tdrCacheStats().misses==2,"Changed data reused stale waveform");
  s.kaiserBeta=4; si::transformTdr(t,s);
  s.tdrStartSeconds=1e-9; si::transformTdr(t,s);
  s.tdrStopSeconds=10e-9; si::transformTdr(t,s);
  t.referenceOhm=75; si::transformTdr(t,s);
  t.termination=si::Termination::Open; si::transformTdr(t,s);
  expect(si::tdrCacheStats().misses==7,"Physical settings omitted from key");
  si::Control cancelled; cancelled.cancelled=true; bool caught=false;
  try { si::transformTdr(t,s,&cancelled); } catch(const si::Cancelled &) {caught=true;}
  expect(caught,"Cache hit ignored cancellation");
  si::clearTdrCache(); std::barrier gate(8); std::vector<std::jthread> threads;
  for(int i=0;i<8;++i) threads.emplace_back([&]{gate.arrive_and_wait();si::transformTdr(t,s);});
  threads.clear(); auto stats=si::tdrCacheStats();
  expect(stats.misses==1 && stats.hits==7,"Identical concurrent inputs recomputed");
  expect(stats.bytes<128*1024*1024 && stats.entries==1,"Cache budget/statistics invalid");
  std::cout<<"PASS loaded trace identity/source/mapping/direction/load, waveform identity, physical setting invalidation, cancellation, single-flight, budget\n";
 } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
