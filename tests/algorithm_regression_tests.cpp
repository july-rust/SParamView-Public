#include "si/core.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
using namespace si;

static int checks = 0;
static void expect(bool condition, const std::string &message) {
  ++checks;
  if (!condition)
    throw Error("TEST FAILED: " + message);
}
static void near(double a, double b, double tolerance,
                 const std::string &message) {
  expect(std::abs(a - b) <= tolerance,
         message + " got " + std::to_string(a) +
             " expected " + std::to_string(b));
}

int main() {
  try {
    Trace partial;
    partial.parameter = "S21";
    partial.x = {1e9, 2e9};
    partial.s = {Complex(.9, 0), Complex(.9, 0)};
    Settings s;
    s.startHz = 0; // automatic first-available policy, not a measured-DC demand
    s.stopHz = 10e9;
    s.il = {true, -3, {}};
    auto r = analyze(partial, Metric::IL, s);
    expect(r.status == "N/A", "partial requested band cannot pass overall");
    expect(std::isfinite(r.margin) && r.margin > 0,
           "partial-band evaluated margin is retained");
    expect(r.note.find("Requested range incomplete") != std::string::npos,
           "partial-band note is explicit");

    auto failing = partial;
    failing.s[0] = Complex(.1, 0);
    auto failed = analyze(failing, Metric::IL, s);
    expect(failed.status == "NG",
           "evaluated failure remains NG when requested band is incomplete");

    Settings available = s;
    available.stopHz = std::numeric_limits<double>::infinity();
    auto availableResult = analyze(partial, Metric::IL, available);
    expect(availableResult.status == "OK",
           "default start=0/full-available range does not require measured DC");

    Trace sparse;
    sparse.parameter = "S21";
    sparse.x = {1e9, 2e9};
    sparse.s = {Complex(.9, 0), Complex(-.9, 0)};
    Settings sparseSet;
    sparseSet.startHz = 1e9;
    sparseSet.stopHz = 2e9;
    sparseSet.il = {true, -3, {}};
    auto baseline = analyze(sparse, Metric::IL, sparseSet);
    expect(baseline.status == "OK", "sparse source samples pass constant limit");
    near(baseline.worst, 20 * std::log10(.9), 1e-12,
         "constant-limit worst uses source samples");

    sparseSet.manualMarkersHz = {1.5e9};
    auto withMarker = analyze(sparse, Metric::IL, sparseSet);
    expect(withMarker.status == baseline.status,
           "manual marker never changes verdict");
    expect(!withMarker.peaks.empty() && withMarker.peaks.back().manual &&
               std::isinf(withMarker.peaks.back().y) &&
               withMarker.peaks.back().y < 0,
           "manual marker exposes interpolated sparse-phase null");
    expect(withMarker.note.find("display only") != std::string::npos,
           "interpolated marker is labelled as display-only");

    sparseSet.manualMarkersHz.clear();
    sparseSet.il.points = {{1e9, -3}, {1.5e9, -3}, {2e9, -3}};
    auto equivalentLimit = analyze(sparse, Metric::IL, sparseSet);
    expect(equivalentLimit.status == baseline.status,
           "equivalent constant point-limit representation keeps verdict");
    near(equivalentLimit.worst, baseline.worst, 1e-12,
         "equivalent limit representation keeps worst value");

    sparseSet.il.points = {{1e9, -3}, {1.5e9, -4}, {2e9, -3}};
    auto genuineKink = analyze(sparse, Metric::IL, sparseSet);
    expect(genuineKink.status == "NG",
           "genuine limit kink is evaluated at its breakpoint");
    expect(genuineKink.note.find("limit boundary/kink") != std::string::npos,
           "interpolated kink evaluation is disclosed");

    Trace bounded;
    bounded.parameter = "S21";
    bounded.x = {1e9, 2e9};
    bounded.s = {Complex(.9, 0), Complex(.9, 0)};
    Settings boundedSet;
    boundedSet.startHz = 1e9;
    boundedSet.stopHz = 2e9;
    boundedSet.il = {true, -3, {{1.2e9, -3}, {1.8e9, -3}}};
    auto boundedResult = analyze(bounded, Metric::IL, boundedSet);
    expect(boundedResult.status == "N/A",
           "limit coverage outside its explicit endpoints remains N/A");
    expect(boundedResult.note.find("limit boundary/kink") != std::string::npos,
           "interior limit boundaries are evaluated and disclosed");

    Trace tdr;
    tdr.parameter = "S11";
    for (int k = 0; k <= 512; ++k) {
      tdr.x.push_back(k * 10e9 / 512.0);
      tdr.s.push_back(Complex(0, 0));
    }
    Settings ts;
    ts.tdrStopSeconds = 1e-6;
    ts.tdrLimitEnabled = true;
    auto partialTdr = analyze(tdr, Metric::TDR, ts);
    expect(partialTdr.status == "N/A",
           "TDR requested time beyond computed window cannot pass overall");
    expect(partialTdr.note.find("TDR stop") != std::string::npos,
           "TDR incomplete-time note is explicit");

    ts.targetOhm = 100;
    auto failedTdr = analyze(tdr, Metric::TDR, ts);
    expect(failedTdr.status == "NG",
           "TDR evaluated violation remains NG despite incomplete requested time");

    ts = Settings{};
    ts.tdrLimitEnabled = true;
    auto fullAvailableTdr = analyze(tdr, Metric::TDR, ts);
    expect(fullAvailableTdr.status == "OK",
           "default TDR available-time evaluation remains OK by policy");

    ts.stopHz = 20e9;
    auto missingFmax = analyze(tdr, Metric::TDR, ts);
    expect(missingFmax.status == "N/A",
           "TDR finite requested Fmax beyond source data is incomplete");
    expect(missingFmax.note.find("frequency stop") != std::string::npos,
           "TDR missing-frequency coverage is disclosed");

    // v1.2.1 P2 regression: Margin sorting must use one transitive policy even
    // when a result has no margin. These values formed A < B < C < A with the
    // old pair-dependent comparator.
    Result rankA, rankB, rankC;
    for (auto *value : {&rankA, &rankB, &rankC})
      value->metric = Metric::RL;
    rankA.channel = "A"; rankA.margin = 0.0; rankA.worst = 3.0;
    rankB.channel = "B"; rankB.margin = NaN; rankB.worst = 2.0;
    rankC.channel = "C"; rankC.margin = -1.0; rankC.worst = 1.0;
    const std::vector<std::vector<Result>> permutations = {
        {rankA, rankB, rankC}, {rankA, rankC, rankB}, {rankB, rankA, rankC},
        {rankB, rankC, rankA}, {rankC, rankA, rankB}, {rankC, rankB, rankA}};
    for (auto ranked : permutations) {
      rankResults(ranked, "Margin");
      expect(ranked[0].channel == "C" && ranked[1].channel == "A" &&
                 ranked[2].channel == "B",
             "v1.2.1 mixed-margin ordering stays deterministic");
    }

    // v1.2.1 P2 regression: a deliberate >1 reflection plateau at 5..6 ns
    // must not contaminate a selected 0..1 ns range. 10 GHz bandwidth gives
    // 50 ps spacing, hence 21 selected samples including both endpoints.
    Trace delayed;
    delayed.parameter = "S11";
    delayed.referenceOhm = 50;
    constexpr size_t bins = 1024;
    constexpr double fmax = 10e9;
    constexpr double pi = 3.14159265358979323846;
    constexpr double amplitude = 1.2;
    constexpr double firstDelay = 5e-9;
    constexpr double secondDelay = 6e-9;
    for (size_t i = 0; i <= bins; ++i) {
      const double frequency = fmax * double(i) / double(bins);
      delayed.x.push_back(frequency);
      delayed.s.push_back(
          std::polar(amplitude, -2 * pi * frequency * firstDelay) -
          std::polar(amplitude, -2 * pi * frequency * secondDelay));
    }
    Settings fullRange;
    fullRange.tdrStopSeconds = 8e-9;
    expect(transformTdr(delayed, fullRange).quality == "UNSUITABLE",
           "v1.2.1 TDR fixture still fails safe when singularity is selected");
    Settings selectedRange;
    selectedRange.tdrStopSeconds = 1.001e-9;
    const auto selected = transformTdr(delayed, selectedRange);
    expect(selected.quality == "GOOD" && selected.time.size() == 21 &&
               selected.impedance.size() == 21,
           "v1.2.1 out-of-range TDR singularity does not poison selected range");
    for (double ohms : selected.impedance)
      expect(std::isfinite(ohms) && std::abs(ohms - 50.0) < 0.5,
             "v1.2.1 selected TDR samples remain finite and near 50 ohms");

    std::cout << "PASS: " << checks << " algorithm boundary checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
