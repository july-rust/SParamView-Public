#include "si/core.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace si;
int main(int argc, char **argv) {
  try {
    if (argc < 3) {
      std::cout
          << "SParamView " << version
          << "\ninspect file.sNp [cache-dir]\ntrace file.sNp response-port "
             "stimulus-port output.csv [cache-dir]\nchannel file.sNp NearP "
             "NearN FarP FarN RL|IL|TDR output.csv [reference|resistor|split|open|short]\n";
      return 0;
    }
    std::string command = argv[1];
    auto src = pathFromUtf8(argv[2]);
    auto dir = fs::temp_directory_path() / "SParamView-cli-cache";
    if ((command == "inspect" || command == "benchmark") && argc > 3)
      dir = pathFromUtf8(argv[3]);
    if (command == "trace" && argc > 6)
      dir = pathFromUtf8(argv[6]);
    auto cache = Cache::open(src, dir);
    auto &m = cache->meta();
    if (command == "inspect") {
      std::cout << "version=" << m.version << "\nports=" << m.ports
                << "\npoints=" << m.points
                << "\ncache_reused=" << cache->reused()
                << "\nsha256=" << m.sha256 << "\n";
      return 0;
    }
    if (command == "benchmark") {
      for (int count : {1, 32, 64}) {
        auto begin = std::chrono::steady_clock::now();
        std::vector<std::vector<Complex>> traces;
        for (int i = 0; i < count; ++i) {
          int j = i % int(m.ports);
          traces.push_back(cache->trace(
              {{(j + int(m.ports) / 2) % int(m.ports), 1}}, {{j, 1}}));
        }
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
        std::cout << count << "_trace_read_ms=" << ms << "\n";
      }
      return 0;
    }
    if (command == "trace" && argc >= 6) {
      int i = std::stoi(argv[3]) - 1, j = std::stoi(argv[4]) - 1;
      auto f = cache->frequencies();
      auto s = cache->trace({{i, 1}}, {{j, 1}});
      auto outputPath = pathFromUtf8(argv[5]);
      if (outputPath.extension() != ".csv" ||
          (fs::exists(outputPath) && fs::equivalent(src, outputPath)))
        throw Error("Trace output must be a separate .csv file");
      std::ofstream out(outputPath);
      out << std::setprecision(17) << "Frequency_Hz,Real,Imaginary\n";
      for (size_t k = 0; k < f.size(); ++k)
        out << f[k] << ',' << s[k].real() << ',' << s[k].imag() << '\n';
      return out ? 0 : 1;
    }
    if (command == "channel" && argc >= 9) {
      Channel c;
      c.id = c.name = "Test";
      c.nearP = std::stoi(argv[3]) - 1;
      c.nearN = std::stoi(argv[4]) - 1;
      c.farP = std::stoi(argv[5]) - 1;
      c.farN = std::stoi(argv[6]) - 1;
      validateMapping({c}, m);
      std::string kind = argv[7];
      if (kind != "RL" && kind != "IL" && kind != "TDR")
        throw Error("CLI channel metric must be RL, IL or TDR");
      Metric metric = kind == "IL"    ? Metric::IL
                      : kind == "TDR" ? Metric::TDR
                                      : Metric::RL;
      Settings settings;
      Termination term=Termination::Reference;
      if(argc>9) {
        std::vector<std::string> names{"reference","resistor","split","open","short"};
        auto it=std::find(names.begin(),names.end(),argv[9]);
        if(it==names.end()||metric!=Metric::TDR)throw Error("Invalid TDR termination argument");
        term=Termination(it-names.begin());
      }
      settings.tdrStopSeconds=3e-9;
      auto t=loadTrace(*cache,c,metric,settings,nullptr,nullptr,term);
      exportRawCsv(pathFromUtf8(argv[8]), t, metric, settings);
      auto r = analyze(t, metric, settings);
      std::cout << std::setprecision(17) << "worst=" << r.worst
                << "\nworst_x=" << r.worstX << "\nquality=" << r.quality
                << "\nstatus=" << r.status << "\nreference=" << r.referenceOhm
                << "\n";
      return 0;
    }
    throw Error("Invalid command/arguments");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
