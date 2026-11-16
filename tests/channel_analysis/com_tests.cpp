#include "si/channel_analysis/com.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void demand(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
void equal(Complex actual, Complex expected, double tol, const char *why) {
  demand(std::abs(actual - expected) <= tol, why);
}
template<class Fn> void reject(Fn fn, const char *why) {
  bool caught = false;
  try { fn(); } catch (const Error &) { caught = true; }
  demand(caught, why);
}
ComplexTrace flat(const std::string &parameter, Complex gain = {1,0}) {
  ComplexTrace t;
  t.parameter = parameter;
  t.stimulusOhm = t.responseOhm = 100;
  t.frequencyHz = {0, 0.5e9, 1e9, 1.5e9, 2e9};
  t.transfer.assign(t.frequencyHz.size(), gain);
  return t;
}
ComInput input() {
  ComInput d;
  d.thru = flat("Sdd21");
  d.rxFilter = flat("Sdd21");
  d.aggressors.push_back({"xtalk-next", ComCoupling::Next,
                            flat("Sdd31", {0.1,0})});
  d.aggressors.push_back({"xtalk-fext", ComCoupling::Fext,
                            flat("Sdd41", {0,0.2})});
  return d;
}
ComSettings settings() {
  ComSettings s;
  s.symbolRateHz = 2e9;
  s.referenceOhm = 100;
  s.txFfeTaps = {1.0, -0.5};
  return s;
}
}
int main() {
  try {
    const auto source = input();
    const auto s = settings();
    const auto p = prepareComSpectra(source, s);
    demand(p.usable() && p.status == Status::Warning, "preparation must carry warning");
    demand(p.value->model == "EXPLORATORY_MATCHED_WAVE_SPECTRAL_PREPARATION",
           "model must remain non-normative");
    demand(p.value->aggressors.size() == 2, "NEXT and FEXT preserved separately");
    equal(p.value->txFfeResponse[0], {0.5,0}, 1e-12, "FIR DC");
    equal(p.value->txFfeResponse[2], {1.5,0}, 1e-12, "FIR half-baud");
    equal(p.value->conditionedThru[2], {1.5,0}, 1e-12, "thru FIR multiplication");
    equal(p.value->aggressors[0].conditionedTransfer[2], {0.15,0}, 1e-12,
          "NEXT transfer magnitude");
    equal(p.value->aggressors[1].conditionedTransfer[2], {0,0.3}, 1e-12,
          "FEXT complex phase preserved");
    const auto margin = calculateCom(source, s);
    demand(margin.status == Status::NotEvaluated && !margin.value,
           "missing normative PHY lock must NEVER report COM dB");
    demand(!margin.diagnostics.empty() &&
           margin.diagnostics[0].code == "COM_PROFILE_NOT_QUALIFIED",
           "blocked COM must explain missing independent qualification");
    auto altered = s;
    altered.profileId = "IEEE-802.3dj-draft";
    reject([&]{(void)prepareComSpectra(source, altered);}, "reject unapproved draft");
    altered = s;
    altered.symbolRateHz = std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)prepareComSpectra(source, altered);}, "require actual symbol rate");
    altered = s;
    altered.referenceOhm = 50;
    reject([&]{(void)prepareComSpectra(source, altered);}, "reject wrong Zref");
    altered = s;
    altered.txFfeTaps.clear();
    reject([&]{(void)prepareComSpectra(source, altered);}, "no imaginary FFE default");
    altered = s;
    altered.txFfeTaps = {std::numeric_limits<double>::infinity()};
    reject([&]{(void)prepareComSpectra(source, altered);}, "nonfinite tap");
    auto corrupted = source;
    corrupted.thru.transfer[1] = {NAN, 0};
    reject([&]{(void)prepareComSpectra(corrupted, s);}, "invalid source spectrum");
    corrupted = source;
    corrupted.rxFilter.frequencyHz[2] = 1.1e9;
    reject([&]{(void)prepareComSpectra(corrupted, s);}, "mismatched grid");
    corrupted = source;
    corrupted.aggressors.push_back(corrupted.aggressors.front());
    reject([&]{(void)prepareComSpectra(corrupted, s);}, "duplicate aggressor");
    corrupted = source;
    corrupted.thru.parameter = "S21";
    reject([&]{(void)prepareComSpectra(corrupted, s);}, "unqualified mode");
    corrupted = source;
    corrupted.thru.frequencyHz.erase(corrupted.thru.frequencyHz.begin());
    corrupted.thru.transfer.erase(corrupted.thru.transfer.begin());
    reject([&]{(void)prepareComSpectra(corrupted, s);}, "missing DC");
    std::atomic_bool cancelled{true};
    bool caught = false;
    try {(void)calculateCom(source, s, {}, &cancelled);}
    catch (const Cancelled &) {caught = true;}
    demand(caught, "propagate cancellation");
    std::cout << "012_09 COM spectral diagnostics and fail-closed PASS; "
                 "normative COM NOT EVALUATED\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "012_09 COM prequalification FAIL: " << e.what() << "\n";
    return 1;
  }
}
