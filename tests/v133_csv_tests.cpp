#include "si/core.hpp"
#include "temp_directory.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace si;
static void require(bool ok, const char *why) {
  if (!ok)
    throw Error(why);
}
static std::string readBytes(const fs::path &p) {
  std::ifstream input(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
static void writeBytes(const fs::path &p, const std::string &content) {
  std::ofstream out(p, std::ios::binary);
  out << content;
  require(bool(out), "Could not write fixture");
}
static Channel fixture(const std::string &text) {
  Channel c;
  c.name = c.group = c.alias = c.id = text;
  c.nearP = 0;
  c.nearN = 1;
  c.farP = 2;
  c.farN = 3;
  return c;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const fs::path dir = pathFromUtf8(argv[1]);
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    const std::vector<std::string> values{
        "plain", "=1+2", "+SUM(1,2)", "-1+2", "@SUM(1,2)",
        " =1+2", "\t=1+2", "\r\n=1+2", "\xef\xbc\x9d" "1+2",
        "'", "'=1+2", "''=1+2", "John's Channel",
        "quoted \"name\"", "comma,=1+2", "line\r\nbreak",
        "\xed\x95\x9c\xea\xb8\x80", "", " leading and trailing "};
    for (size_t i = 0; i < values.size(); ++i) {
      const fs::path a = dir / ("roundtrip-" + std::to_string(i) + ".csv");
      const fs::path b = dir / ("repeat-" + std::to_string(i) + ".csv");
      auto original = fixture(values[i]);
      if (values[i].empty()) {
        // Empty metadata is valid; channel names/IDs themselves may not be empty.
        original.name = "Empty metadata";
        original.id = "empty:metadata";
      }
      writeMappingCsv(a, {original});
      auto decoded = readMappingCsv(a);
      require(decoded.size() == 1, "CSV row count changed");
      const auto &got = decoded.front();
      require(got.name == original.name && got.group == original.group &&
                  got.alias == original.alias && got.id == original.id,
              "CSV text did not round-trip exactly");
      require(got.nearP == 0 && got.nearN == 1 && got.farP == 2 &&
                  got.farN == 3, "CSV numerical port columns changed");
      Metadata metadata;
      metadata.ports = 4;
      metadata.reference = {50, 50, 50, 50};
      validateMapping(decoded, metadata);
      writeMappingCsv(b, decoded);
      require(readBytes(a) == readBytes(b),
              "Repeated save changed CSV bytes or double-escaped the text");
      const auto exported = readBytes(a);
      require(exported.starts_with("\xef\xbb\xbf") &&
                  exported.find(",SParamViewCSV\r\n") != std::string::npos &&
                  exported.find(",v2-text-escaped\r\n") != std::string::npos,
              "CSV BOM/format marker missing");
      if (values[i].starts_with("=") || values[i].starts_with("+") ||
          values[i].starts_with("-") || values[i].starts_with("@"))
        require(exported.find("\"'" + values[i] + "\"") != std::string::npos,
                "Formula-looking text was exported without protection");
      require(exported.find(",1,2,3,4,") != std::string::npos,
              "Numerical port fields became string-prefixed");
    }
    const fs::path old = dir / "legacy.csv";
    writeBytes(old,
               "\xef\xbb\xbf"
               "Channel,NearP,NearN,FarP,FarN,Group,Alias,ID\r\n"
               "\"'=1+2\",1,2,3,4,\"'plain\",\"John's Channel\",\"''=1+2\"\r\n");
    auto legacy = readMappingCsv(old);
    require(legacy.size() == 1 && legacy[0].name == "'=1+2" &&
                legacy[0].group == "'plain" &&
                legacy[0].alias == "John's Channel" &&
                legacy[0].id == "''=1+2",
            "Legacy apostrophes were decoded as new format escapes");
    const fs::path oldSpaces = dir / "legacy-spaces.csv";
    writeBytes(oldSpaces, "Channel,NearP,NearN,FarP,FarN\r\n  old name  ,1,2,3,4\r\n");
    auto spaces = readMappingCsv(oldSpaces);
    require(spaces.size() == 1 && spaces[0].name == "old name" &&
                spaces[0].group == "Default" && spaces[0].id == "csv:old name",
            "Legacy trimming/default fields changed");
    const fs::path malformed = dir / "malformed.csv";
    writeBytes(malformed,
               "Channel,NearP,NearN,FarP,FarN,SParamViewCSV\r\n"
               "\"=1+2\",1,2,3,4,unknown-version\r\n");
    bool rejected = false;
    try {
      (void)readMappingCsv(malformed);
    } catch (const Error &) {
      rejected = true;
    }
    require(rejected, "Unknown versioned text encoding was not rejected");
    const fs::path badQuote = dir / "unclosed.csv";
    writeBytes(badQuote, "Channel,NearP,NearN,FarP,FarN\r\n\"unfinished,1,2,3,4");
    rejected = false;
    try {
      (void)readMappingCsv(badQuote);
    } catch (const Error &) {
      rejected = true;
    }
    require(rejected, "Unclosed CSV quote not rejected");
    std::cout << "PASS v1.3.3 Stage 2 Mapping CSV round-trip and security tests\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL v1.3.3 Stage 2: " << e.what() << '\n';
    return 1;
  }
}
