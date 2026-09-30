#include "si/core.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static bool close(double a, double b, double eps = 1e-10) {
  return std::abs(a - b) <= eps * std::max({1.0, std::abs(a), std::abs(b)});
}
static void require(bool ok, const std::string &message) {
  if (!ok) throw si::Error("GOLDEN TEST FAILED: " + message);
}
int main(int argc, char **argv) {
  using namespace si;
  try {
    if (argc != 2) throw Error("golden test needs an output directory");
    fs::path root = argv[1]; fs::create_directories(root);
    fs::path file = root / "golden.s2p";
    {
      std::ofstream out(file);
      out << "! fixed numerical reference\n# GHz S RI R 50\n";
      out << "1  0.1 0  0.5 0  0.4 0  0.2 0\n";
      out << "2  0.1 0  0.5 0  0.4 0  0.2 0\n";
      out << "3  0.1 0  0.5 0  0.4 0  0.2 0\n";
    }
    auto cache = Cache::open(file, root / "cache");
    require(cache->meta().ports == 2 && cache->meta().points == 3, "header/point count");
    auto f = cache->frequencies();
    require(f.size() == 3 && close(f[0], 1e9) && close(f[2], 3e9), "GHz frequency scaling");
    Channel ch; ch.id = "gold"; ch.name = "GOLD"; ch.nearP = 0; ch.farP = 1;
    Settings s; s.startHz = 1e9; s.stopHz = 3e9;
    auto rl = loadTrace(*cache, ch, Metric::RL, s);
    auto il = loadTrace(*cache, ch, Metric::IL, s);
    require(close(logMagnitude(rl.s[1]), -20.0), "S11 golden return loss");
    require(close(logMagnitude(il.s[1]), 20 * std::log10(0.5)), "S21 golden insertion loss");
    s.reverse = true;
    auto rlr = loadTrace(*cache, ch, Metric::RL, s);
    auto ilr = loadTrace(*cache, ch, Metric::IL, s);
    require(close(logMagnitude(rlr.s[1]), 20 * std::log10(0.2)), "S22 golden return loss");
    require(close(logMagnitude(ilr.s[1]), 20 * std::log10(0.4)), "S12 golden insertion loss");
    for (auto value : {logMagnitude(rl.s[1]), logMagnitude(il.s[1]), logMagnitude(rlr.s[1]), logMagnitude(ilr.s[1])})
      require(std::isfinite(value), "no NaN/Inf in golden values");
    std::cout << "golden Touchstone numerical regression passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
