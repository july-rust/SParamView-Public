#include "si/core.hpp"
#include "si/direction_policy.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static bool close(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps * std::max({1.0, std::abs(a), std::abs(b)});
}
static void require(bool ok, const std::string &message) {
  if (!ok) throw si::Error("TEST FAILED: " + message);
}

int main(int argc, char **argv) {
  using namespace si;
  try {
    if (argc != 2) throw Error("direction test needs an output directory");
    fs::path root = argv[1];
    fs::create_directories(root);
    fs::path file = root / "direction.s2p";
    {
      std::ofstream out(file);
      out << "# Hz S RI R 50\n";
      out << "1000000000  0.1 0  0.5 0  0.4 0  0.2 0\n";
      out << "2000000000  0.1 0  0.5 0  0.4 0  0.2 0\n";
    }
    auto cache = Cache::open(file, root / "cache");
    Channel ch; ch.id = "ch"; ch.name = "CH"; ch.nearP = 0; ch.farP = 1;
    Settings fwd, rev; fwd.startHz = rev.startHz = 1e9; fwd.stopHz = rev.stopHz = 2e9; rev.reverse = true;
    auto ilF = loadTrace(*cache, ch, Metric::IL, fwd);
    auto ilR = loadTrace(*cache, ch, Metric::IL, rev);
    auto rlF = loadTrace(*cache, ch, Metric::RL, fwd);
    auto rlR = loadTrace(*cache, ch, Metric::RL, rev);
    require(ilF.parameter == "S21" && ilR.parameter == "S12", "IL direction parameter mapping");
    require(rlF.parameter == "S11" && rlR.parameter == "S22", "RL direction parameter mapping");
    require(close(logMagnitude(ilF.s.front()), 20 * std::log10(0.5)), "forward IL magnitude");
    require(close(logMagnitude(ilR.s.front()), 20 * std::log10(0.4)), "reverse IL magnitude");
    require(close(logMagnitude(rlF.s.front()), -20.0), "near RL magnitude");
    require(close(logMagnitude(rlR.s.front()), 20 * std::log10(0.2)), "far RL magnitude");
    auto di = directionDelta(*cache, ch, Metric::IL, fwd);
    auto dr = directionDelta(*cache, ch, Metric::RL, fwd);
    require(close(di.maximumDb, std::abs(20 * std::log10(0.5) - 20 * std::log10(0.4))), "IL directional delta");
    require(close(dr.maximumDb, std::abs(-20.0 - 20 * std::log10(0.2))), "RL directional delta");
    require(close(di.frequency, 1e9) && close(dr.frequency, 1e9), "directional delta frequency");

    Job jf{cache, ch, std::nullopt, Metric::IL, "R"}; jf.reverse = false;
    Job jr = jf; jr.reverse = true;
    auto rs = runJobs({jf, jr}, fwd);
    require(rs.size() == 2, "two direction jobs return two rows");
    require(!rs[0].reverse && rs[0].parameter == "S21", "forward job result");
    require(rs[1].reverse && rs[1].parameter == "S12", "reverse job result");
    require(metricDirection(Metric::NEXT, false) == "Aggressor Near -> Victim Near", "NEXT forward label");
    require(metricDirection(Metric::NEXT, true) == "Aggressor Far -> Victim Far", "NEXT reverse label");
    require(metricDirection(Metric::FEXT, false) == "Aggressor Near -> Victim Far", "FEXT forward label");
    require(metricDirection(Metric::FEXT, true) == "Aggressor Far -> Victim Near", "FEXT reverse label");

    Channel nearOnly; nearOnly.id = "near"; nearOnly.name = "NEAR"; nearOnly.nearP = 0;
    Job oneRl{cache, nearOnly, std::nullopt, Metric::RL, "R"};
    require(directionAvailable(oneRl, false), "near-only RL forward is available");
    require(!directionAvailable(oneRl, true), "near-only RL reverse is skipped");
    Job oneIl{cache, nearOnly, std::nullopt, Metric::IL, "R"};
    require(!directionAvailable(oneIl, false) && !directionAvailable(oneIl, true), "near-only IL is skipped");
    Channel aggressor = nearOnly; aggressor.id = "ag"; aggressor.name = "AG";
    Job oneNext{cache, nearOnly, aggressor, Metric::NEXT, "R"};
    require(directionAvailable(oneNext, false), "near-only NEXT forward is available");
    require(!directionAvailable(oneNext, true), "near-only NEXT reverse is skipped");
    Job oneFext{cache, nearOnly, aggressor, Metric::FEXT, "R"};
    require(!directionAvailable(oneFext, false) && !directionAvailable(oneFext, true), "near-only FEXT is skipped without far victim endpoint");

    std::cout << "bidirectional direction/delta/endpoint-policy tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
