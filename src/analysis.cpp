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
    const size_t none = std::numeric_limits<size_t>::max();
    size_t mn = none, mx = none, gap = none;
    while (i < last) {
      size_t b = hi == lo
                     ? 0
                     : std::min(buckets - 1, size_t((x[i] - lo) / (hi - lo) *
                                                    double(buckets)));
      if (b != bin)
        break;
      if (!std::isfinite(y[i])) {
        if (gap == none)
          gap = i;
        ++i;
        continue;
      }
      if (mn == none || y[i] < y[mn])
        mn = i;
      if (mx == none || y[i] > y[mx])
        mx = i;
      ++i;
    }
    if (gap != none)
      out.push_back(gap);
    if (mn != none) {
      if (mn > mx)
        std::swap(mn, mx);
      out.push_back(mn);
      if (mx != mn)
        out.push_back(mx);
    }
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
  if (metric != Metric::TDR)
    limit.validate();
  if (metric == Metric::TDR) {
    if (set.tolerancePercent < 0 || !std::isfinite(set.tolerancePercent))
      throw Error("Invalid TDR tolerance");
    if (set.targetOhm < 0 || !std::isfinite(set.targetOhm))
      throw Error("Invalid TDR target impedance");
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

Delta compare(const Trace &a, const Trace &b, Metric m) {
  if (m == Metric::TDR)
    throw Error("TDR frequency delta is not supported");
  if (a.x.empty() || b.x.empty() || a.x.size() != a.s.size() ||
      b.x.size() != b.s.size())
    throw Error("Empty or inconsistent comparison trace");
  if (!(a.referenceOhm > 0) || !(b.referenceOhm > 0) ||
      !(a.responseReferenceOhm > 0) || !(b.responseReferenceOhm > 0) ||
      !std::isfinite(a.referenceOhm) || !std::isfinite(b.referenceOhm) ||
      !std::isfinite(a.responseReferenceOhm) ||
      !std::isfinite(b.responseReferenceOhm))
    throw Error("Invalid comparison reference impedance");
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
  bool haveImprovement = false;
  d.worstImprovement = NaN;
  for (double f : d.frequency) {
    double aa = logMagnitude(interpolate(a.x, a.s, f)),
           bb = logMagnitude(interpolate(b.x, b.s, f));
    double raw = bb - aa;
    d.rawDb.push_back(raw);
    double imp = m == Metric::IL ? raw : -raw;
    d.improvement.push_back(imp);
    if (!std::isnan(imp)) {
      d.worstImprovement =
          haveImprovement ? std::min(d.worstImprovement, imp) : imp;
      haveImprovement = true;
    }
  }
  return d;
}
std::string overall(const std::vector<std::string> &statuses) {
  if (std::find(statuses.begin(), statuses.end(), "NG") != statuses.end())
    return "NG";
  if (statuses.empty())
    return "N/A";
  if (std::any_of(statuses.begin(), statuses.end(),
                  [](const auto &status) { return status != "OK"; }))
    return "N/A";
  return "OK";
}
