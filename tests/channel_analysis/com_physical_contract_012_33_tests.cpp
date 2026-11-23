// 012_33: independently precomputed complex ABCD Golden for a synthetic
// passive 4-port + two historical causal packages + fixed FFE/CTLE/RX.
// Synthetic source only: not an IEEE 802.3ck PHY reference/certification.
#include "si/channel_analysis/com_causal_channel.hpp"
#include "si/channel_analysis/com_120f8.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
  if(!ok) throw std::runtime_error(why);
}
void near(Complex got, Complex expected, const char* what) {
  // Pre-locked absolute complex tolerance, applied to Re/Im separately:
  // 5e-10 dimensionless (much smaller than any selected phase component).
  constexpr double epsilon=5e-10;
  demand(std::isfinite(got.real()) && std::isfinite(got.imag()) &&
         std::abs(got.real()-expected.real()) <= epsilon &&
         std::abs(got.imag()-expected.imag()) <= epsilon,what);
}
template<typename F> void rejects(F&& function, const char* why) {
  bool failed=false;
  try { function(); } catch(const si::Error&) { failed=true; }
  demand(failed,why);
}
ComPhysicalFourPort synthetic4p(double f) {
  ComPhysicalFourPort sample;
  sample.referenceOhm={50.,50.,50.,50.};
  const Complex g=std::polar(0.75,
      -2.*std::numbers::pi*f*12.e-12);
  // Four physical ports (nearP, nearN, farP, farN).
  // No mixed-mode conversion; matched differential board transfer.
  for(int pair=0;pair<2;++pair) {
    sample.scattering[4*(pair+2)+pair]=g;
    sample.scattering[4*pair+(pair+2)]=g;
  }
  return sample;
}
ComCausalPhysicalInput writeSyntheticS4p(const std::filesystem::path& file) {
  ComCausalPhysicalInput input;
  std::ofstream out(file);
  demand(bool(out),"cannot create ephemeral synthetic physical S4P");
  out<<"# GHz S RI R 50\n"<<std::setprecision(17);
  for(int k=0;k<=2657;++k) {
    const double f=static_cast<double>(k)*1.e7;
    const auto sample=synthetic4p(f);
    input.frequencyHz.push_back(f);
    input.boardS4p.push_back(sample);
    out<<f/1.e9;
    // Touchstone v1: stimulus column first; measured response inside.
    for(int stimulus=0;stimulus<4;++stimulus)
      for(int response=0;response<4;++response) {
        const Complex s=sample.scattering[4*response+stimulus];
        out<<" "<<s.real()<<" "<<s.imag();
      }
    out<<"\n";
  }
  out.close();
  demand(bool(out),"synthetic S4P write failed");
  return input;
}
void expectDiagnostic(const std::vector<Diagnostic>& diagnostics,
                      const std::string& code) {
  bool found=false;
  for(const auto& d:diagnostics)
    if(d.code==code) found=true;
  demand(found,"required fail-closed or exploratory diagnostic missing");
}
}

int main(int argc, char** argv) {
  try {
    demand(argc==2,"expected dedicated CTest temporary source folder");
    namespace fs=std::filesystem;
    const fs::path root=argv[1];
    fs::create_directories(root);
    const auto input=writeSyntheticS4p(root/"fixed-com-012_33.s4p");

    // 1. Independent ABCD oracle: each package is obtained by multiplying
    // shunt/series/causal-line ABCD matrices, then the matched board
    // [A B;C D] calculated analytically from S21=.75*exp(-j2*pi*f*12ps).
    // The expected Sij and H values below are PINNED numeric literals, not
    // obtained by a C++ function under test or by reading its output.
    const auto physical=calculateHistoricalCausalC2cCases(input);
    demand(physical.usable() && physical.status==Status::Warning,
           "synthetic historical causal result must warn");
    expectDiagnostic(physical.diagnostics,"NOT_IEEE_COM");
    demand(physical.value->test1.txPrimaryLineMm==13. &&
           physical.value->test1.rxPrimaryLineMm==11. &&
           physical.value->test2.txPrimaryLineMm==31. &&
           physical.value->test2.rxPrimaryLineMm==29.,
           "Test1/Test2 package identity mismatch");
    demand(physical.value->referenceOhmDifferential==100.,
           "single-ended 50 ohm source must yield differential 100 ohm");
    for(const auto& tc:{physical.value->test1,physical.value->test2})
      near(tc.loadedSdd[0].s21,{0.75,0.},
           "DC transmission violates separate package reference planes");

    constexpr size_t k10GHz=1000;
    constexpr size_t k2657GHz=2657;
    const auto& a=physical.value->test1.loadedSdd;
    const auto& b=physical.value->test2.loadedSdd;
    near(a[k10GHz].s11,{-0.013062413672278447,-0.19048964173558278},
         "Test1 near-port reflection ABCD Golden");
    near(a[k10GHz].s21,{0.5916511922056075,0.19225745881316827},
         "Test1 complex forward ABCD Golden");
    near(a[k10GHz].s22,{0.030514690993256547,-0.12145878853157195},
         "Test1 far-port reflection ABCD Golden");
    near(b[k10GHz].s11,{-0.1549611649491547,-0.15094647200903374},
         "Test2 near-port reflection ABCD Golden");
    near(b[k10GHz].s21,{0.28504798985641333,-0.40432116587475825},
         "Test2 complex forward ABCD Golden");
    near(b[k10GHz].s22,{-0.0922806182801023,-0.16783622490938535},
         "Test2 far-port reflection ABCD Golden");
    near(a[k10GHz].s12,a[k10GHz].s21,
         "Test1 passive reciprocal channel must preserve reverse direction");
    near(b[k10GHz].s12,b[k10GHz].s21,
         "Test2 passive reciprocal channel must preserve reverse direction");
    near(a[k2657GHz].s21,{0.2507290276716482,-0.2920026623662875},
         "Test1 near-Nyquist complex physical Golden");
    near(b[k2657GHz].s21,{0.24723377043074093,0.04807074227116718},
         "Test2 near-Nyquist complex physical Golden");

    Com120F8EqualizerChoice fixedEq;
    fixedEq.txFfe={-0.04,0.10,-0.20,0.56,-0.10};
    fixedEq.ctleDcGainDb=-20.;
    fixedEq.ctleDcGain2Db=-4.;
    validateCom120F8Equalizer(fixedEq);
    auto direct=prepareCom120F8LoadedTestCases(*physical.value,fixedEq);
    demand(direct.usable() && direct.status==Status::Warning,
           "fixed historical Test1/2 spectra should be available");
    expectDiagnostic(direct.diagnostics,"NOT_IEEE_COM");
    // Independent complex spectral oracle: ABCD S21 above, multiplied by
    // sum_k c_k exp(-j*2*pi*f*(k-3)/53.125GBd), CTLE transfer, fourth-order
    // receiver filter, and real Gaussian 7.5ps TX transition (once).
    near(direct.value->test1.conditionedThru[0],
         {0.01514297626752464,0.},
         "Test1 DC package/FFE/CTLE/RX Golden");
    near(direct.value->test2.conditionedThru[0],
         {0.01514297626752464,0.},
         "Test2 DC package/FFE/CTLE/RX Golden");
    near(direct.value->test1.conditionedThru[k10GHz],
         {0.09903983926314527,0.041418880560227654},
         "Test1 complete complex 10GHz Golden");
    near(direct.value->test2.conditionedThru[k10GHz],
         {0.054732367610984996,-0.06551197911112154},
         "Test2 complete complex 10GHz Golden");
    near(direct.value->test1.conditionedThru[k2657GHz],
         {-0.1987064153710583,-0.11649354516659073},
         "Test1 complete complex near-Nyquist Golden");
    near(direct.value->test2.conditionedThru[k2657GHz],
         {0.0019288695483848457,-0.1507199568823488},
         "Test2 complete complex near-Nyquist Golden");
    demand(std::abs(direct.value->test1.conditionedThru[k10GHz]-
                    direct.value->test2.conditionedThru[k10GHz])>0.05,
           "Test1/Test2 physical cases cannot silently collapse");

    // 2. Source-driven adapter retains full SourceSnapshot provenance
    // (the fixture is synthetic and its SHA must not claim IEEE origin).
    const auto cache=Cache::open(root/"fixed-com-012_33.s4p",
                                 root/"cache",nullptr,true);
    Channel ch;
    ch.id="victim"; ch.name="SYNTHETIC";
    ch.nearP=0; ch.nearN=1; ch.farP=2; ch.farN=3;
    const auto snapshot=SourceSnapshot::capture(
        cache,{ch},"synthetic-revision-012_33",17,19);
    auto observed=snapshot.identity();
    const auto fromSource=prepareCom120F8HistoricalCausalFromSource(
        snapshot,"victim",fixedEq,observed);
    demand(fromSource.usable() && fromSource.status==Status::Warning,
           "real Touchstone SourceSnapshot must reach both EQ paths");
    expectDiagnostic(fromSource.diagnostics,"NOT_IEEE_COM");
    expectDiagnostic(fromSource.diagnostics,"REFERENCE_BLOCKED");
    demand(fromSource.provenance.sourceSha256==cache->meta().sha256 &&
           fromSource.provenance.sourceSha256==observed.sha256 &&
           fromSource.provenance.revision==observed.revision &&
           fromSource.provenance.mappingIdentity==observed.mappingIdentity &&
           fromSource.provenance.workspaceGeneration==17 &&
           fromSource.provenance.mappingGeneration==19 &&
           fromSource.provenance.settingsIdentity.find("historical-causal")!=
              std::string::npos,
           "SourceSnapshot provenance/physical model not retained");
    near(fromSource.value->test1.conditionedThru[k10GHz],
         {0.09903983926314527,0.041418880560227654},
         "S4P Touchstone full source Test1 Golden");
    near(fromSource.value->test2.conditionedThru[k10GHz],
         {0.054732367610984996,-0.06551197911112154},
         "S4P Touchstone full source Test2 Golden");

    // 3. Explicit fail-closed paths: wrong package role, unsupported model,
    // stale observed live identity, invalid EQ, mapping, grid and cancel.
    auto broken=*physical.value;
    broken.test2.packageCase=ComPackageCase::Test1;
    rejects([&]{(void)prepareCom120F8LoadedTestCases(broken,fixedEq);},
            "relabelled Test2 must be rejected");
    broken=*physical.value;
    broken.test2.rxPrimaryLineMm=11.;
    rejects([&]{(void)prepareCom120F8LoadedTestCases(broken,fixedEq);},
            "swapped TX/RX package contract must be rejected");
    broken=*physical.value;
    broken.model="EXPLORATORY_S4P_TX_BOARD_RX_PHYSICAL_CASCADE";
    rejects([&]{(void)prepareCom120F8LoadedTestCases(broken,fixedEq);},
            "attenuation-only physical model cannot be laundered as causal");
    broken=*physical.value;
    broken.referenceOhmDifferential=50.;
    rejects([&]{(void)prepareCom120F8LoadedTestCases(broken,fixedEq);},
            "single-ended Zref cannot masquerade as differential Zref");
    auto brokenInput=input;
    brokenInput.boardS4p.pop_back();
    rejects([&]{(void)calculateHistoricalCausalC2cCases(brokenInput);},
            "S4P missing a frequency sample must be rejected");
    brokenInput=input;
    brokenInput.boardS4p[100].referenceOhm[0]=60.;
    rejects([&]{(void)calculateHistoricalCausalC2cCases(brokenInput);},
            "unequal physical port impedance must be rejected");
    brokenInput=input;
    brokenInput.boardS4p[100].scattering[8]={1.2,0.};
    rejects([&]{(void)calculateHistoricalCausalC2cCases(brokenInput);},
            "nonpassive physical 4-port must be rejected");
    auto invalidEq=fixedEq;
    invalidEq.ctleDcGainDb=-19.5;
    rejects([&]{(void)prepareCom120F8HistoricalCausalFromSource(
        snapshot,"victim",invalidEq,observed);},
        "off-grid CTLE must fail before source preparation");
    rejects([&]{(void)prepareCom120F8HistoricalCausalFromSource(
        snapshot,"unmapped-channel",fixedEq,observed);},
        "unmapped differential source channel cannot be guessed");
    rejects([&]{(void)prepareCom120F8HistoricalCausalFromSource(
        snapshot,"victim",fixedEq,observed,
        std::numeric_limits<double>::quiet_NaN());},
        "nonfinite mode conversion cannot be accepted");
    observed.workspaceGeneration++;
    const auto stale=prepareCom120F8HistoricalCausalFromSource(
        snapshot,"victim",fixedEq,observed);
    demand(stale.status==Status::Stale && !stale.value.has_value(),
           "changed workspace generation must fail as Stale");
    expectDiagnostic(stale.diagnostics,"SOURCE_STALE");
    observed=snapshot.identity();
    Control cancelled;
    cancelled.cancelled.store(true);
    bool wasCancelled=false;
    try {
      (void)prepareCom120F8HistoricalCausalFromSource(
          snapshot,"victim",fixedEq,observed,0.,&cancelled);
    } catch(const si::Cancelled&) {
      wasCancelled=true;
    }
    demand(wasCancelled,"physical source cancellation must propagate");
    const auto publicCom=calculateCom({},ComSettings{});
    demand(publicCom.status==Status::NotEvaluated &&
           !publicCom.value.has_value(),
           "production COM must remain NOT_EVALUATED");
    std::cout<<"012_33 source-backed fixed physical Test1/Test2 complex "
                "ABCD Goldens PASS; final IEEE COM REFERENCE BLOCKED\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"012_33 physical contract FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
