#include "si/channel_analysis/com_interference.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

using namespace si;
using namespace si::channel;
namespace {
void demand(bool value, const char* why) {
  if (!value) throw std::runtime_error(why);
}
void close(double got, double expected, double tol, const char* why) {
  demand(std::isfinite(got) && std::abs(got - expected) <= tol, why);
}
template<class Fn> void reject(Fn fn, const char* why) {
  bool caught = false;
  try { fn(); } catch (const Error&) { caught = true; }
  demand(caught, why);
}
constexpr double df = 1e7;
constexpr size_t bins = 2901; // 29 GHz stop, 10 MHz step
ComPreparedData fixture() {
  ComPreparedData p;
  p.frequencyHz.reserve(bins);
  p.conditionedThru.assign(bins, {1., 0.});
  for (size_t k = 0; k < bins; ++k)
    p.frequencyHz.push_back(static_cast<double>(k) * df);
  return p;
}
ComPreparedAggressor aggressor(const std::string& id, ComCoupling kind,
                               double gain, double delay = 0) {
  ComPreparedAggressor a;
  a.id = id;
  a.coupling = kind;
  a.conditionedTransfer.reserve(bins);
  for (size_t k = 0; k < bins; ++k)
    a.conditionedTransfer.push_back(
        gain * std::polar(1., -2. * std::numbers::pi * k * df * delay));
  return a;
}
// Independent analytic finite-band rectangular-symbol pulse oracle.
// H(f) = g exp(-j 2pi f tau).
// p(t) = g/pi [Si(2pi F (t-tau)) + Si(2pi F (UI-t+tau))].
// The sine integral is evaluated on an independent Simpson grid, not using
// the production trapezoid implementation.
double sineIntegral(double x) {
  constexpr int steps = 20000;
  const double h = x / steps;
  double sum = 0;
  for (int i = 0; i <= steps; ++i) {
    const double y = i * h;
    const double v = i ? std::sin(y)/y : 1.;
    sum += (i==0 || i==steps ? 1. : (i&1 ? 4. : 2.)) * v;
  }
  return sum * h / 3.;
}
double oracle(double t, double delay, double gain) {
  const double ui = 1. / 53.125e9;
  const double f = (bins-1) * df;
  return gain/std::numbers::pi *
      (sineIntegral(2.*std::numbers::pi*f*(t-delay)) +
       sineIntegral(2.*std::numbers::pi*f*(ui-t+delay)));
}
} // namespace

int main() {
  try {
    const double ui = 1./53.125e9;
    const double delay = 5e-12;
    const std::vector<double> times{ui/8, ui/4, ui/2, 3*ui/4, ui};
    auto empty = fixture();
    const auto none = calculateComInterferencePulses(empty, times);
    demand(none.usable() && none.value->aggressors.empty(),
           "no aggressors must produce an explicit empty result");
    empty.aggressors.push_back(aggressor("zero", ComCoupling::Next, 0.));
    const auto zero = calculateComInterferencePulses(empty, times);
    for (double v : zero.value->aggressors.front().unitSymbolPulse)
      close(v, 0., 1e-15, "zero crosstalk must be zero");

    auto one = fixture();
    one.aggressors.push_back(aggressor("next", ComCoupling::Next, 0.125, delay));
    const auto next = calculateComInterferencePulses(one, times);
    demand(next.usable() && next.status == Status::Warning,
           "unqualified NEXT should be warning");
    demand(next.value->aggressors.front().coupling == ComCoupling::Next,
           "NEXT identity lost");
    for (size_t k = 0; k < times.size(); ++k)
      close(next.value->aggressors.front().unitSymbolPulse[k],
            oracle(times[k],delay,0.125), 3e-6,
            "independent finite-band phase/delay oracle");

    auto multi = one;
    multi.aggressors.push_back(aggressor("fext",ComCoupling::Fext,-0.25));
    const auto both = calculateComInterferencePulses(multi,times);
    demand(both.value->aggressors.size() == 2, "NEXT/FEXT count");
    for (size_t k = 0; k < times.size(); ++k) {
      close(both.value->aggressors[0].unitSymbolPulse[k],
            next.value->aggressors[0].unitSymbolPulse[k],1e-13,
            "adding FEXT must not change NEXT");
      close(both.value->aggressors[1].unitSymbolPulse[k],
            oracle(times[k],0.,-0.25),3e-6,
            "FEXT negative polarity independent oracle");
    }
    std::reverse(multi.aggressors.begin(),multi.aggressors.end());
    const auto reord = calculateComInterferencePulses(multi,times);
    demand(reord.value->aggressors[0].id == "fext" &&
           reord.value->aggressors[1].id == "next", "IDs reordered");
    for (size_t k = 0; k < times.size(); ++k)
      close(reord.value->aggressors[1].unitSymbolPulse[k],
            next.value->aggressors[0].unitSymbolPulse[k],1e-13,
            "reordering changed identified source");

    Com120F8LoadedTestCaseSpectra cases;
    cases.test1 = one;
    cases.test2 = one;
    cases.test2.aggressors.front() =
        aggressor("next",ComCoupling::Next,0.25,delay);
    const auto pair = calculateComInterferenceTestCases(cases,times);
    demand(pair.usable() &&
           pair.value->test1.aggressors[0].id=="next" &&
           pair.value->test2.aggressors[0].id=="next",
           "Test1/Test2 labels");
    for (size_t k = 0; k < times.size(); ++k)
      close(pair.value->test2.aggressors[0].unitSymbolPulse[k],
            2*pair.value->test1.aggressors[0].unitSymbolPulse[k],1e-12,
            "Test1/Test2 results mixed");
    cases.test2.aggressors[0].id = "different";
    reject([&]{(void)calculateComInterferenceTestCases(cases,times);},
           "Test1/Test2 source mismatch");

    auto bad = one;
    bad.aggressors.push_back(bad.aggressors.front());
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "duplicate ID");
    bad = one;
    bad.aggressors[0].conditionedTransfer[1] = {NAN,0};
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "NaN source");
    bad = one;
    bad.aggressors[0].conditionedTransfer[0] = {0.1,0.1};
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "nonreal DC");
    bad = one;
    bad.aggressors[0].coupling = static_cast<ComCoupling>(-1);
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "invalid coupling");
    bad = one;
    bad.aggressors[0].conditionedTransfer.pop_back();
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "spectrum length mismatch");
    bad = one;
    bad.frequencyHz[15] += 1e6;
    reject([&]{(void)calculateComInterferencePulses(bad,times);},
           "nonuniform frequency grid");
    reject([&]{(void)calculateComInterferencePulses(one,{ui,ui/2});},
           "non-increasing time");
    bad = fixture();
    for (size_t i = 0; i < 32; ++i)
      bad.aggressors.push_back(
          aggressor("a"+std::to_string(i),ComCoupling::Next,0.01));
    std::vector<double> times200;
    for (size_t i = 0; i < 200; ++i)
      times200.push_back(i*ui/8);
    reject([&]{(void)calculateComInterferencePulses(bad,times200);},
           "aggregate work cap");
    std::atomic_bool cancelled{true};
    bool cancelledReceived = false;
    try {(void)calculateComInterferencePulses(one,times,{},&cancelled);}
    catch (const Cancelled&) {cancelledReceived = true;}
    demand(cancelledReceived,"cancel requested");
    const auto margin = calculateCom(ComInput{},ComSettings{});
    demand(margin.status==Status::NotEvaluated && !margin.value,
           "normative COM gate changed");
    std::cout << "012_14 independent oracle, safety and fail-closed PASS\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr << "012_14 FAIL: " << e.what() << '\n';
    return 1;
  }
}
