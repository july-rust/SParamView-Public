#include "si/core.hpp"
#include "temp_directory.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

using namespace si;

static int checks = 0;
static void expect(bool condition, const std::string &message) {
  ++checks;
  if (!condition)
    throw Error("TEST FAILED: " + message);
}

template <class F> static void expectError(F &&fn, const std::string &message) {
  bool threw = false;
  try {
    fn();
  } catch (const Error &) {
    threw = true;
  }
  expect(threw, message);
}

int main() {
  const auto dir = fs::temp_directory_path() / "sparamview_v116_regression";
  fs::remove_all(dir);
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    auto write = [&](const std::string &name, const std::string &text) {
      const auto path = dir / name;
      std::ofstream(path, std::ios::binary) << text;
      return path;
    };
    const std::string data = "1 .1 0 .2 0 .3 0 .4 0\n2 .2 0 .4 0 .6 0 .8 0\n";
    auto canonical = Cache::open(
        write("canonical.s2p", "# GHz S RI R 50\n" + data), dir / "cache");
    auto reordered = Cache::open(
        write("reordered.s2p", "# GHz S R 50 RI\n" + data), dir / "cache");
    expect(canonical->meta().format == reordered->meta().format &&
               canonical->meta().reference == reordered->meta().reference &&
               canonical->frequencies() == reordered->frequencies() &&
               canonical->trace({{1, 1}}, {{0, 1}}) ==
                   reordered->trace({{1, 1}}, {{0, 1}}),
           "R option ordering is semantically identical");
    auto multiref = Cache::open(
        write("multiref.s2p", "# GHz S R 40 75 RI\n" + data), dir / "cache");
    expect(multiref->meta().reference.size() == 2 &&
               multiref->meta().reference[0] == 40 &&
               multiref->meta().reference[1] == 75 &&
               multiref->meta().version == "1.1",
           "Touchstone 1.1 per-port R values survive reordered options");
    expectError(
        [&] {
          Cache::open(write("duplicate-option.s2p",
                            "# GHz S RI R 50\n# GHz S RI R 50\n" + data),
                      dir / "cache");
        },
        "Duplicate option line is rejected");

    Trace ideal;
    ideal.parameter = "S11";
    ideal.referenceOhm = ideal.responseReferenceOhm = 50;
    for (int i = 0; i <= 1024; ++i) {
      ideal.x.push_back(i * 1e7);
      ideal.s.push_back(0);
    }
    for (double beta : {NaN, std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(), -0.1, 13.1}) {
      Settings s;
      s.kaiserBeta = beta;
      expect(transformTdr(ideal, s).quality == "UNSUITABLE",
             "Invalid Kaiser beta is fail-safe");
    }
    for (double beta : {0.0, 6.0, 13.0}) {
      Settings s;
      s.kaiserBeta = beta;
      expect(transformTdr(ideal, s).quality != "UNSUITABLE",
             "Valid Kaiser beta remains accepted");
    }
    {
      Settings s;
      s.tdrStartSeconds = NaN;
      expect(transformTdr(ideal, s).quality == "UNSUITABLE",
             "NaN TDR start is rejected");
    }
    {
      Settings s;
      s.tdrStartSeconds = -1e-12;
      expect(transformTdr(ideal, s).quality == "UNSUITABLE",
             "Negative TDR start is rejected");
    }
    {
      Settings s;
      s.tdrStopSeconds = NaN;
      expect(transformTdr(ideal, s).quality == "UNSUITABLE",
             "NaN TDR stop is rejected");
    }
    {
      Settings s;
      s.tdrStartSeconds = 2e-9;
      s.tdrStopSeconds = 1e-9;
      expect(transformTdr(ideal, s).quality == "UNSUITABLE",
             "TDR stop before start is rejected");
    }
    {
      Settings s;
      s.rl.enabled = false;
      s.rl.constant = NaN;
      auto result = analyze(ideal, Metric::TDR, s);
      expect(!result.plotX.empty(),
             "TDR analysis is independent of disabled invalid RL limit");
    }
    expectError(
        [&] {
          Settings s;
          s.targetOhm = NaN;
          (void)analyze(ideal, Metric::TDR, s);
        },
        "Non-finite TDR target is rejected");

    expect(overall({}) == "N/A", "Empty overall is N/A");
    expect(overall({"OK"}) == "OK", "Single OK overall");
    expect(overall({"OK", "OK"}) == "OK", "All OK overall");
    expect(overall({"OK", "N/A"}) == "N/A", "N/A is preserved");
    expect(overall({"NG", "OK"}) == "NG", "NG has priority");
    expect(overall({"NG", "N/A"}) == "NG", "NG has priority over N/A");
    expect(overall({"ERROR"}) == "N/A", "Unknown status cannot become OK");
    expect(overall({"OK", "UNKNOWN"}) == "N/A",
           "Mixed unknown status cannot become OK");

    Job missing;
    missing.victim.name = missing.victim.id = "missing";
    auto missingResult = runJobs({missing}, Settings{});
    expect(missingResult.size() == 1 && missingResult[0].status == "N/A" &&
               missingResult[0].note.find("no cache") != std::string::npos,
           "Null Job cache fails safely without crash");
    expectError([] { (void)workerLimit(-1, 1, 4); },
                "Negative performance rejected");
    expectError([] { (void)workerLimit(3, 1, 4); },
                "Out-of-range performance rejected");

    Trace empty;
    expectError([&] { (void)compare(empty, ideal, Metric::RL); },
                "Empty comparison trace rejected");
    Trace mismatched;
    mismatched.x = {1, 2};
    mismatched.s = {{1, 0}};
    expectError([&] { (void)compare(mismatched, ideal, Metric::RL); },
                "Mismatched comparison shape rejected");
    Trace zeroA, zeroB;
    zeroA.x = zeroB.x = {1, 2};
    zeroA.s = zeroB.s = {Complex{}, Complex{}};
    auto delta = compare(zeroA, zeroB, Metric::RL);
    expect(std::isnan(delta.worstImprovement),
           "All-invalid comparison reports N/A/NaN instead of +Inf");

    std::vector<double> ex{0, 1, 2, 3};
    std::vector<double> ey{NaN, 5, -2, 1};
    auto env = envelope(ex, ey, 1, 0, 3);
    expect(std::find(env.begin(), env.end(), size_t(1)) != env.end() &&
               std::find(env.begin(), env.end(), size_t(2)) != env.end(),
           "NaN-leading display bucket still retains finite min/max");

    auto csv = write("overflow.csv",
                     "Channel,NearP,NearN,FarP,FarN\nA,2147483648,,,\n");
    expectError([&] { (void)readMappingCsv(csv); },
                "Mapping CSV integer overflow rejected");

    std::cout << "v1.1.6 regression PASS " << checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
