#include "si/core.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#undef far
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace si {
static constexpr double pi = 3.1415926535897932384626433832795;
std::string utf8(const fs::path &p) {
  auto s = p.u8string();
  return {reinterpret_cast<const char *>(s.data()), s.size()};
}
fs::path pathFromUtf8(const std::string &s) {
  return fs::path(
      std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
void Control::update(double f, const std::string &s) const {
  check();
  if (progress)
    progress(f, s);
}
static void check(Control *c) {
  if (c)
    c->check();
}
static std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == s.npos)
    return {};
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
static std::string lower(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
static std::vector<std::string> words(const std::string &s) {
  std::istringstream in(s);
  std::vector<std::string> v;
  std::string t;
  while (in >> t)
    v.push_back(t);
  return v;
}
static double number(std::string s) {
  for (char &c : s)
    if (c == 'D' || c == 'd')
      c = 'e';
  if (!s.empty() && s[0] == '+')
    s.erase(0, 1);
  double v;
  auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (r.ec != std::errc() || r.ptr != s.data() + s.size() || !std::isfinite(v))
    throw Error("Invalid finite number: " + s);
  return v;
}
static uint64_t positiveInteger(const std::string &s) {
  uint64_t n;
  auto r = std::from_chars(s.data(), s.data() + s.size(), n);
  if (r.ec != std::errc() || r.ptr != s.data() + s.size() || n == 0)
    throw Error("Invalid positive integer: " + s);
  return n;
}
static uint64_t hashBytes(const void *p, size_t n,
                          uint64_t h = 14695981039346656037ULL) {
  auto b = static_cast<const unsigned char *>(p);
  for (size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 1099511628211ULL;
  }
  return h;
}
// SHA-256 per FIPS 180-4. Only used during initial import or explicit
// verification.
class Sha256 {
  std::array<uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::array<unsigned char, 64> buffer{};
  size_t used = 0;
  uint64_t bytes = 0;
  void block(const unsigned char *p) {
    static constexpr uint32_t k[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
             (uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
      auto a =
          std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      auto b =
          std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + a + w[i - 7] + b;
    }
    auto [a, b, c, d, e, f, g, hh] = h;
    for (int i = 0; i < 64; ++i) {
      auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      auto t1 = hh + s1 + ((e & f) ^ (~e & g)) + k[i] + w[i];
      auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      auto t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }

public:
  void add(const char *p, size_t n) {
    bytes += n;
    while (n) {
      size_t m = std::min(n, 64 - used);
      std::memcpy(buffer.data() + used, p, m);
      used += m;
      p += m;
      n -= m;
      if (used == 64) {
        block(buffer.data());
        used = 0;
      }
    }
  }
  std::string finish() {
    uint64_t bits = bytes * 8;
    unsigned char one = 0x80;
    add(reinterpret_cast<char *>(&one), 1);
    unsigned char zero = 0;
    while (used != 56)
      add(reinterpret_cast<char *>(&zero), 1);
    unsigned char tail[8];
    for (int i = 0; i < 8; ++i)
      tail[7 - i] = static_cast<unsigned char>(bits >> (i * 8));
    add(reinterpret_cast<char *>(tail), 8);
    std::ostringstream out;
    for (auto v : h)
      out << std::hex << std::setw(8) << std::setfill('0') << v;
    return out.str();
  }
};
std::string sha256File(const fs::path &path, Control *c) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw Error("Cannot read source: " + utf8(path));
  Sha256 sha;
  std::array<char, 65536> b;
  while (in) {
    check(c);
    in.read(b.data(), b.size());
    sha.add(b.data(), static_cast<size_t>(in.gcount()));
  }
  if (in.bad())
    throw Error("Source read failed");
  return sha.finish();
}
static uint64_t fingerprint(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw Error("Cannot read: " + utf8(path));
  auto n = fs::file_size(path);
  uint64_t h = hashBytes(&n, sizeof(n));
  std::array<char, 65536> b;
  for (uint64_t off :
       {uint64_t(0), n / 2, n > 65536 ? n - 65536 : uint64_t(0)}) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(off));
    in.read(b.data(), b.size());
    h = hashBytes(b.data(), static_cast<size_t>(in.gcount()), h);
  }
  return h;
}
struct Header {
  char magic[8];
  uint64_t version, n, f, tile, block, metaOffset, metaBytes, size;
  int64_t mtime;
  uint64_t fingerprint, metaChecksum;
  char reserved[384 + 32];
};
static_assert(sizeof(Header) == 512);
static void atomicReplace(const fs::path &tmp, const fs::path &dest,
                          [[maybe_unused]] bool preserveOpenCache = false) {
#ifdef _WIN32
  if (!MoveFileExW(tmp.c_str(), dest.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    // Preserve this thread's failure code before formatting or any other API call.
    const DWORD moveError = GetLastError();
    DWORD error = moveError;
    const char *operation = "MoveFileExW";
    // An open CacheFile denies MoveFileExW replacement on Windows. ReplaceFileW
    // preserves that handle's old data while publishing the new cache at dest.
    if (preserveOpenCache && moveError == ERROR_ACCESS_DENIED) {
      if (ReplaceFileW(dest.c_str(), tmp.c_str(), nullptr, 0, nullptr, nullptr))
        return;
      error = GetLastError();
      operation = "ReplaceFileW";
    }
    wchar_t buffer[2048]{};
    const DWORD chars = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, buffer, DWORD(std::size(buffer)), nullptr);
    std::string message = "Windows error message unavailable";
    if (chars) {
      const int bytes = WideCharToMultiByte(
          CP_UTF8, 0, buffer, int(chars), nullptr, 0, nullptr, nullptr);
      if (bytes > 0) {
        message.resize(size_t(bytes));
        if (!WideCharToMultiByte(CP_UTF8, 0, buffer, int(chars),
                                 message.data(), bytes, nullptr, nullptr))
          message = "Windows error message unavailable";
        while (!message.empty() &&
               (message.back() == '\r' || message.back() == '\n'))
          message.pop_back();
      }
    }
    std::ostringstream detail;
    detail << "Cannot commit file\nOperation: " << operation;
    if (preserveOpenCache && moveError == ERROR_ACCESS_DENIED)
      detail << "\nInitial MoveFileExW Error: " << moveError;
    detail << "\nWin32 Error: " << error
           << "\nMessage: " << message
           << "\nSource: " << utf8(tmp)
           << "\nDestination: " << utf8(dest);
    throw Error(detail.str());
  }
#else
  fs::rename(tmp, dest);
#endif
}
struct Temporary {
  fs::path path;
  ~Temporary() {
    std::error_code ec;
    fs::remove(path, ec);
  }
};
static std::string encodeMeta(const Metadata &m) {
  std::ostringstream out;
  out << std::quoted(m.source) << '\n'
      << std::quoted(m.version) << '\n'
      << std::quoted(m.format) << '\n'
      << std::quoted(m.matrix) << '\n'
      << std::quoted(m.order) << '\n'
      << std::quoted(m.sha256) << '\n';
  out << m.reference.size() << '\n' << std::setprecision(17);
  for (double r : m.reference)
    out << r << ' ';
  out << '\n';
  for (auto *v :
       {&m.labels, &m.mixedOrder, &m.warnings, &m.differentialHints}) {
    out << v->size() << '\n';
    for (auto &s : *v)
      out << std::quoted(s) << '\n';
  }
  return out.str();
}
static Metadata decodeMeta(const std::string &s, const Header &h) {
  Metadata m;
  std::istringstream in(s);
  in >> std::quoted(m.source) >> std::quoted(m.version) >>
      std::quoted(m.format) >> std::quoted(m.matrix) >> std::quoted(m.order) >>
      std::quoted(m.sha256);
  size_t n = 0;
  in >> n;
  if (n != h.n)
    throw Error("Corrupt cache reference metadata");
  m.reference.resize(n);
  for (auto &r : m.reference) {
    in >> r;
    if (!(r > 0) || !std::isfinite(r))
      throw Error("Corrupt cache reference");
  }
  for (auto *v :
       {&m.labels, &m.mixedOrder, &m.warnings, &m.differentialHints}) {
    in >> n;
    if (n > h.n + 1000)
      throw Error("Corrupt cache metadata size");
    v->resize(n);
    for (auto &t : *v)
      in >> std::quoted(t);
  }
  if (!in)
    throw Error("Corrupt cache metadata");
  m.ports = h.n;
  m.points = h.f;
  m.sourceSize = h.size;
  m.modified = h.mtime;
  m.fingerprint = h.fingerprint;
  return m;
}
// Fixed-size input buffer: even a legal multi-gigabyte data line is never held
// in memory.
class Scanner {
  std::ifstream in;
  std::array<char, 65536> buf{};
  size_t pos = 0, len = 0;
  uint64_t consumed = 0, total;
  Control *control;
  Sha256 sha;

public:
  uint64_t line = 1;
  enum Kind { Word, Newline, Comment, Keyword, Option, End };
  struct Token {
    Kind kind;
    std::string text;
  };
  Scanner(const fs::path &p, Control *c)
      : in(p, std::ios::binary), total(fs::file_size(p)), control(c) {
    if (!in)
      throw Error("Cannot read " + utf8(p));
    if (peek() == 0xEF) {
      get();
      if (get() != 0xBB || get() != 0xBF)
        throw Error("Invalid UTF-8 BOM");
    }
  }
  int peek() {
    if (pos == len) {
      check(control);
      in.read(buf.data(), buf.size());
      len = static_cast<size_t>(in.gcount());
      pos = 0;
      if (!len) {
        if (in.bad())
          throw Error("Read error");
        return -1;
      }
      sha.add(buf.data(), len);
      consumed += len;
      if (control && consumed % (4 * 1024 * 1024) < 65536)
        control->update(0.9 * double(consumed) / std::max<uint64_t>(1, total),
                        "Importing Touchstone");
    }
    return static_cast<unsigned char>(buf[pos]);
  }
  int get() {
    int c = peek();
    if (c >= 0)
      ++pos;
    return c;
  }
  Token next() {
    int c;
    while ((c = peek()) == ' ' || c == '\t')
      get();
    if (c < 0)
      return {End, {}};
    if (c == '\n' || c == '\r') {
      get();
      if (c == '\r' && peek() == '\n') get();
      ++line;
      return {Newline, {}};
    }
    if (c == '!') {
      get();
      std::string s;
      while ((c = peek()) >= 0 && c != '\n' && c != '\r') {
        get();
        if (s.size() < 65536)
          s += char(c);
      }
      return {Comment, s};
    }
    if (c == '[') {
      get();
      std::string s;
      while ((c = get()) >= 0 && c != ']' && c != '\n' && c != '\r') {
        if (s.size() > 128)
          throw Error("Overlong keyword");
        s += char(c);
      }
      if (c != ']')
        throw Error("Unclosed keyword");
      return {Keyword, lower(trim(s))};
    }
    if (c == '#') {
      get();
      return {Option, {}};
    }
    std::string s;
    while ((c = peek()) >= 0 && c != ' ' && c != '\t' && c != '\r' &&
           c != '\n' && c != '!' && c != '[' && c != '#') {
      get();
      if (s.size() > 4096)
        throw Error("Overlong numeric token");
      s += char(c);
    }
    return {Word, s};
  }
  std::string restLine() {
    std::string s;
    while (peek() >= 0 && peek() != '\n' && peek() != '\r' && peek() != '!') {
      if (s.size() > 65536)
        throw Error("Header exceeds 64 KiB");
      s += char(get());
    }
    return trim(s);
  }
  std::string digest() { return sha.finish(); }
};
struct Basis {
  char kind;
  int p, q;
};
static std::vector<Basis> parseBasis(const Metadata &m) {
  std::vector<Basis> out;
  std::vector<int> used(m.ports);
  std::map<std::pair<int, int>, int> pairs;
  std::regex rx("([sdc])([0-9]+)(?:,([0-9]+))?", std::regex::icase);
  for (auto token : m.mixedOrder) {
    std::smatch a;
    if (!std::regex_match(token, a, rx))
      throw Error("Invalid Mixed-Mode Order: " + token);
    char k = lower(a[1])[0];
    int p = std::stoi(a[2]) - 1, q = a[3].matched ? std::stoi(a[3]) - 1 : -1;
    if (p < 0 || uint64_t(p) >= m.ports ||
        (k != 's' && (q < 0 || uint64_t(q) >= m.ports || p == q)) ||
        (k == 's' && q != -1))
      throw Error("Mixed-mode port out of range");
    if (k == 's') {
      used[p] += 10;
    } else {
      used[p]++;
      used[q]++;
      auto key = std::make_pair(p, q);
      int bit = k == 'd' ? 1 : 2;
      if (pairs[key] & bit)
        throw Error("Duplicate mixed-mode descriptor");
      pairs[key] |= bit;
      if (std::abs(m.reference[p] - m.reference[q]) >
          1e-10 * std::max(m.reference[p], m.reference[q]))
        throw Error("Mixed-mode P/N references must be equal");
    }
    out.push_back({k, p, q});
  }
  if (!out.empty()) {
    if (out.size() != m.ports)
      throw Error("Mixed-mode descriptor count mismatch");
    for (int n : used)
      if (n != 2 && n != 10)
        throw Error("Incomplete or overlapping mixed-mode ports");
    for (auto &[k, v] : pairs)
      if (v != 3)
        throw Error("D and C descriptors must use the same ordered port pair");
  }
  return out;
}
static Header importFile(const fs::path &source, const fs::path &temporary,
                         Metadata &m, Control *control) {
  Scanner scan(source, control);
  Header h{};
  std::memcpy(h.magic, "SICACH6", 7);
  h.version = 6;
  h.size = m.sourceSize;
  h.mtime = m.modified;
  h.fingerprint = m.fingerprint;
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  if (!out)
    throw Error("Cannot create cache");
  out.write(reinterpret_cast<char *>(&h), sizeof(h));
  std::smatch ext;
  std::string filename = lower(utf8(source.filename()));
  if (std::regex_search(filename, ext, std::regex("\\.s([0-9]+)p$")))
    m.ports = positiveInteger(ext[1]);
  double scale = 1e9;
  std::vector<double> optionRef{50};
  std::map<int, std::string> labels;
  std::vector<double> f;
  std::vector<Complex> tile;
  bool option = false, network = false, initialized = false, ended = false,
       information = false, noise = false;
  std::string continuation;
  bool differentialSection = false, impedancePending = false;
  std::vector<double> impedanceValues;
  size_t impedanceBlocks = 0;
  std::set<std::string> seen;
  uint64_t expected = 0, noiseExpected = 0, noiseTokens = 0, pair = 0,
           scalar = 0, frame = 0, row = 0, col = 0;
  double first = 0, lastFrequency = -1;
  unsigned noiseColumns = 0;
  double lastNoiseFrequency = -1;
  auto finishNoiseLine = [&] {
    if (noiseColumns && noiseColumns != 5)
      throw Error("Noise record requires exactly five values on one line");
    noiseColumns = 0;
  };
  auto addNoise = [&](double value) {
    if (noiseColumns >= 5)
      throw Error("Noise record requires exactly five values; decreasing network frequency is invalid");
    if (!noiseColumns) {
      double frequency = value * scale;
      if (!std::isfinite(frequency) || frequency < 0 || frequency <= lastNoiseFrequency ||
          (!noiseTokens && frequency > lastFrequency))
        throw Error("Invalid noise frequency order");
      lastNoiseFrequency = frequency;
    }
    ++noiseColumns;
    ++noiseTokens;
  };
  std::regex labelRx(R"(^\s*Port\s*\[?\s*(\d+)\s*\]?\s*[=:]\s*(.+)$)",
                     std::regex::icase);
  auto flush = [&]() {
    if (frame == 0)
      return;
    out.write(reinterpret_cast<const char *>(f.data()),
              static_cast<std::streamsize>(f.size() * 8));
    out.write(reinterpret_cast<const char *>(tile.data()),
              static_cast<std::streamsize>(tile.size() * sizeof(Complex)));
    if (!out)
      throw Error("Cache write failed (disk full?)");
    frame = 0;
    std::fill(f.begin(), f.end(), 0);
    std::fill(tile.begin(), tile.end(), Complex{});
  };
  auto init = [&]() {
    if (initialized)
      return;
    if (!option)
      throw Error("Missing # option line");
    if (!m.ports)
      throw Error("Port count absent: use .sNp or [Number of Ports]");
    if (m.ports > uint64_t(std::numeric_limits<int>::max()) ||
        m.ports > std::sqrt(double(std::numeric_limits<size_t>::max() /
                                   sizeof(Complex))))
      throw Error("Port matrix exceeds address space");
    const uint64_t matrixPairs = m.matrix == "Full" ? m.ports*m.ports : m.ports*(m.ports+1)/2;
    if (m.ports*m.ports > (512ULL*1024*1024)/sizeof(Complex) || matrixPairs > m.sourceSize/3)
      throw Error("Port matrix exceeds the 512 MiB frame budget or cannot fit in this source file");
    if (m.version[0] == '2') {
      if (!network || !expected || !seen.count("number of ports"))
        throw Error("Missing required Touchstone 2.x keyword");
      if (m.ports == 2 && !seen.count("two-port data order"))
        throw Error("2.x two-port file requires [Two-Port Data Order]");
      if (optionRef.size() != 1)
        throw Error("2.x per-port reference must use [Reference]");
    }
    if (m.reference.empty())
      m.reference = optionRef;
    if (m.reference.size() == 1 && !seen.count("reference"))
      m.reference.resize(m.ports, m.reference.front());
    if (m.reference.size() != m.ports)
      throw Error("Reference impedance count does not match ports");
    for (double r : m.reference)
      if (!(r > 0))
        throw Error("Reference impedance must be positive");
    if (m.version[0] != '2' && optionRef.size() > 1)
      m.version = "1.1";
    parseBasis(m);
    if (m.matrix != "Full" && !m.mixedOrder.empty())
      throw Error(
          "Triangular mixed-mode matrices are not defined by Touchstone 2.x");
    h.n = m.ports;
    h.tile = std::max<uint64_t>(
        1, std::min<uint64_t>(64, (8 * 1024 * 1024) / (8 + 16 * h.n * h.n)));
    h.block = h.tile * (8 + 16 * h.n * h.n);
    f.resize(h.tile);
    tile.resize(h.tile * h.n * h.n);
    initialized = true;
  };
  try {
    while (true) {
      auto t = scan.next();
      if (t.kind == Scanner::End) {
        finishNoiseLine();
        break;
      }
      if (t.kind == Scanner::Newline) {
        finishNoiseLine();
        continue;
      }
      if (t.kind == Scanner::Comment) {
        std::smatch match;
        const auto comment = trim(t.text);
        if (std::regex_match(comment, match, labelRx))
          labels[std::stoi(match[1]) - 1] = trim(match[2]);
        const auto folded = lower(comment);
        if (folded == ".diffchannels") {
          differentialSection = true;
          continue;
        }
        if (folded == ".enddiffchannels") {
          differentialSection = false;
          continue;
        }
        if (differentialSection && !comment.empty()) {
          if (m.differentialHints.size() >= m.ports)
            throw Error("Too many differential channel hints");
          m.differentialHints.push_back(comment);
        }
        const bool impedanceStart = folded.starts_with("port impedance");
        if (impedanceStart || impedancePending) {
          if (!initialized)
            throw Error("Port Impedance comment before network record");
          if (impedanceStart) {
            if (impedancePending)
              throw Error("Incomplete Port Impedance comment");
            impedanceValues.clear();
          }
          for (const auto &value :
               words(impedanceStart ? comment.substr(14) : comment))
            impedanceValues.push_back(number(value));
          if (impedanceValues.size() > 2 * m.ports)
            throw Error("Too many Port Impedance values");
          impedancePending = impedanceValues.size() != 2 * m.ports;
          if (!impedancePending) {
            for (size_t i = 0; i < m.ports; ++i) {
              const double tolerance = 1e-10 * std::max(1.0, m.reference[i]);
              if (std::abs(impedanceValues[2 * i] - m.reference[i]) >
                      tolerance ||
                  std::abs(impedanceValues[2 * i + 1]) > tolerance)
                throw Error("Port Impedance differs from fixed real Reference; "
                            "renormalize the export before analysis");
            }
            ++impedanceBlocks;
          }
        }
        continue;
      }
      if (impedancePending)
        throw Error("Incomplete Port Impedance comment");
      if (information) {
        if (t.kind == Scanner::Keyword && t.text == "end information")
          information = false;
        continue;
      }
      if (ended)
        throw Error("Unexpected data after [End]");
      if (t.kind == Scanner::Option) {
        auto s = words(scan.restLine());
        if (option)
          continue;
        if (initialized)
          throw Error("Option line after network data");
        option = true;
        bool refs = false;
        optionRef.clear();
        for (auto x : s) {
          auto w = lower(x);
          if (refs) {
            optionRef.push_back(number(w));
            continue;
          }
          if (w == "hz")
            scale = 1;
          else if (w == "khz")
            scale = 1e3;
          else if (w == "mhz")
            scale = 1e6;
          else if (w == "ghz")
            scale = 1e9;
          else if (w == "s") {
          } else if (w == "ri")
            m.format = "RI";
          else if (w == "ma")
            m.format = "MA";
          else if (w == "db")
            m.format = "DB";
          else if (w == "r")
            refs = true;
          else
            throw Error("Unsupported option (S-parameters only): " + x);
        }
        if (refs && optionRef.empty())
          throw Error("Missing R value");
        if (optionRef.empty())
          optionRef = {50};
        continue;
      }
      if (t.kind == Scanner::Keyword) {
        auto key = t.text;
        auto value = scan.restLine();
        continuation.clear();
        if (scalar != 0)
          throw Error("Incomplete frequency record: expected " +
                      std::to_string(m.matrix == "Full"
                                         ? m.ports * m.ports
                                         : m.ports * (m.ports + 1) / 2) +
                      " pairs; got " + std::to_string(pair));
        if (!seen.insert(key).second && key != "begin information" &&
            key != "end information")
          throw Error("Duplicate keyword: " + key);
        if (key == "end") {
          ended = true;
          continue;
        }
        if (key == "noise data") {
          if (m.ports != 2 || !noiseExpected)
            throw Error("Invalid noise section");
          noise = true;
          continue;
        }
        if (key == "begin information") {
          information = true;
          continue;
        }
        if (initialized)
          throw Error("Header keyword after network data: " + key);
        if (key == "version") {
          if (value != "1.0" && value != "1.1" && value != "2.0" &&
              value != "2.1")
            throw Error("Unsupported Touchstone version: " + value);
          m.version = value;
        } else if (key == "number of ports") {
          auto n = positiveInteger(value);
          if (m.ports && m.ports != n)
            throw Error("Extension and [Number of Ports] disagree");
          m.ports = n;
        } else if (key == "number of frequencies")
          expected = positiveInteger(value);
        else if (key == "number of noise frequencies") {
          if (value == "0")
            noiseExpected = 0;
          else
            noiseExpected = positiveInteger(value);
        } else if (key == "two-port data order") {
          if (value != "12_21" && value != "21_12")
            throw Error("Invalid two-port order");
          m.order = value;
        } else if (key == "matrix format") {
          auto v = lower(value);
          if (v == "full")
            m.matrix = "Full";
          else if (v == "lower")
            m.matrix = "Lower";
          else if (v == "upper")
            m.matrix = "Upper";
          else
            throw Error("Invalid matrix format");
        } else if (key == "reference") {
          continuation = "reference";
          for (auto &w : words(value))
            m.reference.push_back(number(w));
        } else if (key == "mixed-mode order") {
          continuation = "mixed";
          m.mixedOrder = words(value);
        } else if (key == "network data") {
          network = true;
          init();
        } else
          throw Error("Unsupported keyword: [" + key + "]");
        continue;
      }
      if (continuation == "reference") {
        m.reference.push_back(number(t.text));
        continue;
      }
      if (continuation == "mixed") {
        m.mixedOrder.push_back(t.text);
        continue;
      }
      double v = number(t.text);
      if (noise) {
        addNoise(v);
        continue;
      }
      init();
      if (scalar == 0) {
        double frequency = v * scale;
        if (!std::isfinite(frequency) || frequency < 0)
          throw Error("Invalid frequency");
        if (frequency <= lastFrequency) {
          if (m.version[0] == '1' && m.ports == 2 &&
              frequency <= lastFrequency) {
            noise = true;
            addNoise(v);
            m.warnings.push_back("Legacy 2-port noise data ignored");
            continue;
          }
          throw Error("Frequencies must be strictly increasing");
        }
        if (expected && m.points >= expected)
          throw Error("More frequencies than declared");
        lastFrequency = frequency;
        f[frame] = frequency;
        scalar = 1;
        pair = 0;
        row = 0;
        col = 0;
        continue;
      }
      if (scalar % 2 == 1) {
        first = v;
        ++scalar;
        continue;
      }
      Complex z;
      if (m.format == "RI")
        z = {first, v};
      else {
        if (m.format == "MA" && first < 0)
          throw Error("Negative MA magnitude");
        double mag = m.format == "DB" ? std::pow(10., first / 20.) : first;
        z = std::polar(mag, v * pi / 180.);
      }
      if (!std::isfinite(z.real()) || !std::isfinite(z.imag()))
        throw Error("Non-finite complex value");
      uint64_t r = row, c = col;
      if (m.ports == 2 && m.matrix == "Full" && m.order == "21_12")
        std::swap(r, c);
      tile[(r * m.ports + c) * h.tile + frame] = z;
      if (m.matrix != "Full")
        tile[(c * m.ports + r) * h.tile + frame] = z;
      ++pair;
      ++scalar;
      if (m.matrix == "Full") {
        if (++col == m.ports) {
          col = 0;
          ++row;
        }
      } else if (m.matrix == "Lower") {
        if (++col > row) {
          col = 0;
          ++row;
        }
      } else if (++col == m.ports) {
        ++row;
        col = row;
      }
      auto pairs =
          m.matrix == "Full" ? m.ports * m.ports : m.ports * (m.ports + 1) / 2;
      if (pair == pairs) {
        scalar = 0;
        ++m.points;
        ++frame;
        if (frame == h.tile)
          flush();
      }
    }
    if (impedancePending)
      throw Error("Incomplete Port Impedance comment");
    if (differentialSection)
      throw Error("Unclosed differential channel hints");
    if (impedanceBlocks)
      m.warnings.push_back(
          "Port Impedance comments checked against fixed real Reference at " +
          std::to_string(impedanceBlocks) + " records");
    if (information)
      throw Error("Unclosed information section");
    if (scalar != 0)
      throw Error("Truncated frequency record: expected " +
                  std::to_string(m.matrix == "Full"
                                     ? m.ports * m.ports
                                     : m.ports * (m.ports + 1) / 2) +
                  " pairs; got " + std::to_string(pair));
    if (!m.points)
      throw Error("No S-parameter records");
    if (expected && expected != m.points)
      throw Error("[Number of Frequencies] mismatch");
    if (m.version[0] == '2' && !ended)
      throw Error("Missing [End]");
    if (noiseTokens % 5 ||
        ((m.version[0] == '2') && noiseTokens / 5 != noiseExpected))
      throw Error("Noise data count mismatch");
    if (m.ports != 2 && seen.count("two-port data order"))
      throw Error("Two-port order in non-two-port file");
    flush();
    h.f = m.points;
    m.labels.resize(m.ports);
    for (auto &[p, s] : labels)
      if (p >= 0 && uint64_t(p) < m.ports)
        m.labels[p] = s;
    m.sha256 = scan.digest();
    if (fs::file_size(source) != m.sourceSize ||
        fs::last_write_time(source).time_since_epoch().count() != m.modified ||
        fingerprint(source) != m.fingerprint)
      throw Error("Source changed during import; retry");
    auto meta = encodeMeta(m);
    h.metaOffset = static_cast<uint64_t>(out.tellp());
    h.metaBytes = meta.size();
    h.metaChecksum = hashBytes(meta.data(), meta.size());
    out.write(meta.data(), static_cast<std::streamsize>(meta.size()));
    out.seekp(0);
    out.write(reinterpret_cast<char *>(&h), sizeof(h));
    out.flush();
    if (!out)
      throw Error("Cache commit failed");
    out.close();
    return h;
  } catch (const Cancelled &) {
    throw;
  } catch (const std::exception &e) {
    throw Error(utf8(source.filename()) + " / line " +
                std::to_string(scan.line) + " / frequency " +
                std::to_string(lastFrequency) + " Hz: " + e.what());
  }
}
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
    align = static_cast<uint64_t>(sysconf(_SC_PAGE_SIZE));
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
void validateMapping(const std::vector<Channel> &channels, const Metadata &m) {
  std::set<std::string> ids, names;
  std::set<int> claimed;
  for (auto &c : channels) {
    if (c.name.empty() || c.id.empty() || !names.insert(lower(c.name)).second ||
        !ids.insert(c.id).second)
      throw Error("Mapping needs unique non-empty names and IDs");
    std::vector<int> ports{c.nearP};
    if (c.farP >= 0)
      ports.push_back(c.farP);
    if (c.nearN >= 0)
      ports.push_back(c.nearN);
    if (c.farN >= 0)
      ports.push_back(c.farN);
    if (c.nearP < 0 || (c.farN >= 0 && c.nearN < 0) ||
        (c.nearN >= 0 && ((c.farP >= 0) != (c.farN >= 0))))
      throw Error("Incomplete P/N or near/far mapping: " + c.name);
    for (int p : ports) {
      if (p < 0 || uint64_t(p) >= m.ports)
        throw Error("Port out of range: " + c.name);
      if (!claimed.insert(p).second)
        throw Error("Port assigned more than once: " + std::to_string(p + 1));
    }
    for (auto [p, n] : {std::pair{c.nearP, c.nearN}, std::pair{c.farP, c.farN}})
      if (n >= 0 && std::abs(m.reference[p] - m.reference[n]) >
                        1e-10 * std::max(m.reference[p], m.reference[n]))
        throw Error(
            "Unequal P/N references require renormalization (not in V1): " +
            c.name);
  }
}
std::vector<Channel> suggestMapping(const Metadata &m,
                                    std::string *diagnostic) {
  // PowerSI exports explicit endpoint indices in .DiffChannels comments.
  // They are physical single-ended ports, NOT pre-converted mixed-mode data.
  if (!m.differentialHints.empty()) {
    try {
      std::vector<Channel> candidates;
      for (const auto &line : m.differentialHints) {
        std::istringstream input(line);
        std::vector<std::string> fields;
        std::string token;
        while (input >> std::quoted(token))
          fields.push_back(token);
        if (fields.size() < 11)
          throw Error("Invalid PowerSI differential channel hint");
        const auto firstNet = lower(fields[1]), secondNet = lower(fields[2]);
        // Only one differing, underscore-delimited P/N token is unambiguous.
        size_t polarity = std::string::npos;
        if (firstNet.size() == secondNet.size()) {
          for (size_t i = 0; i < firstNet.size(); ++i) {
            if (firstNet[i] == secondNet[i]) continue;
            if (polarity != std::string::npos ||
                !((firstNet[i] == 'p' && secondNet[i] == 'n') ||
                  (firstNet[i] == 'n' && secondNet[i] == 'p')) ||
                (i > 0 && firstNet[i - 1] != '_') ||
                (i + 1 < firstNet.size() && firstNet[i + 1] != '_')) {
              polarity = std::string::npos;
              break;
            }
            polarity = i;
          }
        }
        if (polarity == std::string::npos)
          throw Error("Cannot establish P/N polarity from PowerSI channel hint");
        const bool firstPositive = firstNet[polarity] == 'p';
        std::string base = fields[1];
        if (polarity + 1 < base.size()) base.erase(polarity, 2);
        else if (polarity > 0) base.erase(polarity - 1, 2);
        else base.clear();
        if (base.empty()) throw Error("PowerSI hint has no channel name");
        std::array<int, 4> ports;
        for (size_t i = 0; i < 4; ++i) {
          const double index = number(fields[7 + i]);
          if (index < 0 || index >= double(m.ports) || index != std::floor(index))
            throw Error("PowerSI hint port index out of range");
          ports[i] = int(index);
          const auto &net = fields[1 + i % 2];
          if (size_t(ports[i]) >= m.labels.size())
            throw Error("PowerSI hint port has no physical label");
          auto actualNet = lower(trim(m.labels[size_t(ports[i])]));
          const auto separator = actualNet.find_last_of(" \t");
          if (separator != std::string::npos)
            actualNet = lower(trim(actualNet.substr(separator + 1)));
          if (actualNet != lower(trim(net)))
            throw Error("PowerSI hint does not match port label");
          std::smatch endpoint;
          if (std::regex_search(fields[3 + i], endpoint,
                                std::regex(R"(^\s*Port\s*(\d+)_)",
                                           std::regex::icase)) &&
              positiveInteger(endpoint[1].str()) != uint64_t(ports[i] + 1))
            throw Error("PowerSI hint endpoint does not match port index");
        }
        Channel c;
        c.name = base;
        c.id = "net:" + lower(base);
        c.nearP = ports[firstPositive ? 0 : 1];
        c.nearN = ports[firstPositive ? 1 : 0];
        c.farP = ports[firstPositive ? 2 : 3];
        c.farN = ports[firstPositive ? 3 : 2];
        candidates.push_back(c);
      }
      std::set<int> used;
      for (const auto &c : candidates)
        for (int p : {c.nearP, c.nearN, c.farP, c.farN})
          if (p >= 0) used.insert(p);
      std::map<std::string, std::vector<int>> remaining;
      for (size_t i = 0; i < m.labels.size(); ++i) {
        if (used.count(int(i))) continue;
        auto at = m.labels[i].find_last_of(" \t");
        if (at != std::string::npos)
          remaining[trim(m.labels[i].substr(at + 1))].push_back(int(i));
      }
      bool mixed = false;
      for (const auto &[net, ports] : remaining) {
        if (net.empty() || ports.size() != 2) continue;
        Channel c;
        c.id = "net:" + lower(net); c.name = net;
        c.nearP = ports[0]; c.farP = ports[1]; c.group = "Single-ended";
        candidates.push_back(c); mixed = true;
      }
      if (mixed)
        for (auto &c : candidates)
          if (c.differential()) c.group = "Differential";
      validateMapping(candidates, m);
      return candidates;
    } catch (const Error &e) {
      if (!diagnostic)
        throw;
      *diagnostic = std::string("PowerSI .DiffChannels could not be confirmed; compatibility label mapping was used. ") + e.what();
    }
  }
  // Unambiguous terminal net labels: DIE.pin.NET / BGA.pin.NET,
  // or PowerSI "component-pin NET". Every net needs exactly two endpoints.
  std::map<std::string, std::vector<int>> terminals;
  for (size_t i = 0; i < m.labels.size(); ++i) {
    const auto &label = m.labels[i];
    std::string net;
    auto space = label.find_last_of(" \t");
    if (space != std::string::npos)
      net = trim(label.substr(space + 1));
    else if (label.starts_with("DIE.") || label.starts_with("BGA.")) {
      auto dot = label.find('.', label.find('.') + 1);
      if (dot != std::string::npos)
        net = label.substr(dot + 1);
    }
    if (!net.empty())
      terminals[net].push_back(int(i));
  }
  std::vector<Channel> terminalCandidates;
  for (const auto &[net, ports] : terminals) {
    if (ports.size() != 2)
      continue;
    Channel c;
    c.id = "net:" + lower(net);
    c.name = net;
    c.nearP = ports[0];
    c.farP = ports[1];
    if (m.labels[size_t(c.nearP)].starts_with("BGA.") &&
        m.labels[size_t(c.farP)].starts_with("DIE."))
      std::swap(c.nearP, c.farP);
    terminalCandidates.push_back(c);
  }
  if (!terminalCandidates.empty()) {
    // Net labels such as DATA_P / DATA_N are useful evidence, but without an
    // explicit differential declaration they are suggestions only. Build a
    // differential-shaped candidate here; the GUI policy decides whether it
    // is authoritative or requires manual confirmation.
    auto namedPolarity = [](const std::string &name)
        -> std::optional<std::pair<std::string, bool>> {
      auto delimiter = [](char c) {
        return c == '_' || c == ' ' || c == '.' || c == ':' || c == '/' ||
               c == '-';
      };
      auto folded = lower(name);
      size_t polarity = std::string::npos;
      for (size_t i = 0; i < folded.size(); ++i) {
        if (folded[i] != 'p' && folded[i] != 'n') continue;
        const bool left = i == 0 || delimiter(folded[i - 1]);
        const bool right = i + 1 == folded.size() || delimiter(folded[i + 1]);
        if (!left || !right) continue;
        if (polarity != std::string::npos) return std::nullopt;
        polarity = i;
      }
      if (polarity == std::string::npos) return std::nullopt;
      std::string base = name;
      if (polarity + 1 < base.size() && delimiter(base[polarity + 1]))
        base.erase(polarity, 2);
      else if (polarity > 0 && delimiter(base[polarity - 1]))
        base.erase(polarity - 1, 2);
      else
        base.erase(polarity, 1);
      if (trim(base).empty()) return std::nullopt;
      return std::pair{trim(base), folded[polarity] == 'p'};
    };
    struct NamedPair {
      std::optional<Channel> positive, negative;
      std::string name;
      bool ambiguous = false;
    };
    std::map<std::string, NamedPair> namedPairs;
    for (const auto &candidate : terminalCandidates) {
      auto polarity = namedPolarity(candidate.name);
      if (!polarity) continue;
      auto &[base, positive] = *polarity;
      auto &pair = namedPairs[lower(base)];
      pair.name = base;
      auto &slot = positive ? pair.positive : pair.negative;
      if (slot) pair.ambiguous = true;
      else slot = candidate;
    }
    std::set<std::string> consumed;
    std::vector<Channel> combined;
    for (const auto &[key, pair] : namedPairs) {
      if (pair.ambiguous || !pair.positive || !pair.negative) continue;
      Channel c;
      c.name = pair.name;
      c.id = "namepair:" + key;
      c.group = "Differential";
      c.nearP = pair.positive->nearP;
      c.nearN = pair.negative->nearP;
      c.farP = pair.positive->farP;
      c.farN = pair.negative->farP;
      combined.push_back(c);
      consumed.insert(pair.positive->id);
      consumed.insert(pair.negative->id);
    }
    if (!combined.empty()) {
      for (auto candidate : terminalCandidates) {
        if (consumed.count(candidate.id)) continue;
        candidate.group = "Single-ended";
        combined.push_back(std::move(candidate));
      }
      validateMapping(combined, m);
      return combined;
    }
    validateMapping(terminalCandidates, m);
    return terminalCandidates;
  }
  // Only label-based near/far matches are proposed. Port numbers are not net
  // identity.
  std::map<std::string, Channel> found;
  std::set<std::string> ambiguous;
  std::regex rx(
      R"(^(.+?)[_ .:/-](near|far|in|out|tx|rx|a|b)(?:[_ .:/-]([pn+\-]))?$)",
      std::regex::icase);
  std::regex alt(
      R"(^(.+?)[_ .:/-]([pn+\-])[_ .:/-](near|far|in|out|tx|rx|a|b)$)",
      std::regex::icase);
  std::regex pairOnly(R"(^(.+?)[_ .:/-]([pn+\-])$)", std::regex::icase);
  for (size_t i = 0; i < m.labels.size(); ++i) {
    std::smatch s;
    std::string label = m.labels[i], base, end, pol;
    if (std::regex_match(label, s, rx)) {
      base = s[1];
      end = lower(s[2]);
      pol = lower(s[3]);
    } else if (std::regex_match(label, s, alt)) {
      base = s[1];
      pol = lower(s[2]);
      end = lower(s[3]);
    } else if (std::regex_match(label, s, pairOnly)) {
      base = s[1];
      pol = lower(s[2]);
      end = "near";
    } else
      continue;
    if (ambiguous.count(base))
      continue;
    auto &c = found[base];
    c.name = base;
    c.id = "label:" + lower(base);
    bool near = end == "near" || end == "in" || end == "tx" || end == "a";
    bool minus = pol == "n" || pol == "-";
    int &p = near ? (minus ? c.nearN : c.nearP) : (minus ? c.farN : c.farP);
    if (p != -1) {
      found.erase(base);
      ambiguous.insert(base);
      continue;
    }
    p = int(i);
  }
  std::vector<Channel> out;
  for (auto &[n, c] : found)
    if (c.nearP >= 0 && ((c.farP < 0 && c.farN < 0) ||
                         (c.farP >= 0 && ((c.nearN >= 0) == (c.farN >= 0)))))
      out.push_back(c);
  if (out.empty() && m.ports <= 2) {
    Channel c;
    c.name =
        m.labels.empty() || m.labels[0].empty() ? "Channel 1" : m.labels[0];
    c.id = "manual:" + c.name;
    c.nearP = 0;
    if (m.ports == 2)
      c.farP = 1;
    out.push_back(c);
  }
  return out;
}
static std::vector<std::string> csvRow(const std::string &line) {
  std::vector<std::string> out;
  std::string cell;
  bool quote = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '"') {
      if (quote && i + 1 < line.size() && line[i + 1] == '"') {
        cell += '"';
        ++i;
      } else
        quote = !quote;
    } else if (c == ',' && !quote) {
      out.push_back(trim(cell));
      cell.clear();
    } else
      cell += c;
  }
  if (quote)
    throw Error("Unclosed CSV quote (multiline fields not supported)");
  out.push_back(trim(cell));
  return out;
}
static std::string csvQuote(const std::string &s) {
  std::string out = "\"";
  for (char c : s) {
    out += c;
    if (c == '"')
      out += '"';
  }
  return out + '"';
}
std::vector<Channel> readMappingCsv(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    throw Error("Cannot open mapping CSV");
  std::string line;
  std::getline(in, line);
  if (line.starts_with("\xef\xbb\xbf"))
    line.erase(0, 3);
  auto headers = csvRow(line);
  std::map<std::string, size_t> columns;
  for (size_t i = 0; i < headers.size(); ++i)
    columns[lower(headers[i])] = i;
  for (auto key : {"channel", "nearp", "nearn", "farp", "farn"})
    if (!columns.count(key))
      throw Error("CSV header required: Channel,NearP,NearN,FarP,FarN");
  std::vector<Channel> out;
  while (std::getline(in, line)) {
    if (trim(line).empty())
      continue;
    auto row = csvRow(line);
    auto cell = [&](const std::string &key) {
      return columns.count(key) && columns[key] < row.size() ? row[columns[key]]
                                                             : std::string{};
    };
    auto port = [&](const std::string &key) {
      auto v = cell(key);
      return v.empty() ? -1 : static_cast<int>(positiveInteger(v)) - 1;
    };
    Channel c;
    c.name = cell("channel");
    c.id = cell("id");
    if (c.id.empty())
      c.id = "csv:" + lower(c.name);
    c.nearP = port("nearp");
    c.nearN = port("nearn");
    c.farP = port("farp");
    c.farN = port("farn");
    c.group = cell("group");
    if (c.group.empty())
      c.group = "Default";
    c.alias = cell("alias");
    out.push_back(c);
  }
  return out;
}
void writeMappingCsv(const fs::path &p, const std::vector<Channel> &cs) {
  std::ofstream out(p, std::ios::binary);
  if (!out)
    throw Error("Cannot create mapping CSV");
  out << "\xef\xbb\xbf" << "Channel,NearP,NearN,FarP,FarN,Group,Alias,ID\r\n";
  for (auto &c : cs) {
    out << csvQuote(c.name);
    for (auto i : {c.nearP, c.nearN, c.farP, c.farN})
      out << ',' << (i < 0 ? "" : std::to_string(i + 1));
    out << ',' << csvQuote(c.group) << ',' << csvQuote(c.alias) << ','
        << csvQuote(c.id) << "\r\n";
  }
  if (!out)
    throw Error("CSV write failed");
}
std::vector<int> matchChannels(const std::vector<Channel> &a,
                               const std::vector<Channel> &b) {
  std::vector<int> out(a.size(), -1);
  std::set<int> assigned;
  for (size_t i = 0; i < a.size(); ++i) {
    for (int stage = 0; stage < 3 && out[i] < 0; ++stage) {
      std::vector<int> hits;
      for (size_t j = 0; j < b.size(); ++j) {
        bool yes = stage == 0   ? a[i].id == b[j].id
                   : stage == 1 ? lower(a[i].name) == lower(b[j].name)
                                : (!a[i].alias.empty() &&
                                   (lower(a[i].alias) == lower(b[j].alias) ||
                                    lower(a[i].alias) == lower(b[j].name))) ||
                                      (!b[j].alias.empty() &&
                                       lower(b[j].alias) == lower(a[i].name));
        if (yes)
          hits.push_back(int(j));
      }
      if (hits.size() > 1)
        throw Error("Ambiguous revision channel match: " + a[i].name);
      if (hits.size() == 1) {
        if (!assigned.insert(hits[0]).second)
          throw Error("Revision match is not one-to-one");
        out[i] = hits[0];
      }
    }
  }
  return out;
}
std::string name(Metric m) {
  switch (m) {
  case Metric::RL:
    return "Return Loss";
  case Metric::IL:
    return "Insertion Loss";
  case Metric::NEXT:
    return "NEXT";
  case Metric::FEXT:
    return "FEXT";
  case Metric::TDR:
    return "TDR";
  }
  return {};
}
const Limit &Settings::limit(Metric m) const {
  switch (m) {
  case Metric::RL:
    return rl;
  case Metric::IL:
    return il;
  case Metric::NEXT:
    return next;
  case Metric::FEXT:
    return fext;
  default:
    return rl;
  }
}
void Limit::validate() const {
  if (!std::isfinite(constant))
    throw Error("Invalid constant limit");
  double last = -1;
  for (auto [f, v] : points) {
    if (!std::isfinite(f) || !std::isfinite(v) || f < 0 || f <= last)
      throw Error("Limit frequency points must be finite and increasing");
    last = f;
  }
  if (points.size() == 1)
    throw Error("Frequency limit needs at least two points");
}
double Limit::at(double f) const {
  if (!enabled)
    return NaN;
  if (points.empty())
    return constant;
  if (f < points.front().first || f > points.back().first)
    return NaN;
  auto it = std::lower_bound(points.begin(), points.end(), f,
                             [](auto p, double x) { return p.first < x; });
  if (it == points.begin() || it->first == f)
    return it->second;
  auto a = *(it - 1), b = *it;
  return a.second + (b.second - a.second) * (f - a.first) / (b.first - a.first);
}
static std::vector<std::pair<int, double>> endpoint(const Channel &c,
                                                    bool far) {
  int p = far ? c.farP : c.nearP, n = far ? c.farN : c.nearN;
  if (p < 0)
    throw Error("Far end is not mapped: " + c.name);
  if (n < 0)
    return {{p, 1}};
  return {{p, 1 / std::sqrt(2.)}, {n, -1 / std::sqrt(2.)}};
}

std::string name(Termination t, bool diff) {
  switch(t) {
  case Termination::Reference:return "Reference matched (all physical ports)";
  case Termination::Resistor:return diff?"P-N 100 ohm (floating)":"Signal-GND 50 ohm";
  case Termination::Split:return diff?"P-GND 50 ohm + N-GND 50 ohm":"Signal-GND 50 ohm";
  case Termination::Open:return diff?"P and N open":"Signal open";
  case Termination::Short:return diff?"P-N short (floating)":"Signal-GND short";
  } throw Error("Invalid termination");
}
std::string shortName(Termination t,bool diff) {
  switch(t) {
  case Termination::Reference:return "REF";
  case Termination::Resistor:return diff?"PN100":"50G";
  case Termination::Split:return diff?"GND50x2":"50G";
  case Termination::Open:return "OPEN";
  case Termination::Short:return "SHORT";
  } throw Error("Invalid termination");
}
std::vector<Job> expandTdrJobs(const std::vector<Job> &jobs,const Settings &s) {
  std::vector<Job> out;
  for(const auto &job:jobs) {
    if(job.metric!=Metric::TDR) {out.push_back(job);continue;}
    if(s.tdrTerminations.empty()) throw Error("Select at least one TDR termination");
    std::vector<Termination> seen;
    for(auto t:s.tdrTerminations) {
      (void)name(t,job.victim.differential());
      if(!job.victim.differential()&&t==Termination::Split)t=Termination::Resistor;
      if(std::find(seen.begin(),seen.end(),t)!=seen.end())continue;
      seen.push_back(t);auto copy=job;copy.termination=t;out.push_back(copy);
    }
  } return out;
}
// A singular floating mode is acceptable only when the physical load
// boundary is consistent AND the measured input response is unique.
static Complex loadFeedback(int n,Complex a[2][2],const Complex b[2],const Complex response[2]) {
  Complex m[2][3]{},original[2][2]{};double scale=1;
  for(int i=0;i<n;++i) {
    m[i][n]=b[i];
    for(int j=0;j<n;++j) {original[i][j]=m[i][j]=a[i][j];scale=std::max(scale,std::abs(a[i][j]));}
  }
  int pivots[2]{},rank=0;
  for(int col=0;col<n&&rank<n;++col) {
    int best=rank;
    for(int i=rank;i<n;++i)if(std::abs(m[i][col])>std::abs(m[best][col]))best=i;
    if(std::abs(m[best][col])<=1e-12*scale)continue;
    for(int j=0;j<=n;++j)std::swap(m[rank][j],m[best][j]);
    auto pivot=m[rank][col];for(int j=0;j<=n;++j)m[rank][j]/=pivot;
    for(int i=0;i<n;++i)if(i!=rank){auto f=m[i][col];for(int j=0;j<=n;++j)m[i][j]-=f*m[rank][j];}
    pivots[rank++]=col;
  }
  for(int i=rank;i<n;++i)if(std::abs(m[i][n])>1e-10)throw Error("Load boundary is singular/inconsistent");
  Complex x[2]{};
  for(int i=0;i<rank;++i)x[pivots[i]]=m[i][n];
  for(int col=0;col<n;++col) {
    bool pivot=false;for(int i=0;i<rank;++i)pivot|=pivots[i]==col;
    if(pivot)continue;
    Complex effect=response[col];
    for(int i=0;i<rank;++i)effect-=response[pivots[i]]*m[i][col];
    if(std::abs(effect)>1e-10)throw Error("Load boundary has no unique input response");
  }
  Complex out{};
  for(int i=0;i<n;++i) {
    Complex residual=-b[i];for(int j=0;j<n;++j)residual+=original[i][j]*x[j];
    if(std::abs(residual)>1e-8*(1+std::abs(b[i])))throw Error("Load boundary residual exceeds tolerance");
    out+=response[i]*x[i];
  }
  if(!std::isfinite(out.real())||!std::isfinite(out.imag()))throw Error("Non-finite loaded response");
  return out;
}
static void terminateTrace(Trace &t,const Cache &cache,const Channel &ch,const Settings &set,Termination term,Control *control) {
  (void)name(term,ch.differential());t.termination=term;
  if(term==Termination::Reference)return;
  auto driven=endpoint(ch,set.reverse),loaded=endpoint(ch,!set.reverse);
  const int n=int(loaded.size());
  if(ch.differential()&&n!=2)throw Error("Both opposite-end P/N ports must be mapped for differential loading");
  Complex gamma[2][2]{};double z[2]{};
  for(int i=0;i<n;++i)z[i]=cache.meta().reference.at(size_t(loaded[i].first));
  if(term==Termination::Open){for(int i=0;i<n;++i)gamma[i][i]=1;}
  else if(n==2&&(term==Termination::Resistor||term==Termination::Short)) {
    // Floating P-N resistor: Y = [1,-1]^T [1,-1] / R.
    double denom=(term==Termination::Resistor?100.:0.)+z[0]+z[1];
    double v[2]{std::sqrt(z[0]),-std::sqrt(z[1])};
    for(int i=0;i<2;++i)for(int j=0;j<2;++j)gamma[i][j]=(i==j?1.:0.)-2*v[i]*v[j]/denom;
  } else {
    double r=term==Termination::Short?0.:50.;
    for(int i=0;i<n;++i)gamma[i][i]=(r-z[i])/(r+z[i]);
  }
  std::vector<Complex> ll[2][2],ln[2],nl[2];
  for(int i=0;i<n;++i) {
    auto physical=std::vector<std::pair<int,double>>{{loaded[i].first,1}};
    ln[i]=cache.trace(physical,driven,control);nl[i]=cache.trace(driven,physical,control);
    for(int j=0;j<n;++j)ll[i][j]=cache.trace(physical,{{loaded[j].first,1}},control);
  }
  for(size_t k=0;k<t.s.size();++k) {
    if(k%128==0)check(control);
    Complex a[2][2]{},b[2]{},response[2]{};
    for(int i=0;i<n;++i) {
      response[i]=nl[i][k];
      for(int j=0;j<n;++j) {
        b[i]+=gamma[i][j]*ln[j][k];a[i][j]=(i==j?1.:0.);
        for(int q=0;q<n;++q)a[i][j]-=gamma[i][q]*ll[q][j][k];
      }
    }
    t.s[k]+=loadFeedback(n,a,b,response);
  }
  t.parameter+=" (loaded)";
}

static Trace computeTrace(const Cache &cache, const Channel &victim, Metric metric,
                const Settings &set, const Channel *aggressor,
                Control *control, Termination termination) {
  bool fromFar = set.reverse, toFar = set.reverse;
  const Channel *source = &victim;
  if (metric == Metric::IL)
    toFar = !fromFar;
  if (metric == Metric::NEXT || metric == Metric::FEXT) {
    if (!aggressor)
      throw Error("No aggressor in selected group");
    if (victim.differential() != aggressor->differential())
      throw Error(
          "SE/differential cross-mode coupling is outside this quick metric");
    source = aggressor;
    toFar = metric == Metric::NEXT ? fromFar : !fromFar;
  }
  Trace t;
  t.x = cache.frequencies(control);
  t.s =
      cache.trace(endpoint(victim, toFar), endpoint(*source, fromFar), control);
  int p = set.reverse ? source->farP : source->nearP;
  int n = set.reverse ? source->farN : source->nearN;
  t.referenceOhm =
      cache.meta().reference.at(static_cast<size_t>(p)) * (n >= 0 ? 2 : 1);
  t.responseReferenceOhm = cache.meta().reference.at(static_cast<size_t>(
                               toFar ? victim.farP : victim.nearP)) *
                           (victim.differential() ? 2 : 1);
  std::string prefix = victim.differential() ? "Sdd" : "S";
  if (metric == Metric::RL || metric == Metric::TDR)
    t.parameter = prefix + (set.reverse ? "22" : "11");
  else if (metric == Metric::IL)
    t.parameter = prefix + (set.reverse ? "12" : "21");
  else
    t.parameter = prefix + "[" + victim.name + ":" + (toFar ? "Far" : "Near") +
                  "," + source->name + ":" + (fromFar ? "Far" : "Near") + "]";
  if(metric==Metric::TDR)terminateTrace(t,cache,victim,set,termination,control);
  return t;
}
Trace loadTrace(const Cache &cache, const Channel &victim, Metric metric,
                const Settings &set, const Channel *aggressor,
                Control *control, Termination termination) {
  check(control);
  if (metric != Metric::TDR || cache.meta().sha256.empty())
    return computeTrace(cache,victim,metric,set,aggressor,control,termination);
  struct Entry { std::string key; std::shared_ptr<const Trace> data; size_t bytes; };
  struct Memo {
    std::mutex mutex; std::array<std::timed_mutex,32> flights;
    std::list<Entry> lru; size_t bytes=0;
  };
  static Memo memo;
  Sha256 hash;
  hash.add(cache.meta().sha256.data(),cache.meta().sha256.size());
  for (auto v : {victim.nearP,victim.nearN,victim.farP,victim.farN,int(set.reverse),int(termination)})
    hash.add(reinterpret_cast<const char *>(&v),sizeof(v));
  auto key=hash.finish();
  auto lookup=[&]() -> std::shared_ptr<const Trace> {
    std::lock_guard lock(memo.mutex);
    auto it=std::find_if(memo.lru.begin(),memo.lru.end(),[&](const auto &e){return e.key==key;});
    if(it==memo.lru.end()) return {};
    auto data=it->data;memo.lru.splice(memo.lru.begin(),memo.lru,it);return data;
  };
  if(auto hit=lookup()) {check(control);return *hit;}
  std::unique_lock flight(memo.flights[hashBytes(key.data(),key.size())%memo.flights.size()],std::defer_lock);
  while(!flight.try_lock_for(std::chrono::milliseconds(10)))check(control);
  check(control);
  if(auto hit=lookup())return *hit;
  auto trace=computeTrace(cache,victim,metric,set,aggressor,control,termination);
  check(control);
  size_t bytes=sizeof(Entry)+key.size()+sizeof(Trace)+trace.parameter.capacity()+
      trace.x.capacity()*sizeof(double)+trace.s.capacity()*sizeof(Complex);
  constexpr size_t budget=64*1024*1024;
  if(bytes<=budget) {
    auto data=std::make_shared<const Trace>(trace);
    std::lock_guard lock(memo.mutex);
    while(!memo.lru.empty() && (memo.bytes+bytes>budget || memo.lru.size()>=2048)) {
      memo.bytes-=memo.lru.back().bytes;memo.lru.pop_back();
    }
    memo.lru.push_front({key,std::move(data),bytes});memo.bytes+=bytes;
  }
  return trace;
}
DirectionDelta directionDelta(const Cache &cache, const Channel &channel, Metric metric,
                              const Settings &settings, Control *control) {
  if (metric != Metric::RL && metric != Metric::IL)
    throw Error("Directional delta is defined for RL and IL only");
  Settings forward = settings, reverse = settings;
  forward.reverse = false;
  reverse.reverse = true;
  auto a = crop(loadTrace(cache, channel, metric, forward, nullptr, control),
                settings.startHz, settings.stopHz);
  auto b = crop(loadTrace(cache, channel, metric, reverse, nullptr, control),
                settings.startHz, settings.stopHz);
  const double lo = std::max(a.x.front(), b.x.front());
  const double hi = std::min(a.x.back(), b.x.back());
  if (lo > hi) throw Error("No common frequency range for directional delta");
  std::vector<double> frequency;
  frequency.reserve(a.x.size() + b.x.size() + 2);
  for (double f : a.x) if (f >= lo && f <= hi) frequency.push_back(f);
  for (double f : b.x) if (f >= lo && f <= hi) frequency.push_back(f);
  frequency.push_back(lo); frequency.push_back(hi);
  std::sort(frequency.begin(), frequency.end());
  frequency.erase(std::unique(frequency.begin(), frequency.end()), frequency.end());
  DirectionDelta out;
  double maximum = -1;
  for (size_t i = 0; i < frequency.size(); ++i) {
    if ((i & 4095) == 0) check(control);
    const double f = frequency[i];
    const double x = logMagnitude(interpolate(a.x, a.s, f));
    const double y = logMagnitude(interpolate(b.x, b.s, f));
    const double delta = std::abs(x - y);
    if (std::isnan(delta)) continue;
    if (delta > maximum) { maximum = delta; out.maximumDb = delta; out.frequency = f; }
  }
  return out;
}
Complex interpolate(const std::vector<double> &f, const std::vector<Complex> &s,
                    double x) {
  if (f.empty() || f.size() != s.size() || x < f.front() || x > f.back())
    throw Error("Interpolation outside available frequency range");
  auto it = std::lower_bound(f.begin(), f.end(), x);
  size_t i = static_cast<size_t>(it - f.begin());
  if (i == 0 || *it == x)
    return s[i];
  double t = (x - f[i - 1]) / (f[i] - f[i - 1]);
  return s[i - 1] + t * (s[i] - s[i - 1]);
}
double logMagnitude(Complex z) {
  double a = std::abs(z);
  return a == 0 ? -std::numeric_limits<double>::infinity() : 20 * std::log10(a);
}
Trace crop(const Trace &t, double start, double stop) {
  if (t.x.empty() || t.x.size() != t.s.size())
    throw Error("Empty/invalid trace");
  if (!std::isfinite(start) || start < 0 || std::isnan(stop) || stop < start)
    throw Error("Invalid frequency range");
  start = std::max(start, t.x.front());
  stop = std::min(stop, t.x.back());
  if (start > stop)
    throw Error("No data in analysis range");
  Trace out;
  out.referenceOhm = t.referenceOhm;
  out.responseReferenceOhm = t.responseReferenceOhm;
  out.parameter = t.parameter;
  out.termination = t.termination;
  out.x.push_back(start);
  out.s.push_back(interpolate(t.x, t.s, start));
  auto a = std::upper_bound(t.x.begin(), t.x.end(), start),
       b = std::lower_bound(t.x.begin(), t.x.end(), stop);
  for (auto it = a; it < b; ++it) {
    auto i = it - t.x.begin();
    out.x.push_back(*it);
    out.s.push_back(t.s[static_cast<size_t>(i)]);
  }
  if (stop > start) {
    out.x.push_back(stop);
    out.s.push_back(interpolate(t.x, t.s, stop));
  }
  return out;
}
static void fft(std::vector<Complex> &a, Control *c) {
  size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    check(c);
    Complex wl = std::polar(1., 2 * pi / double(len));
    for (size_t i = 0; i < n; i += len) {
      Complex w = 1;
      for (size_t j = 0; j < len / 2; ++j) {
        Complex u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wl;
      }
    }
  }
  for (auto &v : a)
    v /= double(n);
}
static double bessel0(double x) {
  double sum = 1, term = 1;
  for (int k = 1; k < 64; ++k) {
    term *= x * x / (4 * k * k);
    sum += term;
    if (term < sum * 1e-16)
      break;
  }
  return sum;
}
// Nonuniform low-pass step fallback. Integrate the original frequency
// intervals instead of replacing a narrow DC response with one coarse FFT bin.
// rho(t) = Re(S(0))/2 + integral_0^F [Re(S)sin(2*pi*f*t)
//                                  + Im(S)cos(2*pi*f*t)] W(f)/(pi*f) df.
// A second quadrature order and a shape-preserving cubic sensitivity estimate
// bound numerical integration and warn about insufficient source sampling.
static Tdr nonuniformStep(const Trace &t, const Settings &set, Control *control,
                          double fftDf) {
  Tdr out;
  out.df = NaN; // Original intervals are nonuniform; there is no transform df.
  out.fmax = t.x.back();
  out.quality = "UNSUITABLE";
  auto fail = [&](const std::string &message) {
    out.time.clear();
    out.impedance.clear();
    out.reflection.clear();
    out.note = "Nonuniform step: " + message;
    return out;
  };
  const double fmax = t.x.back(), dt = 1 / (2 * fmax);
  const double lo = std::max(0., set.tdrStartSeconds);
  const double hi = std::min(.45 / fftDf, set.tdrStopSeconds);
  if (!std::isfinite(lo) || !std::isfinite(hi) || hi < lo)
    return fail("invalid time range");
  const size_t first = size_t(std::ceil(lo / dt));
  const size_t last = size_t(std::floor(hi / dt));
  if (last < first || last - first > 16384)
    return fail("time window exceeds direct-transform capacity; narrow the time range");
  double dc = t.s.front().real();
  if (t.x.front() > 0) {
    const double ratio = t.x[0] / t.x[1];
    dc = (t.s[0].real() - t.s[1].real() * ratio * ratio) /
         (1 - ratio * ratio);
    const double r2 = t.x[1] / t.x[2];
    const double alternate =
        (t.s[1].real() - t.s[2].real() * r2 * r2) / (1 - r2 * r2);
    if (std::abs(dc - alternate) > .01 || std::abs(t.s.front().imag()) > .2)
      return fail("low-frequency samples do not establish stable DC extrapolation");
  }
  if (!std::isfinite(dc) || std::abs(dc) > 1.01)
    return fail("DC extrapolation is outside passive reflection bounds");
  std::vector<double> f = t.x;
  std::vector<Complex> values = t.s;
  if (f.front() > 0) {
    f.insert(f.begin(), 0);
    values.insert(values.begin(), Complex(dc, 0));
  } else
    values.front() = {dc, 0};
  auto slopes = [&](bool imaginary) {
    std::vector<double> h(f.size() - 1), d(h.size()), slope(f.size());
    auto component = [&](size_t i) {
      return imaginary ? values[i].imag() : values[i].real();
    };
    for (size_t i = 0; i < h.size(); ++i) {
      h[i] = f[i + 1] - f[i];
      d[i] = (component(i + 1) - component(i)) / h[i];
    }
    for (size_t i = 1; i + 1 < f.size(); ++i)
      if (d[i - 1] * d[i] > 0) {
        double w1 = 2 * h[i] + h[i - 1], w2 = h[i] + 2 * h[i - 1];
        slope[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
      }
    auto edge = [](double h0, double h1, double d0, double d1) {
      double v = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
      if (v * d0 <= 0)
        return 0.;
      if (d0 * d1 < 0 && std::abs(v) > 3 * std::abs(d0))
        return 3 * d0;
      return v;
    };
    slope.front() = edge(h[0], h[1], d[0], d[1]);
    auto j = h.size() - 1;
    slope.back() = edge(h[j], h[j - 1], d[j], d[j - 1]);
    return slope;
  };
  const auto sr = slopes(false), si = slopes(true);
  static constexpr double x8[] = {-.9602898564975363, -.7966664774136267,
      -.5255324099163290, -.1834346424956498, .1834346424956498,
      .5255324099163290, .7966664774136267, .9602898564975363};
  static constexpr double w8[] = {.1012285362903763, .2223810344533745,
      .3137066458778873, .3626837833783620, .3626837833783620,
      .3137066458778873, .2223810344533745, .1012285362903763};
  static constexpr double x4[] = {-.8611363115940526, -.3399810435848563,
                                 .3399810435848563, .8611363115940526};
  static constexpr double w4[] = {.3478548451374539, .6521451548625461,
                                 .6521451548625461, .3478548451374539};
  struct Node {
    double omega, re, im, deltaRe, deltaIm;
    double sine = 0, cosine = 1, stepSine = 0, stepCosine = 1;
  };
  std::vector<Node> nodes8, nodes4;
  constexpr double pi = 3.14159265358979323846;
  const double windowNorm = bessel0(set.kaiserBeta);
  size_t pieces = 0;
  for (size_t i = 1; i < f.size(); ++i) {
    check(control);
    double h = f[i] - f[i - 1];
    double divisions = std::max(1., std::ceil(2 * hi * h));
    if (divisions > 100000 ||
        (pieces + divisions) * 12. * (last - first + 1) > 80000000.)
      return fail("work limit exceeded; narrow the time range or use a reference transform");
    size_t split = size_t(divisions);
    pieces += split;
    for (size_t part = 0; part < split; ++part) {
      double a = f[i - 1] + h * double(part) / double(split);
      double b = f[i - 1] + h * double(part + 1) / double(split);
      auto add = [&](auto &nodes, const double *xx, const double *ww, int order) {
        for (int q = 0; q < order; ++q) {
          double frequency = (a + b) / 2 + (b - a) / 2 * xx[q];
          double u = (frequency - f[i - 1]) / h;
          Complex linear = values[i - 1] * (1 - u) + values[i] * u;
          const Complex m0(sr[i - 1], si[i - 1]), m1(sr[i], si[i]);
          Complex cubic = (2*u*u*u - 3*u*u + 1)*values[i - 1] +
              (u*u*u - 2*u*u + u)*h*m0 + (-2*u*u*u + 3*u*u)*values[i] +
              (u*u*u - u*u)*h*m1;
          if (i == 1 && t.x.front() > 0) {
            // Hermitian symmetry: real part even, imaginary part odd at DC.
            linear = {dc + (values[1].real() - dc)*u*u, values[1].imag()*u};
            cubic = linear;
          }
          double v = frequency / fmax;
          double weight = (b - a) / 2 * ww[q] *
              bessel0(set.kaiserBeta * std::sqrt(std::max(0., 1 - v*v))) /
              windowNorm / (pi * frequency);
          auto delta = cubic - linear;
          nodes.push_back({2*pi*frequency, weight*linear.real(),
                           weight*linear.imag(), weight*delta.real(),
                           weight*delta.imag()});
        }
      };
      add(nodes8, x8, w8, 8);
      add(nodes4, x4, w4, 4);
    }
  }
  double quadratureError = 0, interpolationSensitivity = 0;
  bool singular = false;
  for (auto *nodes : {&nodes8, &nodes4})
    for (auto &v : *nodes) {
      v.stepSine = std::sin(v.omega * dt);
      v.stepCosine = std::cos(v.omega * dt);
    }
  for (size_t index = first; index <= last; ++index) {
    check(control);
    double time = double(index) * dt, fine = dc / 2, coarse = dc / 2, delta = 0;
    auto advance = [&](Node &v) {
      // Re-anchor every 128 samples to bound floating-point recurrence drift.
      if ((index - first) % 128 == 0) {
        v.sine = std::sin(v.omega * time);
        v.cosine = std::cos(v.omega * time);
      } else {
        double a = v.sine * v.stepCosine + v.cosine * v.stepSine;
        v.cosine = v.cosine * v.stepCosine - v.sine * v.stepSine;
        v.sine = a;
      }
    };
    for (auto &v : nodes8) {
      advance(v);
      double a = v.sine, b = v.cosine;
      fine += v.re * a + v.im * b;
      delta += v.deltaRe * a + v.deltaIm * b;
    }
    for (auto &v : nodes4) {
      advance(v);
      coarse += v.re * v.sine + v.im * v.cosine;
    }
    quadratureError = std::max(quadratureError, std::abs(fine - coarse));
    interpolationSensitivity = std::max(interpolationSensitivity, std::abs(delta));
    double z = NaN;
    if (std::abs(1 - fine) < 1e-8 || std::abs(fine) > 1.0001)
      singular = true;
    else
      z = t.referenceOhm * (1 + fine) / (1 - fine);
    out.time.push_back(time);
    out.impedance.push_back(z);
    out.reflection.push_back(fine);
  }
  // Preview numerical guards, not SI compliance or instrument accuracy limits.
  if (quadratureError > 1e-5)
    return fail("quadrature did not converge; shorten the time range");
  if (interpolationSensitivity > .001)
    return fail("source sampling is too sparse; interpolation sensitivity=" +
                std::to_string(interpolationSensitivity));
  out.quality = singular && t.termination == Termination::Reference ? "UNSUITABLE" : "LIMITED";
  out.note = "Nonuniform low-pass step; original frequency intervals; even-real DC "
             "extrapolation; Kaiser beta=" + std::to_string(set.kaiserBeta) +
             "; quadrature error=" + std::to_string(quadratureError) +
             "; interpolation sensitivity=" + std::to_string(interpolationSensitivity) +
             ". Preview; PowerSI/VNA correlation pending.";
  if (singular)
    out.note += " Impedance display singular/out of range; rho retained; invalid ohm samples omitted.";
  return out;
}
static Tdr computeTdr(const Trace &t, const Settings &set, Control *c) {
  Tdr out;
  auto unsuitable = [&](const std::string &s) {
    out.quality = "UNSUITABLE";
    out.note = s;
    return out;
  };
  if (t.x.size() < 8 || t.x.back() <= 0)
    return unsuitable("At least 8 frequency points are required");
  if (t.x.front() / t.x.back() > 0.1)
    return unsuitable("Low-frequency gap exceeds 10% of bandwidth");
  if (!(t.referenceOhm > 0) || !std::isfinite(t.referenceOhm))
    return unsuitable("Invalid source reference impedance");
  if (set.kaiserBeta < 0 || set.kaiserBeta > 13)
    return unsuitable("Window beta must be in [0,13]");
  double minDf = std::numeric_limits<double>::infinity(), maxDf = 0;
  for (size_t i = 1; i < t.x.size(); ++i) {
    double df = t.x[i] - t.x[i - 1];
    if (!(df > 0))
      return unsuitable("Frequency grid is not strictly increasing");
    minDf = std::min(minDf, df);
    maxDf = std::max(maxDf, df);
  }
  double nominal = (t.x.back() - t.x.front()) / double(t.x.size() - 1);
  size_t n = 16;
  double bins = std::ceil(t.x.back() / nominal);
  if (bins > 131072)
    return unsuitable("TDR transform needs more than 262144 time samples");
  while (n / 2 < bins)
    n *= 2;
  out.fmax = t.x.back();
  out.df = out.fmax / double(n / 2);
  double dt = 1 / (double(n) * out.df);
  std::vector<Complex> spec(n);
  Complex dc = t.s.front();
  if (t.x.front() > 0) {
    double re = t.s[0].real() + (t.s[0].real() - t.s[1].real()) * t.x.front() /
                                    (t.x[1] - t.x[0]);
    dc = {re, 0};
  } else
    dc = {dc.real(), 0};
  bool limited = t.x.front() > 0 || maxDf / minDf > 1.01 ||
                 std::abs(t.s.front().imag()) > 0.05;
  if (limited) {
    out.quality = "LIMITED";
    out.note = "DC extrapolation and/or complex grid resampling used. ";
  }
  out.note += "Preview Kaiser beta=" + std::to_string(set.kaiserBeta) +
              "; reference-tool release validation pending.";
  if (std::abs(dc) > 1.01)
    return unsuitable("DC extrapolation is outside passive reflection bounds");
  // Bound the loss introduced by the uniform Quick-TDR grid. A narrow
  // low-frequency pole (for example AC coupling) can otherwise alias into a
  // large DC offset despite thousands of source frequency samples.
  std::vector<Complex> uniform(n / 2 + 1);
  for (size_t k = 0; k <= n / 2; ++k) {
    const double frequency = out.df * double(k);
    uniform[k] = frequency < t.x.front()
                     ? dc + (t.s.front() - dc) * (frequency / t.x.front())
                     : interpolate(t.x, t.s, std::min(frequency, t.x.back()));
  }
  double gridError = 0;
  for (size_t i = 0; i < t.x.size(); ++i) {
    if (i % 4096 == 0)
      check(c);
    const double position = t.x[i] / out.df;
    const auto k = std::min(n / 2 - 1, size_t(position));
    const double alpha = std::clamp(position - double(k), 0.0, 1.0);
    gridError = std::max(
        gridError,
        std::abs(t.s[i] - (uniform[k] * (1 - alpha) + uniform[k + 1] * alpha)));
  }
  // This 0.05 absolute-complex-error guard is a documented preview heuristic,
  // not an SI compliance limit or a reference-tool accuracy specification.
  if (gridError > 0.05)
    return nonuniformStep(t, set, c, out.df);
  double denominator = bessel0(set.kaiserBeta);
  for (size_t k = 0; k <= n / 2; ++k) {
    if (k % 4096 == 0)
      check(c);
    Complex z = uniform[k];
    double u = double(k) / double(n / 2);
    z *= bessel0(set.kaiserBeta * std::sqrt(std::max(0., 1 - u * u))) /
         denominator;
    spec[k] = z;
    if (k > 0 && k < n / 2)
      spec[n - k] = std::conj(z);
  }
  spec[0] = {spec[0].real(), 0};
  spec[n / 2] = {spec[n / 2].real(), 0};
  fft(spec, c);
  // fftshift then cumulative trapezoid (same time origin convention as
  // scikit-rf).
  double cumulative = 0, previous = spec[n / 2].real();
  std::vector<double> time(n), z(n), rho(n);
  bool singular = false;
  for (size_t i = 0; i < n; ++i) {
    double impulse = spec[(i + n / 2) % n].real();
    if (i)
      cumulative += 0.5 * (previous + impulse);
    previous = impulse;
    time[i] = (double(i) - double(n / 2)) * dt;
    rho[i] = cumulative;
    double d = 1 - cumulative;
    if (std::abs(d) < 1e-8 || std::abs(cumulative) > 1.0001) {
      z[i] = NaN;
      if (time[i] >= 0)
        singular = true;
    } else
      z[i] = t.referenceOhm * (1 + cumulative) / d;
  }
  double lo = std::max(0., set.tdrStartSeconds),
         hi = std::min(0.45 / out.df, set.tdrStopSeconds);
  if (!std::isfinite(lo) || std::isnan(hi) || hi < lo)
    return unsuitable("Invalid TDR time range");
  for (size_t i = 0; i < n; ++i)
    if (time[i] >= lo && time[i] <= hi) {
      out.time.push_back(time[i]);
      out.impedance.push_back(z[i]);
      out.reflection.push_back(rho[i]);
    }
  if (out.time.empty())
    return unsuitable("No TDR data inside time range");
  if (singular) {
    out.quality = t.termination == Termination::Reference ? "UNSUITABLE" : "LIMITED";
    out.note += " Impedance display singular/out of range; rho retained; invalid ohm samples omitted.";
  }
  return out;
}
namespace {
struct TdrMemo {
  struct Entry { std::string key; std::shared_ptr<const Tdr> data; size_t bytes; };
  std::mutex mutex;
  std::array<std::timed_mutex, 32> flights;
  std::list<Entry> lru;
  size_t bytes = 0, hits = 0, misses = 0;
  static constexpr size_t budget = 128 * 1024 * 1024;
};
TdrMemo &tdrMemo() { static TdrMemo memo; return memo; }
}
TdrCacheStats tdrCacheStats() {
  auto &memo = tdrMemo(); std::lock_guard lock(memo.mutex);
  return {memo.hits, memo.misses, memo.bytes, memo.lru.size()};
}
void clearTdrCache() {
  auto &memo = tdrMemo(); std::lock_guard lock(memo.mutex);
  memo.lru.clear(); memo.bytes = memo.hits = memo.misses = 0;
}
Tdr transformTdr(const Trace &t, const Settings &set, Control *c) {
  check(c);
  if (t.x.size() != t.s.size()) throw Error("TDR frequency/value size mismatch");
  Sha256 hash;
  auto add = [&](const auto &v) {
    hash.add(reinterpret_cast<const char *>(&v), sizeof(v));
  };
  add(t.referenceOhm); add(t.termination);
  add(set.tdrStartSeconds); add(set.tdrStopSeconds); add(set.kaiserBeta);
  const size_t count = t.x.size(); add(count);
  // Hash all input bytes; no path-only keys or revision-name assumptions.
  for (size_t offset = 0; offset < count; offset += 4096) {
    check(c); size_t n = std::min<size_t>(4096, count - offset);
    hash.add(reinterpret_cast<const char *>(t.x.data() + offset), n * sizeof(double));
    hash.add(reinterpret_cast<const char *>(t.s.data() + offset), n * sizeof(Complex));
  }
  const auto key = hash.finish(); auto &memo = tdrMemo();
  auto lookup = [&]() -> std::shared_ptr<const Tdr> {
    std::lock_guard lock(memo.mutex);
    auto it = std::find_if(memo.lru.begin(), memo.lru.end(),
                           [&](const auto &e) { return e.key == key; });
    if (it == memo.lru.end()) return {};
    auto data = it->data; memo.lru.splice(memo.lru.begin(), memo.lru, it);
    ++memo.hits; return data;
  };
  if (auto hit = lookup()) { check(c); return *hit; }
  std::unique_lock flight(memo.flights[hashBytes(key.data(),key.size()) % memo.flights.size()], std::defer_lock);
  while (!flight.try_lock_for(std::chrono::milliseconds(10))) check(c);
  check(c);
  if (auto hit = lookup()) return *hit;
  auto result = computeTdr(t, set, c); check(c);
  const size_t bytes = sizeof(TdrMemo::Entry) + key.size() + sizeof(Tdr) +
      (result.time.capacity() + result.impedance.capacity() + result.reflection.capacity()) * sizeof(double) +
      result.quality.capacity() + result.note.capacity();
  auto stored = std::make_shared<const Tdr>(result);
  { std::lock_guard lock(memo.mutex);
    ++memo.misses;
    if (bytes <= TdrMemo::budget) {
      while (!memo.lru.empty() && (memo.bytes + bytes > TdrMemo::budget || memo.lru.size() >= 2048)) {
        memo.bytes -= memo.lru.back().bytes; memo.lru.pop_back();
      }
      memo.lru.push_front({key, std::move(stored), bytes}); memo.bytes += bytes;
    }
  }
  return result;
}
std::vector<size_t> envelope(const std::vector<double> &x,
                             const std::vector<double> &y, size_t buckets,
                             double lo, double hi) {
  std::vector<size_t> out;
  if (x.empty() || x.size() != y.size() || hi < lo)
    return out;
  buckets = std::max<size_t>(1, buckets);
  auto first = static_cast<size_t>(std::lower_bound(x.begin(), x.end(), lo) -
                                   x.begin()),
       last = static_cast<size_t>(std::upper_bound(x.begin(), x.end(), hi) -
                                  x.begin());
  if (first == last)
    return out;
  if (last - first <= buckets * 2) {
    for (size_t i = first; i < last; ++i)
      out.push_back(i);
    return out;
  }
  out.push_back(first);
  size_t i = first;
  while (i < last) {
    size_t begin = i;
    size_t bin =
        hi == lo ? 0
                 : std::min(buckets - 1,
                            size_t((x[i] - lo) / (hi - lo) * double(buckets)));
    size_t mn = i, mx = i;
    while (i < last) {
      size_t b = hi == lo
                     ? 0
                     : std::min(buckets - 1, size_t((x[i] - lo) / (hi - lo) *
                                                    double(buckets)));
      if (b != bin)
        break;
      if (y[i] < y[mn])
        mn = i;
      if (y[i] > y[mx])
        mx = i;
      ++i;
    }
    if (mn > mx)
      std::swap(mn, mx);
    out.push_back(mn);
    if (mx != mn)
      out.push_back(mx);
    if (i == begin)
      ++i;
  }
  out.push_back(last - 1);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
static double severity(Metric metric, double value, double target) {
  return metric == Metric::IL    ? -value
         : metric == Metric::TDR ? std::abs(value - target)
                                 : value;
}
static std::vector<Peak> detectPeaks(const std::vector<double> &x,
                                     const std::vector<double> &y, Metric m,
                                     double target, int count,
                                     double prominence, size_t global) {
  std::vector<Peak> peaks;
  if (count <= 0 || x.empty())
    return peaks;
  peaks.push_back({x[global], y[global], true});
  if (count == 1)
    return peaks;
  std::vector<size_t> candidates;
  size_t radius = std::max<size_t>(2, y.size() / 250),
         separation = std::max<size_t>(1, y.size() / 40);
  for (size_t i = 1; i + 1 < y.size(); ++i) {
    double v = severity(m, y[i], target);
    if (!std::isfinite(v) || v <= severity(m, y[i - 1], target) ||
        v < severity(m, y[i + 1], target))
      continue;
    double left = v, right = v;
    for (size_t j = i > radius ? i - radius : 0; j < i; ++j)
      left = std::min(left, severity(m, y[j], target));
    for (size_t j = i + 1; j < std::min(y.size(), i + radius + 1); ++j)
      right = std::min(right, severity(m, y[j], target));
    if (v - std::max(left, right) >= prominence)
      candidates.push_back(i);
  }
  std::sort(candidates.begin(), candidates.end(), [&](size_t a, size_t b) {
    return severity(m, y[a], target) > severity(m, y[b], target);
  });
  std::vector<size_t> selected{global};
  for (size_t i : candidates) {
    bool far = true;
    for (size_t j : selected)
      if (std::max(i, j) - std::min(i, j) < separation)
        far = false;
    if (far) {
      selected.push_back(i);
      peaks.push_back({x[i], y[i], false});
      if (int(peaks.size()) >= count)
        break;
    }
  }
  return peaks;
}
std::string metricDirection(Metric metric, bool reverse) {
  const std::string from = reverse ? "Far" : "Near";
  const std::string opposite = reverse ? "Near" : "Far";
  if (metric == Metric::NEXT || metric == Metric::FEXT)
    return "Aggressor " + from + " -> Victim " +
           (metric == Metric::NEXT ? from : opposite);
  return from + " -> " + (metric == Metric::IL ? opposite : from);
}
Result analyze(const Trace &input, Metric metric, const Settings &set,
               Control *control) {
  if (input.x.empty() || input.x.size() != input.s.size())
    throw Error("Empty or inconsistent analysis trace");
  Result r;
  r.metric = metric;
  r.termination = input.termination;
  r.reflection = metric == Metric::TDR && set.tdrReflection;
  r.parameter = input.parameter;
  r.referenceOhm = input.referenceOhm;
  r.direction = metricDirection(metric, set.reverse);
  auto appendNote = [&](const std::string &text) {
    if (text.empty())
      return;
    if (!r.note.empty() && r.note.back() != ' ')
      r.note += ' ';
    r.note += text;
  };
  auto valueText = [](double value) {
    std::ostringstream out;
    out << std::setprecision(6) << std::scientific << value;
    return out.str();
  };
  auto rangeTolerance = [](double value) {
    return std::max(1.0, std::abs(value)) * 1e-12;
  };
  bool coverageIncomplete = false;
  std::vector<std::string> coverageIssues;
  auto coverageIssue = [&](const std::string &text) {
    coverageIncomplete = true;
    coverageIssues.push_back(text);
  };

  std::vector<double> x, y;
  Trace t;
  const Limit &limit = set.limit(metric);
  limit.validate();
  if (metric == Metric::TDR) {
    if (set.tolerancePercent < 0 || !std::isfinite(set.tolerancePercent))
      throw Error("Invalid TDR tolerance");
    if (std::isfinite(set.stopHz) &&
        input.x.back() + rangeTolerance(set.stopHz) < set.stopHz)
      coverageIssue("frequency stop " + valueText(set.stopHz) +
                    " Hz requested, source ends at " +
                    valueText(input.x.back()) + " Hz");
    t = crop(input, input.x.front(), set.stopHz);
    auto td = transformTdr(t, set, control);
    x = std::move(td.time);
    y = r.reflection ? std::move(td.reflection) : std::move(td.impedance);
    r.quality = td.quality;
    r.note = td.note + " Load: " +
             name(input.termination, input.parameter.starts_with("Sdd")) +
             "; opposite end " + (set.reverse ? "Near" : "Far") +
             "; all other physical ports reference matched.";
    r.transformDf = td.df;
    r.transformFmax = td.fmax;
    r.targetOhm = r.reflection ? 0
                  : set.targetOhm > 0 ? set.targetOhm
                  : input.parameter.starts_with("Sdd") ? 100
                                                       : 50;
    if (!x.empty()) {
      const double step = x.size() > 1 ? std::abs(x[1] - x[0]) : 0;
      const double timeTolerance = std::max(1e-15, step * 1.01);
      if (set.tdrStartSeconds > 0 &&
          x.front() > set.tdrStartSeconds + timeTolerance)
        coverageIssue("TDR start " + valueText(set.tdrStartSeconds) +
                      " s requested, first evaluated sample is " +
                      valueText(x.front()) + " s");
      if (std::isfinite(set.tdrStopSeconds) &&
          x.back() + timeTolerance < set.tdrStopSeconds)
        coverageIssue("TDR stop " + valueText(set.tdrStopSeconds) +
                      " s requested, computed window ends at " +
                      valueText(x.back()) + " s");
    }
  } else {
    // startHz == 0 is the UI/default policy for "from the first available
    // source sample". A positive start is an explicit coverage request.
    if (set.startHz > 0 &&
        input.x.front() > set.startHz + rangeTolerance(set.startHz))
      coverageIssue("frequency start " + valueText(set.startHz) +
                    " Hz requested, source starts at " +
                    valueText(input.x.front()) + " Hz");
    if (std::isfinite(set.stopHz) &&
        input.x.back() + rangeTolerance(set.stopHz) < set.stopHz)
      coverageIssue("frequency stop " + valueText(set.stopHz) +
                    " Hz requested, source ends at " +
                    valueText(input.x.back()) + " Hz");

    t = crop(input, set.startHz, set.stopHz);

    // Equivalent representations of the same limit must not change the
    // response evaluation grid. Preserve the first/last limit boundaries and
    // genuine piecewise-linear kinks; ignore redundant collinear interior
    // points. Any added response value is still explicitly an interpolation.
    std::vector<double> limitChecks;
    if (limit.enabled && !limit.points.empty()) {
      limitChecks.push_back(limit.points.front().first);
      if (limit.points.size() > 2) {
        for (size_t i = 1; i + 1 < limit.points.size(); ++i) {
          const auto a = limit.points[i - 1];
          const auto b = limit.points[i];
          const auto c = limit.points[i + 1];
          const double alpha = (b.first - a.first) / (c.first - a.first);
          const double expected = a.second + (c.second - a.second) * alpha;
          const double tolerance = 1e-12 * std::max(
              {1.0, std::abs(a.second), std::abs(b.second), std::abs(c.second)});
          if (std::abs(b.second - expected) > tolerance)
            limitChecks.push_back(b.first);
        }
      }
      if (limit.points.size() > 1)
        limitChecks.push_back(limit.points.back().first);
    }
    std::sort(limitChecks.begin(), limitChecks.end());
    limitChecks.erase(std::unique(limitChecks.begin(), limitChecks.end()),
                      limitChecks.end());
    size_t insertedLimitChecks = 0;
    for (double f : limitChecks) {
      if (f > t.x.front() && f < t.x.back() &&
          !std::binary_search(t.x.begin(), t.x.end(), f)) {
        auto z = interpolate(t.x, t.s, f);
        auto at = std::lower_bound(t.x.begin(), t.x.end(), f) - t.x.begin();
        t.x.insert(t.x.begin() + at, f);
        t.s.insert(t.s.begin() + at, z);
        ++insertedLimitChecks;
      }
    }
    if (insertedLimitChecks)
      appendNote("Frequency-dependent limit boundary/kink checks at " +
                 std::to_string(insertedLimitChecks) +
                 " non-source point(s) use complex interpolation; redundant "
                 "collinear limit points do not alter the evaluation grid.");
    x = t.x;
    y.reserve(t.s.size());
    for (auto z : t.s)
      y.push_back(logMagnitude(z));
  }
  if (x.empty()) {
    r.note = r.note.empty() ? "No analysis data" : r.note;
    return r;
  }
  r.start = x.front();
  r.stop = x.back();
  size_t worst = 0;
  bool any = false, unknown = false, anyMargin = false, violation = false;
  double worstScore = -std::numeric_limits<double>::infinity();
  r.minimum = std::numeric_limits<double>::infinity();
  r.maximum = -std::numeric_limits<double>::infinity();
  double minMargin = std::numeric_limits<double>::infinity();
  bool hasLimit = metric == Metric::TDR
                      ? set.tdrLimitEnabled && !r.reflection
                      : limit.enabled;
  for (size_t i = 0; i < y.size(); ++i) {
    if (i % 8192 == 0)
      check(control);
    if (std::isnan(y[i])) {
      unknown = true;
      continue;
    }
    double score = severity(metric, y[i], r.targetOhm);
    if (!any || score > worstScore) {
      worstScore = score;
      worst = i;
      any = true;
    }
    r.minimum = std::min(r.minimum, y[i]);
    r.maximum = std::max(r.maximum, y[i]);
    if (hasLimit) {
      double l = metric == Metric::TDR
                     ? r.targetOhm * set.tolerancePercent / 100
                     : limit.at(x[i]);
      double margin = metric == Metric::TDR
                          ? l - std::abs(y[i] - r.targetOhm)
                      : metric == Metric::IL ? y[i] - l
                                             : l - y[i];
      if (std::isnan(margin)) {
        unknown = true;
        continue;
      }
      anyMargin = true;
      if (margin < 0)
        violation = true;
      if (margin < minMargin) {
        minMargin = margin;
        r.marginX = x[i];
        r.limitAtMargin = l;
      }
    }
  }
  if (any) {
    r.worst = y[worst];
    r.worstX = x[worst];
    r.peaks = detectPeaks(x, y, metric, r.targetOhm, set.markers,
                          set.prominence, worst);
  } else {
    r.note += " No finite impedance samples.";
    return r;
  }
  if (metric == Metric::TDR && !r.reflection) {
    r.minErrorPercent = 100 * (r.minimum / r.targetOhm - 1);
    r.maxErrorPercent = 100 * (r.maximum / r.targetOhm - 1);
    if (r.quality == "UNSUITABLE")
      unknown = true;
  }
  if (hasLimit) {
    r.margin = anyMargin ? minMargin : NaN;
    if (violation)
      r.status = "NG";
    else if (unknown || coverageIncomplete || !anyMargin)
      r.status = "N/A";
    else
      r.status = "OK";

    if (unknown || !anyMargin) {
      if (violation)
        appendNote("Some limit samples are unavailable or invalid, but an "
                   "evaluated violation exists; verdict remains NG.");
      else {
        r.margin = NaN;
        appendNote("Incomplete limit coverage or invalid data: verdict N/A.");
      }
    }
  }

  if (coverageIncomplete) {
    std::ostringstream note;
    note << "Requested range incomplete: ";
    for (size_t i = 0; i < coverageIssues.size(); ++i) {
      if (i)
        note << "; ";
      note << coverageIssues[i];
    }
    note << ". Evaluated " << valueText(r.start) << " to " << valueText(r.stop)
         << (metric == Metric::TDR ? " s" : " Hz") << ".";
    if (hasLimit)
      note << (violation
                   ? " An evaluated limit violation was found, so verdict remains NG."
                   : " The evaluated portion did not fail, but the overall verdict is N/A.");
    appendNote(note.str());
  }

  if (metric != Metric::TDR) {
    bool interpolatedManualMarker = false;
    for (double f : set.manualMarkersHz)
      if (f >= x.front() && f <= x.back()) {
        auto z = interpolate(t.x, t.s, f);
        r.peaks.push_back(
            {f, logMagnitude(z), false, true, std::arg(z) * 180 / pi});
        if (!std::binary_search(input.x.begin(), input.x.end(), f))
          interpolatedManualMarker = true;
      }
    if (interpolatedManualMarker)
      appendNote("Manual marker values at non-source frequencies use complex "
                 "interpolation for display only and do not change pass/fail.");
  }

  // Keep the full-range preview compact. TDR detail is reconstructed only for
  // the visible viewport by the UI; analysis statistics above still use every
  // full-resolution sample.
  auto plot = envelope(x, y, 2048, x.front(), x.back());
  r.plotComplete = plot.size() == x.size();
  for (size_t i : plot) {
    r.plotX.push_back(x[i]);
    r.plotY.push_back(y[i]);
  }
  if (metric != Metric::TDR) {
    size_t bins = std::min<size_t>(512, x.size());
    r.heatRaw.assign(bins, metric == Metric::IL
                               ? std::numeric_limits<double>::infinity()
                               : -std::numeric_limits<double>::infinity());
    r.heatMargin.assign(bins, NaN);
    r.heatX.resize(bins);
    std::vector<bool> seen(bins);
    for (size_t i = 0; i < bins; ++i)
      r.heatX[i] =
          x.front() + (x.back() - x.front()) * (double(i) + 0.5) / double(bins);
    for (size_t i = 0; i < x.size(); ++i) {
      size_t b = x.back() == x.front()
                     ? 0
                     : std::min(bins - 1,
                                size_t((x[i] - x.front()) /
                                       (x.back() - x.front()) * double(bins)));
      r.heatRaw[b] = metric == Metric::IL ? std::min(r.heatRaw[b], y[i])
                                          : std::max(r.heatRaw[b], y[i]);
      double l = limit.at(x[i]);
      double ma = metric == Metric::IL ? y[i] - l : l - y[i];
      if (!std::isnan(ma))
        r.heatMargin[b] =
            std::isnan(r.heatMargin[b]) ? ma : std::min(r.heatMargin[b], ma);
      seen[b] = true;
    }
    for (size_t i = 0; i < bins; ++i)
      if (!seen[i])
        r.heatRaw[i] = NaN;
  }
  return r;
}
size_t workerLimit(int performance, size_t jobs, unsigned hardwareThreads) {
  if (!jobs)
    return 0;
  const size_t hardware = std::max(1u, hardwareThreads);
  const size_t requested = performance == 0 ? 1 : performance == 1 ? 2 : 8;
  return std::min({requested, hardware, jobs});
}
std::vector<Result> runJobs(const std::vector<Job> &jobs,
                            const Settings &settings, Control *control,
                            const WorkerRunner &runner) {
  check(control);
  if (jobs.empty())
    return {};
  std::vector<Result> out(jobs.size());
  std::atomic_size_t next{0}, done{0};
  std::mutex progressMutex;
  size_t threads = workerLimit(settings.performance, jobs.size(),
                               std::thread::hardware_concurrency());
  auto work = [&]() {
    while (true) {
      size_t i = next.fetch_add(1);
      if (i >= jobs.size())
        return;
      if (control && control->cancelled)
        return;
      auto &j = jobs[i];
      Settings jobSettings = settings;
      if (j.reverse) jobSettings.reverse = *j.reverse;
      Result r;
      try {
        auto t = loadTrace(*j.cache, j.victim, j.metric, jobSettings,
                           j.aggressor ? &*j.aggressor : nullptr, control, j.termination);
        r = analyze(t, j.metric, jobSettings, control);
      } catch (const Cancelled &) {
        return;
      } catch (const std::exception &e) {
        r.metric = j.metric;
        r.status = "N/A";
        r.note = e.what();
        r.termination = j.termination;
        r.reflection = j.metric == Metric::TDR && jobSettings.tdrReflection;
        if(j.metric==Metric::TDR) {
          r.quality="UNSUITABLE";
          r.parameter=std::string(j.victim.differential()?"Sdd":"S")+(jobSettings.reverse?"22":"11");
        }
      }
      r.channel = j.victim.name;
      r.channelId = j.victim.id;
      r.group = j.victim.group;
      r.revision = j.revision;
      r.reverse = jobSettings.reverse;
      r.direction = metricDirection(j.metric, jobSettings.reverse);
      if (j.aggressor) {
        r.aggressor = j.aggressor->name;
        r.aggressorId = j.aggressor->id;
      }
      if (jobs.size() > 128) {
        // Retain an immediate preview under a shared ~32 MiB point budget.
        size_t buckets = std::clamp<size_t>((32 * 1024 * 1024 / jobs.size()) / (4 * sizeof(double)), 8, 128);
        auto indices = envelope(r.plotX, r.plotY, buckets, r.start, r.stop);
        std::vector<double> x, y;
        for (auto k : indices) { x.push_back(r.plotX[k]); y.push_back(r.plotY[k]); }
        r.plotComplete = r.plotComplete && indices.size() == r.plotX.size();
        r.plotX = std::move(x); r.plotY = std::move(y);
      }
      if (jobs.size() > 512) {
        r.heatX.clear();
        r.heatRaw.clear();
        r.heatMargin.clear();
      }
      out[i] = std::move(r);
      {
        std::lock_guard lock(progressMutex);
        ++done;
        if (control && control->progress && !control->cancelled)
          control->progress(double(done) / std::max<size_t>(1, jobs.size()),
                            "Analyzing " + std::to_string(done) + " / " +
                                std::to_string(jobs.size()));
      }
      if (settings.performance == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  };
  std::exception_ptr failure;
  std::mutex failureMutex;
  auto guarded = [&] {
    try {
      work();
    } catch (...) {
      std::lock_guard lock(failureMutex);
      if (!failure)
        failure = std::current_exception();
    }
  };
  if (runner)
    runner(threads, guarded);
  else {
    std::vector<std::jthread> pool;
    for (size_t i = 0; i < threads; ++i)
      pool.emplace_back(guarded);
  }
  if (failure)
    std::rethrow_exception(failure);
  check(control);
  return out;
}
void rankResults(std::vector<Result> &r, const std::string &by) {
  std::stable_sort(r.begin(), r.end(), [&](const auto &a, const auto &b) {
    if (a.metric != b.metric)
      return a.metric < b.metric;
    if (by == "Channel")
      return a.channel < b.channel;
    if (by == "Result") {
      auto priority = [](auto s) { return s == "NG" ? 0 : s == "N/A" ? 1 : 2; };
      if (a.status != b.status)
        return priority(a.status) < priority(b.status);
    }
    if (by == "Margin" && !std::isnan(a.margin) && !std::isnan(b.margin) &&
        a.margin != b.margin)
      return a.margin < b.margin;
    auto va = severity(a.metric, a.worst, a.targetOhm),
         vb = severity(b.metric, b.worst, b.targetOhm);
    if (std::isnan(va))
      return false;
    if (std::isnan(vb))
      return true;
    return va > vb;
  });
}
Delta compare(const Trace &a, const Trace &b, Metric m) {
  if (m == Metric::TDR)
    throw Error("TDR frequency delta is not supported");
  if (std::abs(a.responseReferenceOhm - b.responseReferenceOhm) >
      1e-9 * std::max(a.responseReferenceOhm, b.responseReferenceOhm))
    throw Error(
        "Different response reference impedances: delta needs renormalization");
  if (std::abs(a.referenceOhm - b.referenceOhm) >
      1e-9 * std::max(a.referenceOhm, b.referenceOhm))
    throw Error("Different reference impedances: delta needs renormalization");
  double lo = std::max(a.x.front(), b.x.front()),
         hi = std::min(a.x.back(), b.x.back());
  if (lo > hi)
    throw Error("No common revision frequency range");
  Delta d;
  for (double f : a.x)
    if (f >= lo && f <= hi)
      d.frequency.push_back(f);
  for (double f : b.x)
    if (f >= lo && f <= hi)
      d.frequency.push_back(f);
  d.frequency.push_back(lo);
  d.frequency.push_back(hi);
  std::sort(d.frequency.begin(), d.frequency.end());
  d.frequency.erase(std::unique(d.frequency.begin(), d.frequency.end()),
                    d.frequency.end());
  d.worstImprovement = std::numeric_limits<double>::infinity();
  for (double f : d.frequency) {
    double aa = logMagnitude(interpolate(a.x, a.s, f)),
           bb = logMagnitude(interpolate(b.x, b.s, f));
    double raw = bb - aa;
    d.rawDb.push_back(raw);
    double imp = m == Metric::IL ? raw : -raw;
    d.improvement.push_back(imp);
    if (!std::isnan(imp))
      d.worstImprovement = std::min(d.worstImprovement, imp);
  }
  return d;
}
std::string overall(const std::vector<std::string> &statuses) {
  if (std::find(statuses.begin(), statuses.end(), "NG") != statuses.end())
    return "NG";
  if (statuses.empty() ||
      std::find(statuses.begin(), statuses.end(), "N/A") != statuses.end())
    return "N/A";
  return "OK";
}
void exportRawCsv(const fs::path &p, const Trace &t, Metric m,
                  const Settings &settings, Control *c) {
  auto tmpPath = p;
  tmpPath += ".tmp";
  Temporary tmp{tmpPath};
  std::ofstream out(tmp.path, std::ios::binary);
  if (!out)
    throw Error("Cannot create raw CSV");
  out << std::setprecision(17) << "\xef\xbb\xbf";
  if (m == Metric::TDR) {
    auto td = transformTdr(crop(t, t.x.front(), settings.stopHz), settings, c);
    if (td.time.empty())
      throw Error(td.note);
    out << "Time_s,Impedance_ohm,Reflection_rho,Termination\r\n";
    for (size_t i = 0; i < td.time.size(); ++i) {
      check(c);
      out << td.time[i] << ',' << td.impedance[i] << ',' << td.reflection[i] << ',' << name(t.termination,t.parameter.starts_with("Sdd")) << "\r\n";
    }
  } else {
    auto a = crop(t, settings.startHz, settings.stopHz);
    out << "Frequency_Hz,Real,Imaginary,LogMagnitude_dB,Phase_deg\r\n";
    for (size_t i = 0; i < a.x.size(); ++i) {
      if (i % 8192 == 0)
        check(c);
      out << a.x[i] << ',' << a.s[i].real() << ',' << a.s[i].imag() << ','
          << logMagnitude(a.s[i]) << ',' << std::arg(a.s[i]) * 180 / pi
          << "\r\n";
    }
  }
  out.close();
  if (!out)
    throw Error("CSV write failed");
  check(c);
  atomicReplace(tmp.path, p);
}
} // namespace si
