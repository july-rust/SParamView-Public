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
