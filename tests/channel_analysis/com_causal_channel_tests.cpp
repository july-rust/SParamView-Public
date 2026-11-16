#include "si/channel_analysis/com_causal_channel.hpp"
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
using namespace si;
using namespace si::channel;
namespace {
void require(bool ok,const char* message) {
  if(!ok)throw std::runtime_error(message);
}
void close(Complex actual,Complex expected,double tolerance,const char* msg) {
  if(!std::isfinite(actual.real()) ||
     !std::isfinite(actual.imag()) ||
     std::abs(actual-expected)>tolerance)
    throw std::runtime_error(msg);
}
template<class F>void reject(F fn,const char* msg) {
  bool caught=false;
  try{fn();}catch(const Error&){caught=true;}
  require(caught,msg);
}
ComPhysicalFourPort wires(Complex gain) {
  ComPhysicalFourPort s;
  s.referenceOhm={50,50,50,50};
  for(int n=0;n<2;++n) {
    s.scattering[4*(n+2)+n]=gain;
    s.scattering[4*n+(n+2)]=gain;
  }
  return s;
}
ComCausalPhysicalInput example() {
  ComCausalPhysicalInput input;
  input.frequencyHz={0,1e9,10e9,26.5625e9};
  for(double f:input.frequencyHz) {
    Complex gain=0.8*std::exp(Complex{0,
       -2*std::numbers::pi*f*12e-12});
    input.boardS4p.push_back(wires(gain));
  }
  return input;
}
void testActualS4pFromSource(const std::filesystem::path &root) {
  namespace fs=std::filesystem;
  fs::create_directories(root);
  auto file=root/"causal-8023ck-synthetic.s4p";
  {
    std::ofstream output(file);
    require(bool(output),"cannot write synthetic .s4p");
    output<<"# GHz S RI R 50\n";
    for(double fGHz:{0.,1.,10.}) {
      output<<fGHz;
      for(int stimulus=0;stimulus<4;++stimulus)
        for(int response=0;response<4;++response) {
          const double thru=
            ((stimulus==0&&response==2) ||
             (stimulus==2&&response==0) ||
             (stimulus==1&&response==3) ||
             (stimulus==3&&response==1)) ? 0.8:0.;
          output<<" "<<thru<<" 0";
        }
      output<<"\n";
    }
  }
  auto cache=Cache::open(file,root/"cache",nullptr,true);
  Channel channel;
  channel.id="victim";channel.name="VICTIM";
  channel.nearP=0;channel.nearN=1;
  channel.farP=2;channel.farN=3;
  const auto source=SourceSnapshot::capture(
      cache,{channel},"causal-s4p-revision",17,19);
  const auto out=calculateHistoricalCausalC2cFromSource(source,"victim");
  require(out.usable() && out.status==Status::Warning,
          "verified physical Touchstone causal result must warn");
  require(out.provenance.sourceSha256==cache->meta().sha256 &&
          out.provenance.workspaceGeneration==17 &&
          out.provenance.mappingGeneration==19,
          "real source hash, version and mapping pinned");
  require(out.value->frequencyHz.size()==3,
          "full real Touchstone frequency count");
  require(out.diagnostics.size()==1 &&
          out.diagnostics[0].code=="NOT_IEEE_COM",
          "real causal channel never claims normative COM");
  for(auto& testCase:{out.value->test1,out.value->test2}) {
    close(testCase.loadedSdd.front().s21,{0.8,0},5e-13,
          "zero-frequency physical S4P and causal packages");
    require(testCase.loadedSdd.size()==3,
            "full S4P loaded causal response");
    require(std::abs(testCase.loadedSdd[2].s21)<1.,
            "passive causal frequency response");
  }
  reject([&]{(void)calculateHistoricalCausalC2cFromSource(
      source,"unconfirmed");},"unconfirmed port mapping blocked");
  Control stop;
  stop.cancelled.store(true);
  bool cancelled=false;
  try{(void)calculateHistoricalCausalC2cFromSource(
      source,"victim",0,&stop);}
  catch(const Cancelled&){cancelled=true;}
  require(cancelled,"cancelable source-based causal COM");
}
}
int main(int argc,char** argv) {
  try {
    require(argc==2,"expected CTest sandbox directory");
    auto input=example();
    const auto output=calculateHistoricalCausalC2cCases(input);
    require(output.usable()&&output.status==Status::Warning,
            "historical C2C causal model only warning");
    require(std::string(output.value->model).find("EXPLORATORY")!=
             std::string::npos,
            "historical model identity may not imply qualification");
    require(output.diagnostics[0].code=="NOT_IEEE_COM",
            "IEEE COM dB remains unavailable");
    require(output.value->test1.txPrimaryLineMm==13 &&
            output.value->test1.rxPrimaryLineMm==11 &&
            output.value->test2.txPrimaryLineMm==31 &&
            output.value->test2.rxPrimaryLineMm==29,
            "distinct Test1/Test2 package length selector");
    require(output.value->test1.loadedSdd.size()==input.frequencyHz.size() &&
            output.value->test2.loadedSdd.size()==input.frequencyHz.size(),
            "every board frequency evaluated in both cases");
    for(size_t i=0;i<input.frequencyHz.size();++i) {
      const double f=input.frequencyHz[i];
      const auto board=extractDifferentialComNetwork(input.boardS4p[i]);
      for(auto testcase:{ComPackageCase::Test1,ComPackageCase::Test2}) {
        auto l=com8023ckC2cPackageLengths(testcase);
        const auto tx=calculateComCausalPackage(
          historical8023ckC2cCausalCandidate(l.txMm),f,
          ComCausalPackageSide::TxDieToBoard);
        const auto rx=calculateComCausalPackage(
          historical8023ckC2cCausalCandidate(l.rxMm),f,
          ComCausalPackageSide::RxBoardToDie);
        const auto oracle=cascadeComScattering(
          cascadeComScattering(tx,board.differential),rx);
        const auto& actual=(testcase==ComPackageCase::Test1?
          output.value->test1:output.value->test2).loadedSdd[i];
        close(actual.s21,oracle.s.s21,2e-12,
              "composed S4P->causal Tx/board/Rx forward transfer");
        close(actual.s12,oracle.s.s12,2e-12,
              "composed S4P->causal Tx/board/Rx reverse transfer");
        close(actual.s11,oracle.s.s11,2e-12,
              "composed S4P->causal Tx/board/Rx near reflection");
        close(actual.s22,oracle.s.s22,2e-12,
              "composed S4P->causal Tx/board/Rx far reflection");
      }
    }
    close(output.value->test1.loadedSdd[0].s21,{0.8,0},2e-13,
          "DC continuous causal model has no loss");
    auto bad=input;bad.boardS4p[1].referenceOhm={75,75,75,75};
    reject([&]{(void)calculateHistoricalCausalC2cCases(bad);},
           "historical 50 ohm ref cannot silently change");
    bad=input;bad.boardS4p.pop_back();
    reject([&]{(void)calculateHistoricalCausalC2cCases(bad);},
           "missing measured physical frequency sample");
    bad=input;bad.boardS4p[1]=wires({1.2,0});
    reject([&]{(void)calculateHistoricalCausalC2cCases(bad);},
           "active board must fail closed");
    bad=input;bad.frequencyHz[0]=1.0;
    reject([&]{(void)calculateHistoricalCausalC2cCases(bad);},
           "DC omission fail closed");
    bad=input;bad.maxAllowedModeConversion=-1;
    reject([&]{(void)calculateHistoricalCausalC2cCases(bad);},
           "invalid mode-conversion bounds fail closed");
    std::atomic_bool stop{true};
    bool cancelled=false;
    try{(void)calculateHistoricalCausalC2cCases(input,{},&stop);}
    catch(const Cancelled&){cancelled=true;}
    require(cancelled,"causal COM model cancellation");
    testActualS4pFromSource(std::filesystem::path(argv[1]));
    std::cout<<"012_09 actual Touchstone S4P -> causal Test1/Test2 "
                "package math PASS; COM margin NOT_EVALUATED\n";
    return 0;
  }catch(const std::exception& e) {
    std::cerr<<"012_09 causal channel FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
