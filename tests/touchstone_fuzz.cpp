#include "si/core.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  namespace fs = std::filesystem;
  if (size > 65536) return 0;
  static uint64_t sequence = 0;
  const fs::path root = fs::temp_directory_path() / "sparamview-touchstone-fuzz";
  std::error_code ec; fs::create_directories(root, ec);
  const fs::path input = root / ("input-" + std::to_string(sequence++) + ".s2p");
  {
    std::ofstream out(input, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data), std::streamsize(size));
  }
  try {
    auto cache = si::Cache::open(input, root / "cache");
    (void)cache->frequencies();
    std::string diagnostic;
    (void)si::suggestMapping(cache->meta(), &diagnostic);
  } catch (const std::exception &) {
    // Invalid or incomplete Touchstone is an expected fuzz outcome.
  }
  fs::remove(input, ec);
  return 0;
}
