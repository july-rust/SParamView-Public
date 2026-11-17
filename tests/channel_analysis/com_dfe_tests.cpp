#include "si/channel_analysis/com_dfe.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
void close(double a, double b, const char* why) {
  demand(std::isfinite(a) && std::isfinite(b) &&
         std::abs(a-b) <= 1e-12*std::max(1.,std::abs(b)), why);
}
template<class F> void reject(F&& f, const char* why) {
  bool rejected = false;
  try { f(); } catch(const si::Error&) { rejected = true; }
  demand(rejected, why);
}
Provenance identity(const std::string& which = "TEST1") {
  Provenance p;
  p.sourceSha256 = std::string(64,'a');
  p.revision = "synthetic-fixed-1";
  p.mappingIdentity = "confirmed-victim-package";
  p.settingsIdentity = "IEEE-120F8|case=" + which + "|ffe-ctle-rx";
  p.workspaceGeneration = 11;
  p.mappingGeneration = 14;
  return p;
}
ComDfeInput fixture(const Provenance& p,
                    const std::string& which = "TEST1") {
  ComDfeInput in;
  in.testCase = which;
  in.taps = {0.4,0.1,0.02,0,0,0};
  Com120F8RectangularPulseData wave;
  wave.symbolPeriodSeconds = 1./final8023ck120F8().symbolRateHz;
  for(int k=-3;k<=9;++k)
    wave.timeSeconds.push_back(5e-9+k*wave.symbolPeriodSeconds);
  // Standalone analytic fixture: 13 observed symbol coefficients.
  wave.unitSymbolPulse = {
    0.02,-0.03,0.01,1.0,0.4,0.1,0.02,0,0,0,0.04,0,0};
  wave.maximumAbsolutePulse=1.;
  in.symbolPulse.status=Status::Warning;
  in.symbolPulse.value=wave;
  in.symbolPulse.provenance=p;
  in.symbolPulse.provenance.algorithm =
    "com-120f8-rectangular-symbol-band-quadrature";
  in.symbolPulse.provenance.version = "012_09_03-pulse-1";
  return in;
}
} // namespace
int main() {
  try {
    const auto p = identity();
    const auto in = fixture(p);
    const auto r = calculateComDfe(in,p);
    demand(r.usable() && r.status==Status::Warning &&
           r.diagnostics.size()==1 &&
           r.diagnostics[0].code=="NOT_IEEE_COM",
           "No fabricated production COM result");
    demand(r.provenance.algorithm==
           "com-120f8-fixed-6tap-symbol-dfe-exploratory",
           "Explicit exploratory provenance");
    const auto& d=*r.value;
    demand(d.testCase=="TEST1", "Test1 identity retained");
    close(d.cursorAmplitudeUnitSymbol,1.,"Main cursor");
    close(d.symbolPeriodSeconds,1./53.125e9,"Symbol UI");
    for(size_t i=4;i<=9;++i)
      close(d.normalizedAfter[i],0.,"Correct six postcursors");
    close(d.normalizedAfter[10],0.04,"Leave +7 UI tail");
    close(d.postcursorRssBefore,std::sqrt(0.172),
          "Independent pre-DFE postcursor RSS");
    close(d.postcursorRssAfter,0.04,"Independent residual RSS");
    close(d.precursorRssBefore,std::sqrt(0.0014),
          "Independent precursor RSS");
    close(d.precursorRssAfter,d.precursorRssBefore,
          "DFE does not touch precursors");
    close(d.normalizedAfter[3],1.,"Main cursor unchanged");

    auto negative=fixture(p);
    for(double& v:negative.symbolPulse.value->unitSymbolPulse)v*=-2;
    negative.symbolPulse.value->maximumAbsolutePulse=2;
    const auto signedGain=calculateComDfe(negative,p);
    for(size_t i=0;i<13;++i) {
      close(signedGain.value->normalizedBefore[i],
            d.normalizedBefore[i],"Signed amplitude scale invariance");
      close(signedGain.value->normalizedAfter[i],
            d.normalizedAfter[i],"Signed DFE compensation invariance");
    }
    auto poor=fixture(p);
    poor.symbolPulse.value->unitSymbolPulse[4]=-0.5;
    const auto worse=calculateComDfe(poor,p);
    demand(worse.value->postcursorRssAfter >
           worse.value->postcursorRssBefore,
           "A legal but poor DFE choice can worsen residual ISI");

    const auto cfg=final8023ck120F8();
    auto boundary=fixture(p);
    boundary.taps=cfg.dfeMin;
    demand(calculateComDfe(boundary,p).usable(),
           "All DFE tap lower bounds accepted");
    boundary.taps=cfg.dfeMax;
    demand(calculateComDfe(boundary,p).usable(),
           "All DFE tap upper bounds accepted");
    boundary.taps[5]=0.100001;
    reject([&]{calculateComDfe(boundary,p);},"Upper bound exceeded");
    boundary=fixture(p);
    boundary.taps[0]=0.299999;
    reject([&]{calculateComDfe(boundary,p);},"Lower bound exceeded");
    boundary=fixture(p);
    boundary.taps[2]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{calculateComDfe(boundary,p);},"NaN tap rejected");

    auto bad=fixture(p);
    bad.testCase="TEST2";
    reject([&]{calculateComDfe(bad,p);},"Test1/2 swapping rejected");
    bad=fixture(p); bad.testCase="TEST3";
    reject([&]{calculateComDfe(bad,p);},"Invalid package rejected");
    bad=fixture(p); bad.symbolPulse.provenance.sourceSha256=std::string(64,'b');
    reject([&]{calculateComDfe(bad,p);},"Source SHA mismatch rejected");
    bad=fixture(p); bad.symbolPulse.provenance.mappingGeneration++;
    reject([&]{calculateComDfe(bad,p);},"Mapping generation mismatch");
    bad=fixture(p); bad.symbolPulse.provenance.settingsIdentity+="changed";
    reject([&]{calculateComDfe(bad,p);},"Settings mismatch");
    bad=fixture(p);
    bad.symbolPulse.provenance.algorithm="com-120f8-bandlimited-pulse-exploratory";
    reject([&]{calculateComDfe(bad,p);},"Impulse is not symbol pulse");
    bad=fixture(p); bad.symbolPulse.value->model="IMPULSE";
    reject([&]{calculateComDfe(bad,p);},"Wrong waveform model");
    bad=fixture(p);
    bad.symbolPulse.value->timeSeconds[9] +=
      0.5*bad.symbolPulse.value->symbolPeriodSeconds;
    reject([&]{calculateComDfe(bad,p);},"Off-UI sampled times");
    bad=fixture(p); bad.symbolPulse.value->unitSymbolPulse.pop_back();
    reject([&]{calculateComDfe(bad,p);},"Missing observed tail");
    bad=fixture(p); bad.symbolPulse.value->maximumAbsolutePulse=3;
    reject([&]{calculateComDfe(bad,p);},"Inconsistent waveform metadata");
    bad=fixture(p); bad.symbolPulse.value->unitSymbolPulse[3]=0;
    reject([&]{calculateComDfe(bad,p);},"Zero cursor");
    bad=fixture(p); bad.symbolPulse.value->unitSymbolPulse[3]=1e-11;
    reject([&]{calculateComDfe(bad,p);},"Ill-conditioned cursor");
    bad=fixture(p); bad.symbolPulse.status=Status::NotEvaluated;
    reject([&]{calculateComDfe(bad,p);},"Blocked upstream source");

    std::atomic_bool cancel{true};
    bool cancelled=false;
    try{(void)calculateComDfe(in,p,&cancel);}
    catch(const si::Cancelled&){cancelled=true;}
    demand(cancelled,"Cancellation propagates");

    const auto p2=identity("TEST2");
    auto second=fixture(p2,"TEST2");
    second.symbolPulse.value->unitSymbolPulse[4]=0.45;
    const auto other=calculateComDfe(second,p2);
    close(other.value->normalizedAfter[4],0.05,
          "Independent Test2 postcursor residual");
    reject([&]{calculateComDfe(second,p);},
           "Cross-package snapshot mismatch forbidden");

    // Real rectangular SYMBOL-pulse execution with complex Gaussian H(f).
    // Exercises actual 012_09_03 quadrature; no repeating full COM reference.
    const double ui=1./cfg.symbolRateHz;
    const double fc=6e9,delay=0.4e-9;
    std::vector<double> times;
    for(int k=-3;k<=9;++k)
      times.push_back(delay+(static_cast<double>(k)+0.5)*ui);
    ComPreparedData channel;
    for(int k=0;k<=4096;++k) {
      const double f=8e6*k;
      channel.frequencyHz.push_back(f);
      channel.conditionedThru.push_back(
        std::exp(-std::pow(f/fc,2.))*
        std::polar(0.8,-2.*std::numbers::pi*f*delay));
    }
    const auto realPulse=calculateCom120F8RectangularPulse(
        channel,times,p);
    demand(realPulse.usable(),"Existing symbol integrator usable");
    auto linked=fixture(p);
    linked.symbolPulse=realPulse;
    const auto actual=calculateComDfe(linked,p);
    demand(actual.usable(),"Actual symbol convolution feeds fixed DFE");
    for(size_t i=0;i<13;++i)
      close(actual.value->normalizedBefore[i],
            realPulse.value->unitSymbolPulse[i] /
                realPulse.value->unitSymbolPulse[3],
            "Actual generated symbol pulse preserved");

    std::cout<<"012_17 DFE: exact golden, real symbol, identity, "
                "bounds and cancel PASS\n";
  }catch(const std::exception& e) {
    std::cerr<<"012_17 failure: "<<e.what()<<'\n';
    return 1;
  }
  return 0;
}
