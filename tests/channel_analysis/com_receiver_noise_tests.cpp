#include "si/channel_analysis/com_receiver_noise.hpp"
#include "si/channel_analysis/com_120f8.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace si;
using namespace si::channel;
namespace {
void require(bool good, const char* reason) {
  if (!good) throw std::runtime_error(reason);
}
void close(double got, double expected, double absTol,
           double relTol, const char* reason) {
  require(std::isfinite(got) && std::isfinite(expected) &&
          std::abs(got - expected) <=
              absTol + relTol*std::abs(expected), reason);
}
template<class F> void reject(F&& f, const char* reason) {
  bool caught = false;
  try { f(); } catch (const Error&) { caught = true; }
  require(caught, reason);
}
ComReceiverNoiseInput flat() {
  ComReceiverNoiseInput in;
  in.oneSidedPsdV2PerHz = 4e-8;
  in.oneSidedTailProbability = 1e-5;
  in.frequencyHz = {0, 5000, 10000};
  in.noiseTransfer = {{1,0},{1,0},{1,0}};
  in.queryVoltageV = {-0.02,0,0.02};
  return in;
}
} // namespace

int main() {
  try {
    auto in = flat();
    const auto result = calculateComReceiverNoise(in);
    require(result.usable() && result.status == Status::Warning,
            "Gaussian receiver output must remain exploratory");
    require(result.provenance.algorithm ==
              "com-gaussian-receiver-noise-exploratory",
            "Exploratory provenance");
    const auto& d = *result.value;
    close(d.varianceV2,4e-4,1e-15,1e-12,"Flat PSD variance");
    close(d.rmsV,0.02,1e-14,1e-12,"Flat PSD RMS");
    close(d.integrationStopHz,10000,0,0,"Finite bandwidth");
    require(d.hasContinuousDensity && d.pdfPerV.size()==3 &&
            d.cdf.size()==3, "Gaussian samples are present");
    close(d.pdfPerV[1],1.0/(0.02*std::sqrt(2.0*std::acos(-1.))),
          1e-12,1e-12,"Independent Gaussian PDF(0)");
    close(d.cdf[1],0.5,1e-15,0,"Gaussian CDF(0)");
    close(d.cdf[0]+d.cdf[2],1.0,1e-14,0,
          "Gaussian CDF symmetric about zero");
    close(d.oneSidedQuantileV/d.rmsV,4.264890793922825,
          1e-10,0,"Independent Gaussian Q^-1(1e-5)");
    close(d.verifiedTailProbability,1e-5,1e-11,1e-6,
          "Quantile reverse-tail evaluation");

    // Codex P2: exp(-800) underflows before scaling by 1e150, but the
    // final PDF is a representable ~1.4632702508383151e-198 1/V.
    // This independent fixed constant checks the entire log-domain density.
    auto tiny=flat();
    tiny.oneSidedPsdV2PerHz=1e-300;
    tiny.frequencyHz={0.,1.};
    tiny.noiseTransfer={{1.,0.},{1.,0.}};
    tiny.queryVoltageV={4e-149};
    const auto thinTail=calculateComReceiverNoise(tiny);
    require(thinTail.usable() && thinTail.value->hasContinuousDensity,
            "Small but finite Gaussian RMS is supported");
    close(thinTail.value->rmsV,1e-150,0,1e-12,
          "Small Gaussian RMS preserved");
    close(thinTail.value->pdfPerV[0],1.4632702508383151e-198,
          0,1e-10,"Representable Gaussian PDF far-tail not underflowed");

    in.oneSidedPsdV2PerHz*=4;
    const auto psd4 = calculateComReceiverNoise(in);
    close(psd4.value->varianceV2,4*d.varianceV2,1e-14,1e-12,
          "PSD*4 -> variance*4");
    close(psd4.value->rmsV,2*d.rmsV,1e-14,1e-12,
          "PSD*4 -> RMS*2");
    in=flat();
    for (auto& h:in.noiseTransfer) h*=2.;
    const auto gain2=calculateComReceiverNoise(in);
    close(gain2.value->varianceV2,4*d.varianceV2,1e-14,1e-12,
          "Gain*2 -> variance*4");

    // Independent analytic finite-band first-order lowpass:
    // integral_0^F S/(1+(f/fc)^2) df = S*fc*atan(F/fc).
    in=flat();
    in.frequencyHz.clear();
    in.noiseTransfer.clear();
    constexpr double fc=1000.;
    constexpr double stop=8192.;
    for (size_t k=0;k<=8192;++k) {
      const double f=static_cast<double>(k);
      in.frequencyHz.push_back(f);
      in.noiseTransfer.push_back(1.0/Complex{1.,f/fc});
    }
    const auto lowpass=calculateComReceiverNoise(in);
    const double analytic=in.oneSidedPsdV2PerHz*fc*std::atan(stop/fc);
    close(lowpass.value->varianceV2,analytic,1e-12,1e-6,
          "Independent atan lowpass variance oracle");

    // Existing final-profile RX filter can be a supplied component, but
    // these diagnostics do not certify the full IEEE 93A noise path.
    in=flat();
    const auto p=final8023ck120F8();
    in.oneSidedPsdV2PerHz =
        p.oneSidedNoiseSpectralDensityV2PerGHz/1e9;
    in.noiseTransfer.clear();
    for (double f:in.frequencyHz)
      in.noiseTransfer.push_back(com120F8ReceiverNoiseFilter(f));
    const auto existingFilter=calculateComReceiverNoise(in);
    require(existingFilter.usable() &&
            existingFilter.value->varianceV2>0 &&
            existingFilter.diagnostics[0].code=="NOT_IEEE_COM",
            "Existing 120F-8 noise filter remains a diagnostic");

    in=flat();
    in.oneSidedPsdV2PerHz=0;
    const auto zero=calculateComReceiverNoise(in);
    require(zero.usable() && !zero.value->hasContinuousDensity &&
            zero.value->varianceV2==0 &&
            std::isnan(zero.value->oneSidedQuantileV) &&
            zero.value->pdfPerV.empty() &&
            zero.value->cdf.empty() &&
            zero.diagnostics.size()==2 &&
            zero.diagnostics[1].code=="NO_CONTINUOUS_DENSITY",
            "Zero-variance Gaussian has no continuous density");
    in=flat();
    for (auto& h:in.noiseTransfer) h={0,0};
    require(!calculateComReceiverNoise(in).value->hasContinuousDensity,
            "Zero transfer is degenerate");

    auto bad=flat();
    bad.frequencyHz[0]=1;
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Missing DC refused");
    bad=flat();bad.frequencyHz[1]=0;
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Duplicate frequency refused");
    bad=flat();bad.frequencyHz[1]=-1;
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Reversed frequency refused");
    bad=flat();bad.frequencyHz.pop_back();
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Mismatched transfer frequency rejected");
    bad=flat();bad.noiseTransfer[1]=
        {std::numeric_limits<double>::quiet_NaN(),0};
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "NaN transfer rejected");
    bad=flat();bad.oneSidedPsdV2PerHz=-1;
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Negative PSD rejected");
    bad=flat();bad.oneSidedTailProbability=0.5;
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Unqualified tail rejected");
    bad=flat();bad.queryVoltageV[0]=
        std::numeric_limits<double>::infinity();
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Nonfinite query rejected");
    bad=flat();bad.queryVoltageV.assign(257,0.0);
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Sample budget rejected");
    bad=flat();bad.frequencyHz.assign(8194,0.0);
    bad.noiseTransfer.assign(8194,{1,0});
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Frequency budget rejected");
    bad=flat();bad.noiseTransfer[1]={1e308,0};
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Gain overflow rejected");
    // Codex P2: positive but subnormal intermediate PSD or H^2 must never
    // silently turn into a usable zero-variance distribution.
    bad=flat();
    bad.oneSidedPsdV2PerHz=1e-300;
    bad.frequencyHz={0,1e100};
    bad.noiseTransfer={{1e-30,0},{1e-30,0}};
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Weighted one-sided PSD underflow rejected");
    bad=flat();
    bad.noiseTransfer={{1e-200,0},{1e-200,0},{1e-200,0}};
    reject([&]{(void)calculateComReceiverNoise(bad);},
           "Transfer power underflow rejected");
    bad.oneSidedPsdV2PerHz=0;
    require(calculateComReceiverNoise(bad).usable(),
            "Exact zero PSD remains valid with tiny finite filter");
    std::atomic_bool cancelled{true};
    bool wasCancelled=false;
    try { (void)calculateComReceiverNoise(flat(),{},&cancelled); }
    catch(const Cancelled&) { wasCancelled=true; }
    require(wasCancelled,"Cancellation propagated");

    std::cout << "012_15 Gaussian receiver-noise mathematical oracle PASS; "
                 "NOT IEEE COM\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr << "012_15 receiver-noise FAIL: " << e.what() << '\n';
    return 1;
  }
}
