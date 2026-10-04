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
  if (!std::isfinite(set.kaiserBeta) || set.kaiserBeta < 0 ||
      set.kaiserBeta > 13)
    return unsuitable("Window beta must be finite and in [0,13]");
  if (!std::isfinite(set.tdrStartSeconds) || set.tdrStartSeconds < 0 ||
      std::isnan(set.tdrStopSeconds) ||
      set.tdrStopSeconds < set.tdrStartSeconds)
    return unsuitable("Invalid TDR time range");
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
  for (size_t i = 0; i < n; ++i) {
    double impulse = spec[(i + n / 2) % n].real();
    if (i)
      cumulative += 0.5 * (previous + impulse);
    previous = impulse;
    time[i] = (double(i) - double(n / 2)) * dt;
    rho[i] = cumulative;
    double d = 1 - cumulative;
    if (std::abs(d) < 1e-8 || std::abs(cumulative) > 1.0001)
      z[i] = NaN;
    else
      z[i] = t.referenceOhm * (1 + cumulative) / d;
  }
  double lo = std::max(0., set.tdrStartSeconds),
         hi = std::min(0.45 / out.df, set.tdrStopSeconds);
  if (!std::isfinite(lo) || std::isnan(hi) || hi < lo)
    return unsuitable("Invalid TDR time range");
  bool singular = false;
  for (size_t i = 0; i < n; ++i)
    if (time[i] >= lo && time[i] <= hi) {
      out.time.push_back(time[i]);
      out.impedance.push_back(z[i]);
      out.reflection.push_back(rho[i]);
      // Suitability belongs to the interval the user asked to analyze. A
      // singularity elsewhere in the periodic FFT record must not turn an
      // otherwise valid selected window into UNSUITABLE/N/A.
      if (!std::isfinite(z[i]))
        singular = true;
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
