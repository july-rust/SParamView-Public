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
struct PnDifference {
  size_t index = std::string::npos;
  bool firstPositive = false;
};
static bool pnDelimiter(char c) {
  return c == '_' || c == ' ' || c == '.' || c == ':' || c == '/' ||
         c == '-';
}
static std::optional<PnDifference>
uniquePnDifference(const std::string &first, const std::string &second) {
  const auto a = lower(trim(first));
  const auto b = lower(trim(second));
  if (a.size() != b.size() || a.empty())
    return std::nullopt;
  size_t difference = std::string::npos;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] == b[i])
      continue;
    if (difference != std::string::npos ||
        !((a[i] == 'p' && b[i] == 'n') ||
          (a[i] == 'n' && b[i] == 'p')))
      return std::nullopt;
    difference = i;
  }
  if (difference == std::string::npos)
    return std::nullopt;
  std::string base = trim(first);
  if (difference >= base.size())
    return std::nullopt;
  const bool atStart = difference == 0;
  const bool atEnd = difference + 1 == base.size();
  const bool leftDelimiter = !atStart && pnDelimiter(base[difference - 1]);
  const bool rightDelimiter = !atEnd && pnDelimiter(base[difference + 1]);
  if (leftDelimiter && (rightDelimiter || atEnd))
    base.erase(atEnd ? difference - 1 : difference, 2);
  else if (atStart && rightDelimiter)
    base.erase(0, 2);
  else
    base.erase(difference, 1);
  if (trim(base).empty())
    return std::nullopt;
  return PnDifference{difference, a[difference] == 'p'};
}
static std::string pnBaseName(std::string name, size_t difference) {
  name = trim(name);
  const bool atStart = difference == 0;
  const bool atEnd = difference + 1 == name.size();
  const bool leftDelimiter = !atStart && pnDelimiter(name[difference - 1]);
  const bool rightDelimiter = !atEnd && pnDelimiter(name[difference + 1]);
  if (leftDelimiter && (rightDelimiter || atEnd))
    name.erase(atEnd ? difference - 1 : difference, 2);
  else if (atStart && rightDelimiter)
    name.erase(0, 2);
  else
    name.erase(difference, 1);
  return trim(name);
}
// Source-specific endpoint NET extraction; unqualified labels must not be
// guessed into physical channel mappings.
static std::string terminalNetFromLabel(const std::string &label) {
  const auto scope = label.rfind("::");
  if (scope != std::string::npos)
    return trim(label.substr(scope + 2));
  const auto space = label.find_last_of(" \t");
  if (space != std::string::npos)
    return trim(label.substr(space + 1));
  if (label.starts_with("DIE.") || label.starts_with("BGA.")) {
    auto dot = label.find('.', label.find('.') + 1);
    if (dot != std::string::npos) return trim(label.substr(dot + 1));
  }
  return {};
}
std::vector<Channel> suggestMapping(const Metadata &m,
                                    std::string *diagnostic,
                                    bool inferNamedDifferential) {
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
        const auto pn = uniquePnDifference(fields[1], fields[2]);
        if (!pn)
          throw Error("Cannot establish a unique P/N polarity from PowerSI channel hint");
        const size_t polarity = pn->index;
        const bool firstPositive = pn->firstPositive;
        std::string base = pnBaseName(fields[1], polarity);
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
          const auto &physicalLabel = m.labels[size_t(ports[i])];
          auto actualNet = terminalNetFromLabel(physicalLabel);
          if (actualNet.empty()) actualNet = trim(physicalLabel);
          if (lower(actualNet) != lower(trim(net)))
            throw Error("Differential hint does not match physical port net");
          std::smatch endpoint;
          if (std::regex_search(fields[3 + i], endpoint,
                                std::regex(R"(^\s*Port\s*(\d+)_)",
                                           std::regex::icase)) &&
              positiveInteger(endpoint[1].str()) != uint64_t(ports[i] + 1))
            throw Error("Differential hint endpoint does not match port index");
          // Scoped vendor labels also declare the physical component/pin.
          // Matching only the net must not silently confirm the wrong pin.
          if (endpoint[0].matched &&
              physicalLabel.find("::") != std::string::npos) {
            const auto hintedLabel =
                trim(fields[3 + i].substr(endpoint[0].length()));
            if (lower(hintedLabel) != lower(trim(physicalLabel)))
              throw Error("Differential hint physical endpoint mismatch");
          }
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
        const auto net = terminalNetFromLabel(m.labels[i]);
        if (!net.empty()) remaining[net].push_back(int(i));
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
    const auto net = terminalNetFromLabel(label);
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
    // Names are differential evidence only when two terminal nets are identical
    // case-insensitively except for exactly one P/N character. The P/N character
    // may appear anywhere in the net name (DATAP0/DATAN0, PCLK1/NCLK1, etc.).
    // Ambiguous, incomplete or multiply differing names remain single-ended
    // candidates and are never guessed into a differential pair.
    if (inferNamedDifferential) {
      std::vector<std::vector<size_t>> matches(terminalCandidates.size());
      for (size_t i = 0; i < terminalCandidates.size(); ++i)
        for (size_t j = i + 1; j < terminalCandidates.size(); ++j)
          if (uniquePnDifference(terminalCandidates[i].name,
                                 terminalCandidates[j].name)) {
            matches[i].push_back(j);
            matches[j].push_back(i);
          }
      std::map<std::string, std::vector<std::pair<size_t, size_t>>> pairsByBase;
      for (size_t i = 0; i < terminalCandidates.size(); ++i) {
        if (matches[i].size() != 1)
          continue;
        const size_t j = matches[i].front();
        if (j <= i || matches[j].size() != 1 || matches[j].front() != i)
          continue;
        const auto pn = uniquePnDifference(terminalCandidates[i].name,
                                           terminalCandidates[j].name);
        if (!pn)
          continue;
        const auto base = pnBaseName(terminalCandidates[i].name, pn->index);
        if (!base.empty())
          pairsByBase[lower(base)].push_back({i, j});
      }
      std::set<size_t> consumed;
      std::vector<Channel> combined;
      for (const auto &[key, pairs] : pairsByBase) {
        if (pairs.size() != 1)
          continue;
        const auto [i, j] = pairs.front();
        const auto pn = uniquePnDifference(terminalCandidates[i].name,
                                           terminalCandidates[j].name);
        if (!pn)
          continue;
        const auto &positive =
            pn->firstPositive ? terminalCandidates[i] : terminalCandidates[j];
        const auto &negative =
            pn->firstPositive ? terminalCandidates[j] : terminalCandidates[i];
        Channel c;
        c.name = pnBaseName(terminalCandidates[i].name, pn->index);
        c.id = "namepair:" + key;
        c.group = "Differential";
        c.nearP = positive.nearP;
        c.nearN = negative.nearP;
        c.farP = positive.farP;
        c.farN = negative.farP;
        combined.push_back(c);
        consumed.insert(i);
        consumed.insert(j);
      }
      if (!combined.empty()) {
        for (size_t i = 0; i < terminalCandidates.size(); ++i) {
          if (consumed.count(i))
            continue;
          auto candidate = terminalCandidates[i];
          candidate.group = "Single-ended";
          combined.push_back(std::move(candidate));
        }
        validateMapping(combined, m);
        return combined;
      }
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
    if (!inferNamedDifferential && !pol.empty()) {
      base += "_" + pol;
      pol.clear();
    }
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
      if (v.empty())
        return -1;
      const auto value = positiveInteger(v);
      if (value > uint64_t(std::numeric_limits<int>::max()))
        throw Error("Mapping CSV port index exceeds supported integer range");
      return static_cast<int>(value) - 1;
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
