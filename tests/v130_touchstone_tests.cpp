#include "si/core.hpp"
#include "temp_directory.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace si;
static void require(bool okay, const std::string &why) {
  if (!okay) throw Error(why);
}
static fs::path writeFile(const fs::path &dir, const std::string &file,
                          const std::string &data) {
  auto path = dir / file;
  std::ofstream(path, std::ios::binary) << data;
  return path;
}
static std::string fixture(unsigned n, bool legacy, const std::string &version) {
  std::ostringstream out;
  for (unsigned i = 0; i < n; ++i) {
    const unsigned pair = (i % (n / 2)) / 2;
    const char pn = i % 2 ? 'P' : 'N';
    const auto chip = i < n/2 ? "J1" : "U1";
    if (legacy)
      out << "! Port " << (i+1) << " = " << chip << '-' << (i+1)
          << " TEST_D" << pair << pn << "\n";
    else
      out << "! Port" << (i+1) << '_' << chip << '_' << (i+1)
          << "::TEST_D" << pair << pn << "\n";
  }
  out << "!.DiffChannels\n";
  for (unsigned pair = 0; pair < n/4; ++pair) {
    const unsigned a=pair*2, b=a+1, c=a+n/2, d=b+n/2;
    auto endpoint = [&](unsigned k) {
      std::ostringstream os;
      os << "Port" << (k+1) << '_' << (k < n/2 ? "J1" : "U1") << '_'
         << (k+1) << "::TEST_D" << pair << (k%2 ? 'P' : 'N');
      return os.str();
    };
    out << "!\"Diff" << pair << "\" \"TEST_D" << pair << "P\" \"TEST_D"
        << pair << "N\" \"" << endpoint(b) << "\" \""
        << endpoint(a) << "\" \"" << endpoint(d) << "\" \""
        << endpoint(c) << "\" \"" << b << "\" \"" << a
        << "\" \"" << d << "\" \"" << c << "\"\n";
  }
  out << "!.EndDiffChannels\n";
  if (!version.empty()) out << "[Version] " << version << '\n';
  out << "# Hz S RI R 50\n";
  if (!version.empty())
    out << "[Number of Ports] " << n <<
           "\n[Number of Frequencies] 1\n[Network Data]\n";
  out << "1";
  for (unsigned k=0; k<n*n; ++k) out << " 0.1 0";
  out << "\n";
  if (!version.empty()) out << "[End]\n";
  return out.str();
}
int main() {
  const auto dir = fs::temp_directory_path() / "sparamview-v130-touchstone";
  fs::remove_all(dir);
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    for (unsigned n : {24u,40u}) {
      for (const std::string version : {"", "2.0", "2.1"}) {
        const auto name = std::string("synthetic") + std::to_string(n) +
                          (version.empty() ? ".s" + std::to_string(n) + "p" :
                           "_v" + version + ".ts");
        const auto path = writeFile(dir,name,fixture(n,false,version));
        auto cache = Cache::open(path,dir/"cache",nullptr,true);
        require(cache->meta().ports==n && cache->meta().points==1,
                "Port count / S-matrix record lost");
        require(cache->meta().labels.size()==n &&
                cache->meta().labels[0]=="J1_1::TEST_D0N",
                "Physical port label not preserved");
        std::string diagnostic;
        auto channels = suggestMapping(cache->meta(),&diagnostic);
        require(diagnostic.empty() && channels.size()==n/4,
                "Explicit differential channel hints not recognized");
        for (unsigned p=0;p<n/4;++p) {
          const auto &c=channels[p];
          require(c.differential() && c.nearP==int(p*2+1) &&
                  c.nearN==int(p*2) &&
                  c.farP==int(p*2+1+n/2) &&
                  c.farN==int(p*2+n/2),
                  "Incorrect near/far and positive/negative mapping");
        }
        auto again=Cache::open(path,dir/"cache",nullptr,true);
        require(again->reused() && again->meta().labels==cache->meta().labels &&
                suggestMapping(again->meta()).size()==channels.size(),
                "Cached metadata re-import mismatch");
      }
    }
    auto legacy=Cache::open(writeFile(dir,"legacy.s4p",fixture(4,true,"")),
                            dir/"cache");
    require(legacy->meta().labels[0]=="J1-1 TEST_D0N",
            "Legacy Port N = NAME comment changed");
    // Change the physical endpoint but keep net and port index unchanged:
    // strict vendor extensions must surface a diagnostic, not trust a hint.
    auto malformed=fixture(4,false,"");
    auto position=malformed.find("\"Port2_J1_2::TEST_D0P\"");
    require(position!=std::string::npos,"Fixture hint missing");
    malformed.replace(position, std::string("\"Port2_J1_2::TEST_D0P\"").size(),
                      "\"Port2_WRONG_2::TEST_D0P\"");
    auto bad=Cache::open(writeFile(dir,"mismatch.s4p",malformed),dir/"cache");
    std::string diagnostic;
    suggestMapping(bad->meta(),&diagnostic);
    require(!diagnostic.empty(),"Endpoint mismatch was silently trusted");
    for (const std::string invalid : {"1.0","1.1","1.9","2.2"}) {
      bool rejected=false;
      try {
        Cache::open(writeFile(dir,"invalid"+invalid+".ts",
                   "[Version] "+invalid+"\n# Hz S RI R 50\n"
                   "[Number of Ports] 1\n[Number of Frequencies] 1\n"
                   "[Network Data]\n1 0.1 0\n[End]\n"),dir/"cache");
      } catch (const Error &) { rejected=true; }
      require(rejected,"Invalid explicit version accepted: "+invalid);
    }
    auto v11=Cache::open(writeFile(dir,"v11.s2p",
                      "# Hz S RI R 40 75\n1 .1 0 .2 0 .3 0 .4 0\n"),
                         dir/"cache");
    require(v11->meta().version=="1.1" &&
            v11->meta().reference==std::vector<double>({40,75}),
            "Touchstone 1.1 per-port references missing");
    std::cout << "PASS SParamView v1.3.0 Touchstone regression\n";
    return 0;
  } catch(const std::exception &error) {
    std::cerr << "FAIL SParamView v1.3.0: " << error.what() << '\n';
    return 1;
  }
}
