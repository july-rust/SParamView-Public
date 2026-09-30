#pragma once
#include <filesystem>
// Declare before cache owners, so Windows handles close before file cleanup.
struct TestDirectoryCleanup {
  std::filesystem::path path;
  ~TestDirectoryCleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
