// Pin the OS file object so a pathname replacement cannot mutate an open Revision.
class CacheFile {
public:
#ifdef _WIN32
  HANDLE file = INVALID_HANDLE_VALUE;
#else
  int file = -1;
#endif
  explicit CacheFile(const fs::path &path) {
#ifdef _WIN32
    file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw Error("Cannot open cache");
#else
    file = ::open(path.c_str(), O_RDONLY);
    if (file < 0) throw Error("Cannot open cache");
#endif
  }
  CacheFile(const CacheFile &) = delete;
  CacheFile &operator=(const CacheFile &) = delete;
  uint64_t size() const {
#ifdef _WIN32
    LARGE_INTEGER n;
    if (!GetFileSizeEx(file, &n) || n.QuadPart < 0) throw Error("Cannot stat cache");
    return uint64_t(n.QuadPart);
#else
    struct stat info{};
    if (fstat(file, &info) || info.st_size < 0) throw Error("Cannot stat cache");
    return uint64_t(info.st_size);
#endif
  }
  Header header() const {
    Header h{};
#ifdef _WIN32
    DWORD n = 0;
    if (!ReadFile(file, &h, sizeof(h), &n, nullptr) || n != sizeof(h)) throw Error("Cannot read cache header");
#else
    if (pread(file, &h, sizeof(h), 0) != sizeof(h)) throw Error("Cannot read cache header");
#endif
    return h;
  }
  ~CacheFile() {
#ifdef _WIN32
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
#else
    if (file >= 0) ::close(file);
#endif
  }
};
// Serialize cache validation, creation and handle acquisition for one target.
// Weak entries keep completed imports from retaining a mutex for every file.
static std::shared_ptr<std::timed_mutex> cacheOpenMutex(const fs::path &target) {
  static std::mutex registryMutex;
  static std::map<fs::path, std::weak_ptr<std::timed_mutex>> registry;
  const auto key = fs::weakly_canonical(target.parent_path()) / target.filename();
  std::lock_guard<std::mutex> guard(registryMutex);
  for (auto it = registry.begin(); it != registry.end();) {
    if (it->second.expired())
      it = registry.erase(it);
    else
      ++it;
  }
  auto &entry = registry[key];
  auto mutex = entry.lock();
  if (!mutex) {
    mutex = std::make_shared<std::timed_mutex>();
    entry = mutex;
  }
  return mutex;
}
std::shared_ptr<Cache> Cache::open(const fs::path &src, const fs::path &dir,
                                   Control *c, bool verify) {
  check(c);
  if constexpr (std::endian::native != std::endian::little)
    throw Error("Only little-endian cache format is supported");
  auto source = fs::absolute(src);
  if (!fs::is_regular_file(source))
    throw Error("Touchstone not found: " + utf8(source));
  fs::create_directories(dir);
  auto key = utf8(source.lexically_normal());
  std::ostringstream keyout;
  keyout << std::hex << hashBytes(key.data(), key.size());
  auto target = dir / (keyout.str() + ".sicache");
  auto targetMutex = cacheOpenMutex(target);
  std::unique_lock<std::timed_mutex> targetLock(*targetMutex, std::defer_lock);
  while (!targetLock.try_lock_for(std::chrono::milliseconds(50)))
    check(c);
  check(c);
  auto cache = std::shared_ptr<Cache>(new Cache);
  Metadata m;
  m.source = utf8(source);
  m.sourceSize = fs::file_size(source);
  m.modified = fs::last_write_time(source).time_since_epoch().count();
  m.fingerprint = fingerprint(source);
  Header h{};
  bool valid = false;
  if (fs::exists(target)) {
    try {
      std::ifstream in(target, std::ios::binary);
      in.read(reinterpret_cast<char *>(&h), sizeof(h));
      auto size = fs::file_size(target);
      if (!in || std::memcmp(h.magic, "SICACH6", 7) || h.version != 6 || !h.n ||
          !h.f || !h.tile || h.n > uint64_t(std::numeric_limits<int>::max()) ||
          h.n > std::sqrt(double(std::numeric_limits<uint64_t>::max() / 16)) ||
          h.n*h.n > (512ULL*1024*1024)/sizeof(Complex) ||
          h.tile > 64 || h.block != h.tile * (8 + 16 * h.n * h.n) ||
          h.metaBytes > 64 * 1024 * 1024 || h.metaOffset > size ||
          h.metaBytes != size - h.metaOffset || h.size != m.sourceSize ||
          h.mtime != m.modified || h.fingerprint != m.fingerprint)
        throw Error("Stale cache");
      if ((h.f - 1) / h.tile + 1 >
              (std::numeric_limits<uint64_t>::max() - 512) / h.block ||
          h.metaOffset != 512 + ((h.f - 1) / h.tile + 1) * h.block)
        throw Error("Truncated cache");
      in.seekg(static_cast<std::streamoff>(h.metaOffset));
      std::string s(h.metaBytes, '\0');
      in.read(s.data(), static_cast<std::streamsize>(s.size()));
      if (!in || hashBytes(s.data(), s.size()) != h.metaChecksum)
        throw Error("Corrupt cache");
      auto old = decodeMeta(s, h);
      if (old.source != m.source ||
          (verify && sha256File(source, c) != old.sha256))
        throw Error("Hash mismatch");
      parseBasis(old);
      m = old;
      valid = true;
    } catch (const Cancelled &) {
      throw;
    } catch (const std::exception &) {
      valid = false;
    }
  }
  if (!valid) {
    auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    Temporary tmp{
        dir / (keyout.str() + "-" + std::to_string(nonce) + ".sicache.tmp")};
    h = importFile(source, tmp.path, m, c);
    check(c);
    atomicReplace(tmp.path, target, true);
  }
  cache->file_ = std::make_shared<CacheFile>(target);
  const auto opened = cache->file_->header();
  if (std::memcmp(&opened, &h, sizeof(h))) throw Error("Cache changed while opening; retry");
  cache->path_ = target;
  cache->metadata_ = m;
  cache->tileFrames_ = h.tile;
  cache->blockBytes_ = h.block;
  cache->reused_ = valid;
  targetLock.unlock();
  if (c)
    c->update(1, valid ? "Cache opened" : "Import complete");
  return cache;
}
// Map just the tile needed for a read, then immediately unmap. Never copy the
// cache into heap.
class Mapping {
#ifdef _WIN32
  HANDLE handle = nullptr;
#endif
  std::shared_ptr<CacheFile> storage;
  uint64_t fileBytes = 0;
  void *data = nullptr;
  size_t length = 0;

public:
  explicit Mapping(std::shared_ptr<CacheFile> source) : storage(std::move(source)), fileBytes(storage->size()) {
#ifdef _WIN32
    handle = CreateFileMappingW(storage->file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!handle) throw Error("Cannot map cache");
#endif
  }
  const char *map(uint64_t offset, size_t bytes) {
    unmap();
    if (!bytes || offset > fileBytes || bytes > fileBytes - offset)
      throw Error("Truncated cache; remove cache and reimport");
    uint64_t align;
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    align = info.dwAllocationGranularity;
#else
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pageSize <= 0)
      throw Error("Cannot determine system page size");
    align = static_cast<uint64_t>(pageSize);
#endif
    auto start = offset - offset % align;
    length = bytes + static_cast<size_t>(offset - start);
#ifdef _WIN32
    data = MapViewOfFile(handle, FILE_MAP_READ, static_cast<DWORD>(start >> 32),
                         static_cast<DWORD>(start & 0xffffffff), length);
    if (!data)
      throw Error("MapViewOfFile failed");
#else
    data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, storage->file,
                static_cast<off_t>(start));
    if (data == MAP_FAILED) {
      data = nullptr;
      throw Error("mmap failed");
    }
#endif
    return static_cast<char *>(data) + (offset - start);
  }
  void unmap() {
    if (data) {
#ifdef _WIN32
      UnmapViewOfFile(data);
#else
      munmap(data, length);
#endif
      data = nullptr;
    }
  }
  ~Mapping() {
    unmap();
#ifdef _WIN32
    if (handle)
      CloseHandle(handle);
#endif
  }
};
std::vector<double> Cache::frequencies(Control *c) const {
  std::vector<double> f(metadata_.points);
  Mapping map(file_);
  for (uint64_t start = 0; start < metadata_.points; start += tileFrames_) {
    check(c);
    auto p = map.map(dataOffset_ + (start / tileFrames_) * blockBytes_,
                     static_cast<size_t>(tileFrames_ * 8));
    auto n = std::min(tileFrames_, metadata_.points - start);
    std::memcpy(f.data() + start, p, static_cast<size_t>(n * 8));
    for (uint64_t k = start; k < start+n; ++k)
      if (!std::isfinite(f[k]) || f[k] < 0 || (k && f[k] <= f[k-1]))
        throw Error("Corrupt cached frequency order; remove cache and reimport");
  }
  return f;
}
std::vector<Complex>
Cache::trace(const std::vector<std::pair<int, double>> &response,
             const std::vector<std::pair<int, double>> &stimulus,
             Control *c) const {
  auto basis = parseBasis(metadata_);
  auto project = [&](const auto &weights) {
    std::map<int, double> v;
    for (auto [p, w] : weights) {
      if (p < 0 || uint64_t(p) >= metadata_.ports)
        throw Error("Port out of range");
      if (basis.empty())
        v[p] += w;
      else
        for (size_t i = 0; i < basis.size(); ++i) {
          auto b = basis[i];
          if (b.kind == 's' && b.p == p)
            v[int(i)] += w;
          if (b.kind != 's') {
            if (b.p == p)
              v[int(i)] += w / std::sqrt(2.);
            if (b.q == p)
              v[int(i)] += w * (b.kind == 'd' ? -1 : 1) / std::sqrt(2.);
          }
        }
    }
    return v;
  };
  auto rw = project(response), sw = project(stimulus);
  std::vector<Complex> result(metadata_.points);
  Mapping map(file_);
  for (uint64_t start = 0; start < metadata_.points; start += tileFrames_) {
    check(c);
    const auto blockOffset = dataOffset_ + (start / tileFrames_) * blockBytes_;
    auto n = std::min(tileFrames_, metadata_.points - start);
    for (auto [i, a] : rw)
      for (auto [j, b] : sw) {
        if (std::abs(a * b) < 1e-16)
          continue;
        auto offset =
            tileFrames_ * 8 +
            (uint64_t(i) * metadata_.ports + uint64_t(j)) * tileFrames_ * 16;
        const auto p =
            map.map(blockOffset + offset, static_cast<size_t>(n * 16));
        for (uint64_t k = 0; k < n; ++k) {
          Complex z;
          std::memcpy(&z, p + k * 16, 16);
          if (!std::isfinite(z.real()) || !std::isfinite(z.imag()))
            throw Error("Corrupt cached sample; remove cache and reimport");
          result[start + k] += a * b * z;
        }
      }
  }
  return result;
}
