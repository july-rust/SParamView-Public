#include "si/core.hpp"
#include "temp_directory.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using namespace si;

static void require(bool condition, const std::string &message) {
  if (!condition) throw Error(message);
}
static fs::path write(const fs::path &dir, const std::string &name,
                      const std::string &body) {
  const auto p = dir / name;
  std::ofstream stream(p, std::ios::binary);
  stream << body;
  require(bool(stream), "Could not write parser fixture");
  return p;
}
static std::string twoPort(const std::string &header) {
  return header + "# Hz S RI R 50\n1 0.1 0 0.2 0 0.3 0 0.4 0\n";
}
static std::string mixed(const std::string &order) {
  return "[Version] 2.1\n# Hz S RI R 50\n"
         "[Number of Ports] 2\n[Number of Frequencies] 1\n"
         "[Two-Port Data Order] 21_12\n[Mixed-Mode Order] " + order +
         "\n[Network Data]\n1 0.1 0 0.2 0 0.3 0 0.4 0\n[End]\n";
}
int main() {
  const auto dir = fs::temp_directory_path() / "sparamview-v133-parser";
  fs::remove_all(dir);
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    const auto cacheDir = dir / "cache";
    for (const auto &name : {"S1 S2", "s1 s2", "D1,2 C1,2"}) {
      const auto c = Cache::open(write(dir,"valid.ts",mixed(name)),cacheDir,nullptr,true);
      require(c->meta().ports == 2 && c->meta().points == 1,
              "Valid mixed-mode descriptor rejected");
    }
    for (const auto &bad : {"S0 S2", "S1xyz S2", "S2147483648 S2",
                            "S18446744073709551616 S2", "D1,0 C1,0",
                            "S1,2 S2", "D1,2extra C1,2"}) {
      bool rejectedAsParserError = false;
      try {
        Cache::open(write(dir,"invalid.ts",mixed(bad)),cacheDir,nullptr,true);
      } catch (const Error &) { rejectedAsParserError = true; }
      require(rejectedAsParserError, "Malformed mixed-mode port token accepted");
    }
    {
      const auto c = Cache::open(
          write(dir,"portlabels.s2p",twoPort(
              "! Port [1] = Legacy-label\n"
              "! Port2_J1_A1::DATA_P\n")),cacheDir);
      require(c->meta().labels.size() == 2 &&
              c->meta().labels[0] == "Legacy-label" &&
              c->meta().labels[1] == "J1_A1::DATA_P",
              "Port label grammar or physical identity changed");
    }
    for (const auto &bad : {
             "! Port 0 = invalid\n",
             "! Port 18446744073709551616 = overflow\n"}) {
      bool rejectedAsParserError = false;
      try {
        Cache::open(write(dir,"badlabel.s2p",twoPort(bad)),
                    cacheDir,nullptr,true);
      } catch (const Error &) { rejectedAsParserError = true; }
      require(rejectedAsParserError, "Invalid comment port index accepted");
    }
    {
      Metadata meta;
      meta.ports = 2;
      meta.reference = {50,50};
      const std::string name = std::string(20000, 'Z') +
                               "\xec\xb1\x84\xeb\x84\x90"; // UTF-8 채널
      meta.labels = {name+"_near",name+"_far"};
      const auto mapped = suggestMapping(meta);
      require(mapped.size() == 1 && mapped[0].name == name &&
              mapped[0].nearP == 0 && mapped[0].farP == 1,
              "Long UTF-8 suffix-based mapping changed");
    }
    {
      Metadata meta;
      meta.ports = 4;
      meta.reference = {50,50,50,50};
      meta.labels = {"U1-A DATAP0", "U1-B DATAN0",
                     "J1-1 DATAP0", "J1-2 DATAN0"};
      const std::string head =
          "\"D\" \"DATAP0\" \"DATAN0\" "
          "\"Port1_U1_A::DATAP0\" \"Port2_U1_B::DATAN0\" "
          "\"Port3_J1_1::DATAP0\" \"Port4_J1_2::DATAN0\" ";
      const std::string tail = "\"1\" \"2\" \"3\"";
      meta.differentialHints = {head+"\"0\" "+tail};
      std::string diagnostic;
      auto mapped = suggestMapping(meta,&diagnostic);
      require(diagnostic.empty() && mapped.size() == 1 &&
              mapped[0].differential() && mapped[0].nearP == 0 &&
              mapped[0].nearN == 1 && mapped[0].farP == 2 &&
              mapped[0].farN == 3,
              "Valid PowerSI 0-based hint changed");
      for (const auto &value : {"0.5", "-1", "1e309",
                                "18446744073709551616", "4294967296"}) {
        meta.differentialHints = {head+"\""+value+"\" "+tail};
        diagnostic.clear();
        mapped = suggestMapping(meta,&diagnostic);
        require(!diagnostic.empty(),
                "Malformed or oversized PowerSI port index confirmed");
      }
      meta.differentialHints = {
          "\"D\" \"DATAP0\" \"DATAN0\" "
          "\"Port18446744073709551616_U1_A::DATAP0\" "
          "\"Port2_U1_B::DATAN0\" \"Port3_J1_1::DATAP0\" "
          "\"Port4_J1_2::DATAN0\" \"0\" \"1\" \"2\" \"3\""};
      diagnostic.clear();
      mapped = suggestMapping(meta,&diagnostic);
      require(!diagnostic.empty(), "Overflowing physical endpoint trusted");
    }
    std::cout << "PASS v1.3.3 parser hardening and compatibility\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL v1.3.3: " << e.what() << '\n';
    return 1;
  }
}
