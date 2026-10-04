#include "si/core.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace si;

static int assertions = 0;
static void expect(bool condition, const std::string &message) {
  ++assertions;
  if (!condition)
    throw Error("TEST FAILED: " + message);
}

static Result rankedResult(const std::string &channel, double margin,
                           double worst) {
  Result result;
  result.metric = Metric::RL;
  result.channel = channel;
  result.margin = margin;
  result.worst = worst;
  return result;
}

int main() {
  try {
    // Regression for v1.2.0: pairwise switching between margin and severity
    // comparison could create A < B < C < A when one result had no margin.
    const auto a = rankedResult("A", 0.0, 3.0);
    const auto b = rankedResult("B", NaN, 2.0);
    const auto c = rankedResult("C", -1.0, 1.0);
    const std::vector<std::vector<Result>> permutations = {
        {a, b, c}, {a, c, b}, {b, a, c},
        {b, c, a}, {c, a, b}, {c, b, a}};
    for (auto ranked : permutations) {
      rankResults(ranked, "Margin");
      expect(ranked.size() == 3 && ranked[0].channel == "C" &&
                 ranked[1].channel == "A" && ranked[2].channel == "B",
             "Margin ranking is deterministic when a margin is missing");
    }

    auto missingLow = rankedResult("MissingLow", NaN, 2.0);
    auto missingHigh = rankedResult("MissingHigh", NaN, 4.0);
    std::vector<Result> missingOnly{missingLow, missingHigh};
    rankResults(missingOnly, "Margin");
    expect(missingOnly[0].channel == "MissingHigh" &&
               missingOnly[1].channel == "MissingLow",
           "Missing-margin results retain severity ordering within their class");

    // Build a synthetic reflection that is benign from 0 to 1 ns, then has a
    // >1 reflection plateau from 5 to 6 ns. The full range must remain
    // unsuitable, while a 0..1 ns selection must not inherit that out-of-range
    // singularity. 10 GHz bandwidth gives 50 ps TDR spacing (21 samples over
    // 0..1 ns inclusive).
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
      const Complex first = std::polar(amplitude, -2 * pi * frequency * firstDelay);
      const Complex second = std::polar(amplitude, -2 * pi * frequency * secondDelay);
      delayed.s.push_back(first - second);
    }

    Settings fullRange;
    fullRange.tdrStopSeconds = 8e-9;
    const auto full = transformTdr(delayed, fullRange);
    expect(full.quality == "UNSUITABLE",
           "Synthetic fixture contains a singularity inside the full TDR range");

    Settings selectedRange;
    selectedRange.tdrStartSeconds = 0;
    selectedRange.tdrStopSeconds = 1.001e-9;
    const auto selected = transformTdr(delayed, selectedRange);
    expect(selected.quality == "GOOD",
           "Out-of-range TDR singularity does not mark selected range unsuitable");
    expect(selected.time.size() == 21 && selected.impedance.size() == 21,
           "0..1 ns TDR selection keeps the expected 21 samples");
    for (double ohms : selected.impedance) {
      expect(std::isfinite(ohms),
             "Selected TDR range contains only finite impedance samples");
      expect(std::abs(ohms - 50.0) < 0.5,
             "Selected TDR range remains approximately 50 ohms");
    }

    std::cout << "PASS: " << assertions << " assertions\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
