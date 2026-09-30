#include "si/core.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 4)
      throw si::Error("Usage: real_file_probe source cache-dir output-dir");
    auto c =
        si::Cache::open(si::pathFromUtf8(argv[1]), si::pathFromUtf8(argv[2]));
    auto dest = si::pathFromUtf8(argv[3]);
    si::fs::create_directories(dest);
    auto &m = c->meta();
    auto f = c->frequencies();
    std::ofstream out(dest / "matrix.bin", std::ios::binary);
    uint64_t n = m.ports, k = m.points;
    out.write((char *)&n, 8);
    out.write((char *)&k, 8);
    out.write((char *)f.data(), std::streamsize(f.size() * 8));
    for (int i = 0; i < int(n); ++i)
      for (int j = 0; j < int(n); ++j) {
        auto v = c->trace({{i, 1}}, {{j, 1}});
        out.write((char *)v.data(),
                  std::streamsize(v.size() * sizeof(si::Complex)));
      }
    if (!out)
      throw si::Error("Probe output failure");
    std::ofstream meta(dest / "metadata.txt");
    meta << std::setprecision(17) << "format=" << m.format
         << "\nsha256=" << m.sha256 << "\n";
    for (size_t i = 0; i < m.ports; ++i)
      meta << i + 1 << '\t' << m.reference[i] << '\t' << m.labels[i] << '\n';
    for (const auto &w : m.warnings)
      meta << "NOTE\t" << w << '\n';
    si::writeMappingCsv(dest / "candidates.csv", si::suggestMapping(m));
    std::cout << "ports=" << n << " points=" << k << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
