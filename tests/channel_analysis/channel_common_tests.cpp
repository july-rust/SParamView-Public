#include "si/channel_analysis/common.hpp"
#include "si/channel_analysis/ild.hpp"
#include "si/channel_analysis/icn.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>

using namespace si;
using namespace si::channel;
namespace {
void expect(bool pass, const std::string &message) {
  if (!pass) throw Error("012_03 FAILED: " + message);
}
template<class F> void reject(F &&fn, const std::string &message) {
  bool failed = false;
  try { fn(); } catch (const Error &) { failed = true; }
  expect(failed, message);
}
bool close(double a, double b, double eps = 1e-10) {
  return std::abs(a-b) <= eps*std::max({1.0, std::abs(a), std::abs(b)});
}
ComplexTrace synthetic() {
  return {{0, 1e9, 2.5e9, 4e9},
          {{1,0}, {.8,.2}, {.5,-.3}, {.2,0}},
          50, 50, "S21", {}};
}
void gridTests() {
  auto data = synthetic();
  auto q = validateFrequency(data.frequencyHz, true);
  expect(!q.uniform && q.hasDc, "nonuniform grid");
  auto ui = resampleUniform(data, 5);
  expect(ui.frequencyHz.size() == 5 && close(ui.frequencyHz[2], 2e9),
         "explicit uniform resample");
  expect(std::abs(ui.transfer[2].imag()) > .05,
         "complex interpolation preserves imaginary component");
  auto z = interpolateComplex(data, 1.75e9);
  expect(close(z.real(), .65) && close(z.imag(), -.05),
         "piecewise complex interpolation");
  reject([&] { interpolateComplex(data, 4.1e9); }, "reject outside source band");
  reject([&] { resample(data, {0,5e9}); }, "reject out-of-band resampling");
  reject([&] { resampleUniform(data, 4); }, "reject invalid FFT length");
  reject([&] { validateFrequency({0,1,1}); }, "duplicate frequency");
  reject([&] { validateFrequency({0,2,1}); }, "decreasing frequency");
  reject([&] { validateFrequency({0,std::numeric_limits<double>::infinity()}); },
         "nonfinite frequency");
  reject([&] { validateFrequency({1,2,3}, true); }, "no guessed DC");
  data.transfer.pop_back();
  reject([&] { validateTrace(data); }, "trace length mismatch");
  data = synthetic();
  data.transfer[1] = {std::numeric_limits<double>::quiet_NaN(),0};
  reject([&] { validateTrace(data); }, "nonfinite complex sample");
  data = synthetic();
  data.stimulusOhm = 0;
  reject([&] { validateTrace(data); }, "zero reference impedance");
  Control cancelled;
  cancelled.cancelled = true;
  reject([&] { resampleUniform(synthetic(), 5, &cancelled); },
         "cancelled resampling");
}
void transformTests() {
  std::vector<Complex> waveform{{1,0}, {2,1}, {-1,.5}, {3,-1},
                                {0,0}, {1,4}, {2,-3}, {-2,0}};
  auto original = waveform;
  fft(waveform, false);
  fft(waveform, true);
  for (size_t i=0;i<waveform.size();++i)
    expect(std::abs(waveform[i]-original[i]) < 1e-12,
           "complex FFT/IFFT round trip");
  ComplexTrace ideal{{0,1e9,2e9,3e9,4e9},
                     {{1,0},{1,0},{1,0},{1,0},{1,0}},
                     50,50,"S21",{}};
  auto unit = impulse(ideal);
  expect(unit.values.size() == 8 && close(unit.values[0], 1),
         "ideal flat channel unit discrete impulse");
  for (size_t i=1;i<8;++i)
    expect(std::abs(unit.values[i]) < 1e-12, "ideal channel zero tail");
  expect(close(unit.stepSeconds, 1.25e-10), "time sampling relationship");
  for (size_t k=0;k<ideal.transfer.size();++k) {
    const double phase = -2*std::numbers::pi*double(k)/8;
    ideal.transfer[k] = std::polar(1.0, phase);
  }
  auto delay = impulse(ideal);
  expect(close(delay.values[1],1), "one-sample group delay with phase");
  for (size_t i=0;i<8;++i)
    if (i!=1) expect(std::abs(delay.values[i]) < 1e-12,
                     "one-sample delay other bins");
  ideal.frequencyHz[0] = 1;
  reject([&] { impulse(ideal); }, "no time transform without DC");
  ideal.frequencyHz[0] = 0;
  ideal.frequencyHz[2] = 2.2e9;
  reject([&] { impulse(ideal); }, "explicit uniform sampling required");
  ideal.frequencyHz[2] = 2e9;
  ideal.transfer[0] = {1,.01};
  reject([&] { impulse(ideal); }, "DC Hermitian reality");
  Control cancelled;
  cancelled.cancelled = true;
  reject([&] { fft(waveform, false, &cancelled); }, "cancel FFT");
}
void snapshotTests(const fs::path &root) {
  fs::create_directories(root);
  const auto input = root/"channel.s2p";
  {
    std::ofstream out(input);
    out << "# GHz S RI R 50\n";
    for (int f=0;f<=4;++f)
      out << f << " 0.1 0 0.5 0 0.25 0 0.2 0\n";
  }
  auto src = Cache::open(input, root/"cache", nullptr, true);
  expect(src->meta().sha256.size() == 64, "source full SHA");
  Channel c;
  c.id = "victim"; c.name="VICTIM"; c.nearP=0; c.farP=1;
  const std::vector<Channel> mapping{c};
  auto a = SourceSnapshot::capture(src, mapping, "revision-1", 1, 1);
  expect(a.current(a.identity()), "identity current");
  auto changed = a.identity();
  ++changed.mappingGeneration;
  expect(!a.current(changed), "mapping generation invalidates");
  changed = a.identity();
  changed.sha256[0] = changed.sha256[0] == '0' ? '1' : '0';
  expect(!a.current(changed), "source digest invalidates");
  auto fwd = extract(a,"victim",Metric::IL,false);
  auto rev = extract(a,"victim",Metric::IL,true);
  expect(fwd.parameter == "S21" && rev.parameter == "S12",
         "direction labeling");
  expect(close(fwd.transfer[0].real(),.5) &&
         close(rev.transfer[0].real(),.25),
         "actual 2-port directional samples");
  IldSettings ild;ild.model=IldFitModel::Linear;ild.stopHz=4e9;
  for(bool reverse:{false,true}) {
    const auto result=calculateIld(extract(a,"victim",Metric::IL,reverse),ild);
    expect(result.usable() && close(result.value->insertionLossDb[0],
        -20*std::log10(reverse?.25:.5)) && result.value->rmsDb<1e-9,
        "S2P directional ILD oracle");
  }
  // More than four physical ports, nonadjacent selected path, distinct directions.
  const auto multi=root/"ild-multi.s6p";
  {
    std::ofstream out(multi);
    out<<"# GHz S RI R 50\n";
    // N>=3 Touchstone full matrix is row-major (response, stimulus).
    for(int k=0;k<=4;++k) {
      out<<k;
      for(int response=0;response<6;++response) {
        for(int stimulus=0;stimulus<6;++stimulus) {
          double v=.01;
          if(response==4&&stimulus==1)v=.5;
          if(response==1&&stimulus==4)v=.25;
          out<<" "<<v<<" 0";
        }
        out<<"\n";
      }
    }
  }
  auto multiCache=Cache::open(multi,root/"ild-multi-cache",nullptr,true);
  Channel lane;lane.id="selected";lane.name="Selected";lane.nearP=1;lane.farP=4;
  Channel other;other.id="other";other.name="Other";other.nearP=0;other.farP=3;
  auto ms=SourceSnapshot::capture(multiCache,{other,lane},"multi-ild",1,1);
  for(bool reverse:{false,true}) {
    auto result=calculateIld(extract(ms,"selected",Metric::IL,reverse),ild);
    expect(result.usable() && result.value->frequencyHz.size()==5,"S6P selected ILD result");
    const double expected=-20*std::log10(reverse?.25:.5);
    for(double loss:result.value->insertionLossDb)
      expect(close(loss,expected),"S6P nonadjacent mapped directional ILD loss");
    expect(result.value->rmsDb<1e-9,"S6P flat ILD");
  }
  // 012_45: asymmetric differential physical paths in a row-major S8P.
  // The four terms of each 2x2 block are (+h/2,-h/2,-h/2,+h/2).
  // Nonadjacent mapping deliberately prevents an adjacent-port shortcut.
  const auto icnFile=root/"icn-differential.s8p";
  {
    std::ofstream out(icnFile);
    out<<"# GHz S RI R 50\n";
    const int vp[2][2]={{0,4},{1,5}};
    const int ap[2][2]={{2,6},{3,7}};
    const double gains[2][2]={{.04,.16},{.08,.12}};
    for(int k=0;k<=4;++k) {
      out<<k;
      for(int r=0;r<8;++r) for(int s=0;s<8;++s) {
        double re=0,im=0;
        for(int v=0;v<2;++v) for(int a=0;a<2;++a)
          for(int rp=0;rp<2;++rp) for(int sp=0;sp<2;++sp)
            if(r==vp[v][rp] && s==ap[a][sp]) {
              re=gains[v][a]*(rp==sp?.5:-.5);
              im=re*.5;
            }
        out<<" "<<re<<" "<<im;
      }
      out<<"\n";
    }
  }
  auto icnCache=Cache::open(icnFile,root/"icn-differential-cache",nullptr,true);
  Channel v;v.id="V";v.name="Victim";v.nearP=0;v.nearN=4;v.farP=1;v.farN=5;
  Channel ag;ag.id="A";ag.name="Aggressor";ag.nearP=2;ag.nearN=6;ag.farP=3;ag.farN=7;
  auto icnSnap=SourceSnapshot::capture(icnCache,{v,ag},"icn-diff",1,1);
  IcnSettings cfg;cfg.stopHz=4e9;
  for(bool reverse:{false,true}) {
    auto n=extract(icnSnap,"V",Metric::NEXT,reverse,"A");
    auto f=extract(icnSnap,"V",Metric::FEXT,reverse,"A");
    const double ng=reverse?.12:.04,fg=reverse?.16:.08;
    for(const auto &z:n.transfer) expect(std::abs(z-Complex(ng,ng*.5))<1e-10,"S8P differential NEXT physical path");
    for(const auto &z:f.transfer) expect(std::abs(z-Complex(fg,fg*.5))<1e-10,"S8P differential FEXT physical path");
    expect(close(n.stimulusOhm,100) && close(n.responseOhm,100) &&
           close(f.stimulusOhm,100) && close(f.responseOhm,100),"S8P matched differential reference");
    auto icn=calculateIcn({{"A",IcnEnd::Next,n},{"A",IcnEnd::Fext,f}},cfg);
    expect(icn.usable(),"S8P differential ICN usable");
    double power=0,prev=0;
    for(int k=0;k<=4;++k) {
      const double hz=k*1e9,x=std::numbers::pi*hz/4e9;
      const double sinc=k?std::sin(x)/x:1;
      const double edge=2*std::numbers::pi*hz*30e-12/2.5631031310892007;
      const double density=.32/4e9*sinc*sinc*std::exp(-edge*edge)/(1+std::pow(hz/10e9,2));
      if(k)power+=(prev+density)*.5e9;
      prev=density;
    }
    expect(close(icn.value->nextRmsV,std::sqrt(power*1.25)*ng) &&
           close(icn.value->fextRmsV,std::sqrt(power*1.25)*fg) &&
           close(icn.value->totalRmsV,std::sqrt(power*1.25)*std::hypot(ng,fg)),
           "S8P differential ICN independent RMS");
  }
  expect(close(fwd.stimulusOhm,50) && close(fwd.responseOhm,50),
         "source reference");
  reject([&] { extract(a,"missing",Metric::IL); }, "unconfirmed port mapping");
  reject([&] { extract(a,"victim",Metric::NEXT); },
         "NEXT requires aggressor");
  reject([&] { extract(a,"victim",Metric::NEXT,false,"victim"); },
         "self aggressor forbidden");
  reject([&] { extract(a,"victim",Metric::TDR); },
         "no accidental legacy TDR conditioning");
  auto bad = mapping;
  bad[0].farP = 9;
  reject([&] { SourceSnapshot::capture(src,bad,"r",1,1); },
         "invalid physical port");
  reject([&] { SourceSnapshot::capture(src,mapping,"",1,1); },
         "unconfirmed revision");
  CacheRequest r;
  r.victimId="victim"; r.module="ILD"; r.algorithmVersion="1";
  r.settingsIdentity="settings-v1"; r.startHz=1e9; r.stopHz=4e9;
  auto base = cacheKey(a,r);
  expect(base == cacheKey(a,r), "deterministic key");
  r.reverse=true;
  expect(base != cacheKey(a,r), "direction in key");
  r.reverse=false; r.startHz=2e9;
  expect(base != cacheKey(a,r), "band in key");
  r.startHz=1e9; r.settingsIdentity="other";
  expect(base != cacheKey(a,r), "configuration in key");
  r.settingsIdentity.clear();
  reject([&] { cacheKey(a,r); }, "incomplete cache settings");
  {
    std::ofstream out(input, std::ios::app);
    out << "! changed after cached revision\n";
  }
  reject([&] { SourceSnapshot::capture(src,mapping,"revision-2",2,2); },
         "modified source SHA must fail closed");
  expect(close(fwd.transfer[0].real(),.5),
         "pinned source snapshot remains independent");
}
} // namespace
int main(int argc,char **argv) {
  try {
    if(argc!=2) throw Error("expected output directory argument");
    gridTests();
    transformTests();
    snapshotTests(fs::path(argv[1]));
    std::cout << "012_03 Channel Analysis Core tests PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
