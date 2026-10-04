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
