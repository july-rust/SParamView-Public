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
          throw Error("Duplicate # option line");
        if (initialized)
          throw Error("Option line after network data");
        option = true;
        optionRef.clear();
        bool seenReference = false;
        auto optionToken = [](const std::string &w) {
          return w == "hz" || w == "khz" || w == "mhz" || w == "ghz" ||
                 w == "s" || w == "ri" || w == "ma" || w == "db" ||
                 w == "r";
        };
        for (size_t i = 0; i < s.size();) {
          auto w = lower(s[i]);
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
          else if (w == "r") {
            if (seenReference)
              throw Error("Duplicate R option");
            seenReference = true;
            ++i;
            const size_t firstReference = optionRef.size();
            while (i < s.size() && !optionToken(lower(s[i]))) {
              optionRef.push_back(number(s[i]));
              ++i;
            }
            if (optionRef.size() == firstReference)
              throw Error("Missing R value");
            continue;
          } else
            throw Error("Unsupported option (S-parameters only): " + s[i]);
          ++i;
        }
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
