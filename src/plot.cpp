#include "app.hpp"
#include <cmath>
#include <set>
#include <map>
static QColor color(int i) {
  static const char *c[] = {"#008f91", "#7c5ce7", "#ee8652", "#3178c6",
                            "#e05273", "#88942c", "#269c70", "#995ca4",
                            "#93502d", "#44546a", "#a1336b", "#30603b"};
  return QColor(c[i % 12]);
}
static QRectF area(QRectF r, bool heat = false) {
  return r.adjusted(82, 100, -(heat ? 35 : 225), -62);
}
std::pair<double, double> graphXRange(const PlotSnapshot &s) {
  double lo = std::numeric_limits<double>::infinity(), hi = -lo;
  for (auto &c : s.curves) {
    if (std::isfinite(c.start))
      lo = std::min(lo, c.start);
    if (std::isfinite(c.stop))
      hi = std::max(hi, c.stop);
  }
  if (!std::isfinite(lo) || !std::isfinite(hi)) {
    lo = 0;
    hi = 1;
  }
  if (std::isfinite(s.viewStart) && std::isfinite(s.viewStop) &&
      s.viewStop > s.viewStart) {
    lo = s.viewStart;
    hi = s.viewStop;
  } else if (s.metric == si::Metric::TDR && !s.fitAll && hi > lo) {
    // Keep the initial engineering view stable rather than feature-seeking.
    // A small pre-zero margin separates time zero from the left plot border,
    // while the 1.5 ns right edge keeps the long low-value tail out of the
    // initial view. Fit All still restores the complete computed range.
    constexpr double reviewStart = -0.1e-9;
    constexpr double reviewStop = 1.5e-9;
    const double fullLo = lo, fullHi = hi;
    constexpr double zeroStartTolerance = 1e-15;
    if (fullHi >= 0.0 && std::abs(fullLo) <= zeroStartTolerance) {
      lo = reviewStart;
      hi = reviewStop;
    } else {
      // Unusual non-zero-based data keeps the same 1.6 ns review span and
      // receives the same 0.1 ns pre-roll before its first available sample.
      constexpr double preRoll = 0.1e-9;
      const double reviewSpan = reviewStop - reviewStart;
      lo = fullLo - preRoll;
      hi = lo + reviewSpan;
    }
  }
  if (hi <= lo)
    hi = lo + std::max(1., std::abs(lo) * .01);
  return {lo, hi};
}
QRectF graphArea(QRectF r, const PlotSnapshot &s) {
  auto box = area(r, s.heatmap && s.metric != si::Metric::TDR);
  if (r.height() < 240) box.setTop(box.top() - 22);
  if (s.metric == si::Metric::TDR) box.setTop(box.top() + 24);
  return box;
}
std::pair<double, double> graphYRange(const PlotSnapshot &s) {
  if (std::isfinite(s.viewBottom) && std::isfinite(s.viewTop) &&
      s.viewTop > s.viewBottom)
    return {s.viewBottom, s.viewTop};

  auto [lo, hi] = graphXRange(s);
  double ymin = std::numeric_limits<double>::infinity(), ymax = -ymin;
  std::vector<double> tdrFocus;
  const bool terminationAwareTdr =
      s.metric == si::Metric::TDR && !s.fitAll && !s.settings.tdrReflection &&
      std::any_of(s.curves.begin(), s.curves.end(), [](const auto &c) {
        return c.termination != si::Termination::Reference;
      });

  for (auto &c : s.curves) {
    const double focusCeiling =
        std::isfinite(c.targetOhm) && c.targetOhm > 0 ? c.targetOhm * 3.0
                                                      : si::NaN;
    for (size_t i = 0; i < c.plotX.size(); ++i)
      if (c.plotX[i] >= lo && c.plotX[i] <= hi &&
          std::isfinite(c.plotY[i])) {
        ymin = std::min(ymin, c.plotY[i]);
        ymax = std::max(ymax, c.plotY[i]);
        // Open/short/resistive termination can create a brief endpoint
        // singularity that is useful data but a poor default axis anchor.
        // Keep those samples in the trace and statistics, while deriving the
        // automatic view from the physically useful impedance neighborhood.
        if (terminationAwareTdr && std::isfinite(focusCeiling) &&
            c.plotY[i] >= 0 && c.plotY[i] <= focusCeiling)
          tdrFocus.push_back(c.plotY[i]);
      }
  }

  if (terminationAwareTdr && tdrFocus.size() >= 8) {
    std::sort(tdrFocus.begin(), tdrFocus.end());
    auto percentile = [&](double q) {
      const double position = q * double(tdrFocus.size() - 1);
      const size_t a = size_t(std::floor(position));
      const size_t b = std::min(a + 1, tdrFocus.size() - 1);
      const double fraction = position - double(a);
      return tdrFocus[a] + (tdrFocus[b] - tdrFocus[a]) * fraction;
    };
    ymin = percentile(.02);
    ymax = percentile(.98);
    // Keep the nominal line impedance visible even when the currently shown
    // interval is dominated by the termination transition.
    for (auto &c : s.curves)
      if (std::isfinite(c.targetOhm) && c.targetOhm > 0) {
        const double context = std::max(
            c.targetOhm * .10,
            s.settings.tdrLimitEnabled
                ? c.targetOhm * s.settings.tolerancePercent / 100
                : 0.0);
        ymin = std::min(ymin, c.targetOhm - context);
        ymax = std::max(ymax, c.targetOhm + context);
      }
  }

  const auto &limit = s.settings.limit(s.metric);
  if (s.metric != si::Metric::TDR && limit.enabled) {
    if (limit.points.empty()) {
      ymin = std::min(ymin, limit.constant);
      ymax = std::max(ymax, limit.constant);
    } else
      for (auto [f, v] : limit.points)
        if (f >= lo && f <= hi) {
          ymin = std::min(ymin, v);
          ymax = std::max(ymax, v);
        }
  }
  if (s.metric == si::Metric::TDR && s.settings.tdrLimitEnabled &&
      !s.settings.tdrReflection)
    for (auto &c : s.curves) {
      double tol = c.targetOhm * s.settings.tolerancePercent / 100;
      ymin = std::min(ymin, c.targetOhm - tol);
      ymax = std::max(ymax, c.targetOhm + tol);
    }
  if (!std::isfinite(ymin)) {
    ymin = s.metric == si::Metric::TDR ? 0 : -100;
    ymax = s.metric == si::Metric::TDR
               ? (s.settings.tdrReflection ? 1 : 110)
               : 0;
  }
  if (ymax <= ymin)
    ymax = ymin + std::max(1., std::abs(ymin) * .02);
  double pad = std::max(
      (ymax - ymin) * .12,
      s.metric == si::Metric::TDR ? (s.settings.tdrReflection ? .02 : 1.) : 2.);
  ymin -= pad;
  ymax += pad;
  return {ymin, ymax};
}
QString graphParameterLabel(const PlotSnapshot &s) {
  if (s.metric == si::Metric::TDR)
    return s.settings.tdrReflection ? "Reflection coefficient ρ [1]" : "Impedance [Ω]";
  std::set<QString> parameters;
  for (const auto &curve : s.curves) {
    if (curve.parameter.empty())
      continue;
    if (s.metric == si::Metric::NEXT || s.metric == si::Metric::FEXT)
      parameters.insert(curve.parameter.starts_with("Sdd")
                            ? "Sdd[response, stimulus] [dB]"
                            : "S[response, stimulus] [dB]");
    else
      parameters.insert(qs(curve.parameter) + " [dB]");
  }
  if (parameters.empty()) {
    if (s.metric == si::Metric::RL)
      return s.settings.reverse ? "S22 [dB] / Sdd22 [dB]"
                                : "S11 [dB] / Sdd11 [dB]";
    if (s.metric == si::Metric::IL)
      return s.settings.reverse ? "S12 [dB] / Sdd12 [dB]"
                                : "S21 [dB] / Sdd21 [dB]";
    return "S / Sdd[response, stimulus] [dB]";
  }
  QStringList labels;
  for (const auto &parameter : parameters)
    labels << parameter;
  return labels.join(" / ");
}
QString graphQualityLabel(const PlotSnapshot &s) {
  if (s.metric != si::Metric::TDR)
    return {};
  auto count = [&](const char *quality) {
    return std::count_if(s.curves.begin(), s.curves.end(),
                         [&](const auto &c) { return c.quality == quality; });
  };
  return QString("TDR quality: GOOD %1  /  LIMITED %2  /  UNSUITABLE %3")
      .arg(count("GOOD"))
      .arg(count("LIMITED"))
      .arg(count("UNSUITABLE"));
}
QPolygonF simplifyGraphLine(const QPolygonF &points, double tolerance) {
  if (points.size() < 3 || tolerance <= 0) return points;
  std::vector<bool> keep(size_t(points.size()),false);
  keep.front() = keep.back() = true;
  std::vector<std::pair<qsizetype,qsizetype>> stack{{0,points.size()-1}};
  while (!stack.empty()) {
    auto [a,b] = stack.back(); stack.pop_back();
    QPointF d = points[b] - points[a];
    double length2 = QPointF::dotProduct(d,d), maximum = tolerance*tolerance;
    qsizetype split = -1;
    for (auto i=a+1; i<b; ++i) {
      QPointF v = points[i] - points[a];
      double fraction = length2 > 0 ? std::clamp(QPointF::dotProduct(v,d)/length2,0.,1.) : 0.;
      auto delta = v - d*fraction; double error = QPointF::dotProduct(delta,delta);
      if (error > maximum) { maximum=error; split=i; }
    }
    if (split >= 0) { keep[size_t(split)]=true; stack.push_back({a,split}); stack.push_back({split,b}); }
  }
  QPolygonF out; out.reserve(points.size());
  for (qsizetype i=0; i<points.size(); ++i) if (keep[size_t(i)]) out.push_back(points[i]);
  return out;
}

// Display-only monotone cubic interpolation for Quick TDR. The electrical
// samples and every numerical verdict remain untouched; this only allocates
// more drawing points inside the currently visible viewport. Straight/flat
// regions collapse again through pixel-space simplification, so the point
// budget naturally concentrates around transitions instead of the dummy tail.
std::vector<QPolygonF> tdrDisplaySegments(const std::vector<double> &x,
                                         const std::vector<double> &y,
                                         double lo, double hi, double ymin,
                                         double ymax, QRectF box,
                                         double tolerancePixels) {
  std::vector<QPolygonF> output;
  if (x.size() != y.size() || x.size() < 2 || !(hi > lo) || !(ymax > ymin) ||
      box.width() <= 0 || box.height() <= 0)
    return output;
  auto px = [&](double value) {
    return box.left() + (value - lo) / (hi - lo) * box.width();
  };
  const double yspan = ymax - ymin;
  auto py = [&](double value) {
    // A termination singularity may be orders of magnitude off screen. Bound
    // only the paint coordinate; the stored sample remains unchanged.
    value = std::clamp(value, ymin - 4 * yspan, ymax + 4 * yspan);
    return box.bottom() - (value - ymin) / yspan * box.height();
  };
  size_t first = size_t(std::lower_bound(x.begin(), x.end(), lo) - x.begin());
  size_t last = size_t(std::upper_bound(x.begin(), x.end(), hi) - x.begin());
  if (first) --first;
  if (last < x.size()) ++last;
  last = std::min(last, x.size());

  auto endpointSlope = [](double h0, double h1, double d0, double d1) {
    double m = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (m * d0 <= 0)
      return 0.0;
    if (d0 * d1 < 0 && std::abs(m) > 3 * std::abs(d0))
      return 3 * d0;
    return m;
  };

  size_t begin = first;
  while (begin < last) {
    while (begin < last && !std::isfinite(y[begin])) ++begin;
    if (begin >= last) break;
    size_t end = begin + 1;
    while (end < last && std::isfinite(y[end]) && x[end] > x[end - 1]) ++end;
    const size_t n = end - begin;
    if (n >= 2) {
      std::vector<double> h(n - 1), d(n - 1), m(n);
      for (size_t k = 0; k + 1 < n; ++k) {
        h[k] = x[begin + k + 1] - x[begin + k];
        d[k] = (y[begin + k + 1] - y[begin + k]) / h[k];
      }
      if (n == 2) {
        m[0] = m[1] = d[0];
      } else {
        m[0] = endpointSlope(h[0], h[1], d[0], d[1]);
        m[n - 1] = endpointSlope(h[n - 2], h[n - 3], d[n - 2], d[n - 3]);
        for (size_t k = 1; k + 1 < n; ++k) {
          if (d[k - 1] * d[k] <= 0) {
            m[k] = 0;
          } else {
            const double w1 = 2 * h[k] + h[k - 1];
            const double w2 = h[k] + 2 * h[k - 1];
            m[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k]);
          }
        }
      }

      QPolygonF segment;
      for (size_t k = 0; k + 1 < n; ++k) {
        const double x0 = x[begin + k], x1 = x[begin + k + 1];
        const double a = std::max(x0, lo), b = std::min(x1, hi);
        if (!(b >= a)) continue;
        const double pixelSpan = std::abs(px(b) - px(a));
        // About one interpolated vertex per screen pixel is enough for a
        // visually smooth antialiased line. The cap prevents a single sparse
        // source interval from consuming an excessive point budget.
        const int parts = std::clamp(int(std::ceil(pixelSpan)), 1, 96);
        for (int q = 0; q <= parts; ++q) {
          if (!segment.empty() && q == 0) continue;
          const double xv = a + (b - a) * double(q) / double(parts);
          const double u = std::clamp((xv - x0) / h[k], 0.0, 1.0);
          const double u2 = u * u, u3 = u2 * u;
          double yv = (2 * u3 - 3 * u2 + 1) * y[begin + k] +
                      (u3 - 2 * u2 + u) * h[k] * m[k] +
                      (-2 * u3 + 3 * u2) * y[begin + k + 1] +
                      (u3 - u2) * h[k] * m[k + 1];
          // PCHIP is shape-preserving; clamp is a final numerical guard against
          // floating-point overshoot at very large impedance singularities.
          yv = std::clamp(yv, std::min(y[begin + k], y[begin + k + 1]),
                          std::max(y[begin + k], y[begin + k + 1]));
          segment.push_back(QPointF(px(xv), py(yv)));
        }
      }
      if (segment.size() >= 2)
        output.push_back(simplifyGraphLine(segment, tolerancePixels));
    }
    begin = std::max(end, begin + 1);
  }
  return output;
}
void paintGraph(QPainter &p, QRectF r, const PlotSnapshot &s, bool interactive) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(r, Qt::white);
  const bool compactHeader = r.height() < 240;
  const QString title = qs(si::name(s.metric)) + "  ·  " +
                        graphParameterLabel(s) +
                        (s.heatmap ? "  /  Heatmap" : "");
  QFont titleFont("Segoe UI", 17, QFont::DemiBold);
  while (QFontMetricsF(titleFont).horizontalAdvance(title) > r.width() - 44 &&
         titleFont.pointSizeF() > 9)
    titleFont.setPointSizeF(titleFont.pointSizeF() - .5);
  p.setFont(titleFont);
  p.setPen(QColor("#172e47"));
  p.drawText(r.adjusted(24, 14, -20, 0), Qt::AlignTop, title);
  p.setFont(QFont("Segoe UI", 9));
  p.setPen(QColor("#64758b"));
  const QString context = QString::number(s.curves.size()) + " traces  •  " +
                          (s.bothDirections ? QString("Both directions")
                                            : qs(si::metricDirection(s.metric, s.settings.reverse)).replace(" -> ", " → "));
  const double projectWidth = std::max(
      0., r.width() - 54 - QFontMetricsF(p.font()).horizontalAdvance(context));
  const QString project = QFontMetricsF(p.font()).elidedText(
      s.project, Qt::ElideRight, projectWidth);
  if (!compactHeader) p.drawText(r.adjusted(24, 47, -20, 0), Qt::AlignTop,
             (project.isEmpty() ? QString() : project + "  •  ") + context);
  if (s.metric == si::Metric::TDR)
    p.drawText(QRectF(r.left() + 24, r.top() + (compactHeader ? 69 : 91), r.width() - 48, 20),
               Qt::AlignLeft, graphQualityLabel(s));
  if (s.curves.empty()) {
    p.setFont(QFont("Segoe UI", 13));
    p.drawText(r, Qt::AlignCenter,
               "Open Touchstone → Select channels → QUICK ANALYSIS");
    p.restore();
    return;
  }
  if (s.metric == si::Metric::TDR &&
      std::all_of(s.curves.begin(), s.curves.end(), [](const auto &c) {
        return c.quality == "UNSUITABLE" && c.plotX.empty();
      })) {
    p.setPen(QColor("#a43f38"));
    p.setFont(QFont("Segoe UI", 12));
    p.drawText(r.adjusted(40, 110, -40, -35),
               Qt::AlignCenter | Qt::TextWordWrap,
               "Quick TDR unavailable · UNSUITABLE\n\nThe current input data and transform settings cannot produce a Quick TDR waveform.\nDo not use this result for impedance evaluation.\n\n" +
                   qs(s.curves.front().note));
    p.restore();
    return;
  }
  auto [lo, hi] = graphXRange(s);
  double scale = s.metric == si::Metric::TDR ? 1e9 : 1e-9;
  QString unit = s.metric == si::Metric::TDR ? "Time [ns]" : "Frequency [GHz]";
  auto box = graphArea(r, s);
  auto px = [&](double x) {
    return box.left() + (x - lo) / (hi - lo) * box.width();
  };
  p.setFont(QFont("Segoe UI", 9));
  p.drawText(
      QRectF(r.left() + 24, r.top() + (compactHeader ? 47 : 69), r.width() - 48, 20), Qt::AlignLeft,
      QString("Analysis: %1 – %2 %3  |  Limit: %4")
          .arg(num(s.curves.front().start * scale))
          .arg(num(s.curves.front().stop * scale))
          .arg(s.metric == si::Metric::TDR ? "ns" : "GHz")
          .arg(s.metric == si::Metric::TDR
                   ? (s.settings.tdrLimitEnabled && !s.settings.tdrReflection
                          ? "Target ± " + num(s.settings.tolerancePercent, 1) +
                                "%"
                          : "Off / N/A")
                   : (s.settings.limit(s.metric).enabled
                          ? (s.settings.limit(s.metric).points.empty()
                                 ? (s.metric == si::Metric::IL ? "value ≥ "
                                                               : "value ≤ ") +
                                       num(s.settings.limit(s.metric).constant,
                                           2) +
                                       " dB"
                                 : "Frequency mask")
                          : "Off / N/A")));
  if (s.heatmap && s.metric != si::Metric::TDR) {
    double mn = std::numeric_limits<double>::infinity(), mx = -mn;
    for (auto &c : s.curves)
      for (double y : s.margin ? c.heatMargin : c.heatRaw)
        if (std::isfinite(y)) {
          mn = std::min(mn, y);
          mx = std::max(mx, y);
        }
    if (!std::isfinite(mn)) {
      mn = -1;
      mx = 1;
    }
    if (mx <= mn)
      mx = mn + 1;
    double rowHeight = box.height() / double(s.curves.size());
    for (size_t j = 0; j < s.curves.size(); ++j) {
      auto &c = s.curves[j];
      auto &values = s.margin ? c.heatMargin : c.heatRaw;
      for (size_t i = 0; i < values.size(); ++i) {
        double a = c.start +
                   (c.stop - c.start) * double(i) / double(values.size()),
               b = c.start +
                   (c.stop - c.start) * double(i + 1) / double(values.size());
        if (b < lo || a > hi)
          continue;
        double v = values[i];
        QColor fill("#e9eef4");
        if (!std::isnan(v)) {
          if (s.margin)
            fill = v < 0
                       ? QColor("#d44851")
                       : QColor::fromRgbF(
                             .25, .60 + .30 * std::clamp(v / 10., 0., 1.), .60);
          else {
            double z = std::clamp((v - mn) / (mx - mn), 0., 1.);
            if (s.metric == si::Metric::IL)
              z = 1 - z;
            fill = QColor::fromHsvF((1 - z) * .48, .55, .93);
          }
        }
        p.fillRect(
            QRectF(px(std::max(a, lo)), box.top() + j * rowHeight,
                   std::max(1., px(std::min(b, hi)) - px(std::max(a, lo))),
                   rowHeight + .2),
            fill);
      }
      if (rowHeight >= 12) {
        p.setPen(QColor("#172e47"));
        p.setFont(QFont("Segoe UI", 8));
        p.drawText(
            QRectF(r.left() + 3, box.top() + j * rowHeight, 76, rowHeight),
            Qt::AlignRight | Qt::AlignVCenter, qs(c.channel));
      }
    }
    p.setPen(QColor("#64758b"));
    p.drawText(QRectF(box.left(), box.bottom() + 38, box.width(), 20),
               Qt::AlignCenter,
               s.margin ? "Worst margin per cell • Red = NG • Gray = N/A"
                        : "Worst value per cell • Teal = better • Red = worse");
  } else {
    auto [ymin, ymax] = graphYRange(s);
    const auto &limit = s.settings.limit(s.metric);
    auto py = [&](double y) {
      if (std::isinf(y))
        return y < 0 ? box.bottom() : box.top();
      return box.bottom() - (y - ymin) / (ymax - ymin) * box.height();
    };
    const int yIntervals = std::clamp(int(box.height() / 28.), 1, 5);
    for (int i = 0; i <= yIntervals; ++i) {
      double y = ymin + (ymax - ymin) * i / double(yIntervals);
      p.setPen(QPen(QColor("#e8eef4"), 1));
      p.drawLine(QPointF(box.left(), py(y)), QPointF(box.right(), py(y)));
      p.setPen(QColor("#64758b"));
      p.drawText(QRectF(r.left() + 5, py(y) - 10, 68, 20),
                 Qt::AlignRight | Qt::AlignVCenter, num(y, 1));
    }
    p.save();
    p.setClipRect(box);
    if (s.metric == si::Metric::TDR && lo <= 0 && hi >= 0) {
      p.setPen(QPen(QColor("#8396aa"), 1, Qt::DashLine));
      p.drawLine(QPointF(px(0), box.top()), QPointF(px(0), box.bottom()));
    }
    auto line = [&](double a, double b, double ya, double yb) {
      p.drawLine(QPointF(px(a), py(ya)), QPointF(px(b), py(yb)));
    };
    p.setPen(QPen(QColor("#d44851"), 1.5, Qt::DashLine));
    if (s.metric == si::Metric::TDR && s.settings.tdrLimitEnabled && !s.settings.tdrReflection) {
      std::set<double> targets;
      for (auto &c : s.curves)
        if (targets.insert(c.targetOhm).second) {
          double t = c.targetOhm, delta = t * s.settings.tolerancePercent / 100;
          line(lo, hi, t - delta, t - delta);
          line(lo, hi, t + delta, t + delta);
        }
    } else if (s.metric != si::Metric::TDR && limit.enabled) {
      if (limit.points.empty())
        line(lo, hi, limit.constant, limit.constant);
      else
        for (size_t i = 1; i < limit.points.size(); ++i)
          line(limit.points[i - 1].first, limit.points[i].first,
               limit.points[i - 1].second, limit.points[i].second);
    }
    auto focused = [&](const si::Result &c) { return s.highlightChannelIds.empty() || s.highlightChannelIds.count(c.channelId) || (!c.aggressorId.empty() && s.highlightChannelIds.count(c.aggressorId)); };
    std::map<std::string,int> logicalColors; std::vector<int> curveColors(s.curves.size()); int nextColor=0;
    for(size_t i=0;i<s.curves.size();++i){const auto &c=s.curves[i];const std::string key=c.revision+"|"+c.channelId+"|"+c.aggressorId+"|"+std::to_string(int(c.termination));auto [it,inserted]=logicalColors.emplace(key,nextColor);if(inserted)++nextColor;curveColors[i]=it->second;}
    std::vector<size_t> drawOrder; drawOrder.reserve(s.curves.size());
    for(size_t j=0;j<s.curves.size();++j) if(!focused(s.curves[j])) drawOrder.push_back(j);
    for(size_t j=0;j<s.curves.size();++j) if(focused(s.curves[j])) drawOrder.push_back(j);
    for (size_t j : drawOrder) {
      auto &c = s.curves[j]; const bool isFocused=focused(c); QColor traceColor=color(curveColors[j]); if(!s.highlightChannelIds.empty()&&!isFocused) traceColor.setAlpha(85); QPen tracePen(traceColor,(!s.highlightChannelIds.empty()&&isFocused)?3.2:1.7); if(s.bothDirections&&c.reverse)tracePen.setStyle(Qt::DashLine); p.setPen(tracePen);
      QPainterPath path;
      if (s.metric == si::Metric::TDR) {
        const auto segments = tdrDisplaySegments(
            c.plotX, c.plotY, lo, hi, ymin, ymax, box,
            interactive ? .15 : .08);
        for (const auto &points : segments) {
          if (points.empty()) continue;
          path.moveTo(points.front());
          for (qsizetype k = 1; k < points.size(); ++k)
            path.lineTo(points[k]);
        }
      } else {
        QPolygonF segment;
        auto flush = [&] {
          const auto points = interactive ? simplifyGraphLine(segment,.2) : segment;
          if (!points.empty()) {
            path.moveTo(points.front());
            for (qsizetype k=1; k<points.size(); ++k) path.lineTo(points[k]);
          }
          segment.clear();
        };
        size_t first = size_t(std::lower_bound(c.plotX.begin(),c.plotX.end(),lo)-c.plotX.begin());
        size_t last = size_t(std::upper_bound(c.plotX.begin(),c.plotX.end(),hi)-c.plotX.begin());
        if (first) --first;
        if (last < c.plotX.size()) ++last;
        for (size_t i = first; i < last; ++i) {
          if (std::isnan(c.plotY[i])) {
            flush();
            continue;
          }
          segment.push_back(QPointF(px(c.plotX[i]), py(c.plotY[i])));
        }
        flush();
      }
      // Markers set a fill brush; traces must never fill over other curves.
      p.setBrush(Qt::NoBrush);
      p.drawPath(path);
      for (auto marker : c.peaks) {
        if (marker.x < lo || marker.x > hi || std::isnan(marker.y))
          continue;
        QPointF point(px(marker.x), py(marker.y));
        p.setBrush(marker.global ? traceColor : Qt::white);
        p.drawEllipse(point, marker.global ? 4.5 : 3.,
                      marker.global ? 4.5 : 3.);
        if ((s.curves.size() <= 4 && marker.global) ||
            (marker.manual && s.curves.size() <= 2)) {
          p.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
          QString label = (marker.manual ? "M " : "Worst ") + num(marker.y, 2);
          double left =
              std::clamp(point.x() + 7, box.left() + 2, box.right() - 90);
          p.drawText(
              QRectF(left, std::max(box.top() + 2, point.y() - 22), 90, 20),
              Qt::AlignLeft, label);
        }
      }
    }
    p.restore();
    double legendStep = std::clamp(
        box.height() / std::max<size_t>(1, s.curves.size()), 38.0, 44.0);
    int maxLegend = std::max(1, int(box.height() / legendStep));
    for (size_t j = 0; j < s.curves.size() && int(j) < maxLegend; ++j) {
      auto &c = s.curves[j];
      double top = box.top() + j * legendStep;
      QColor legendColor=color(curveColors[j]); const bool isFocused=focused(c); if(!s.highlightChannelIds.empty()&&!isFocused) legendColor.setAlpha(85); QPen legendPen(legendColor,(!s.highlightChannelIds.empty()&&isFocused)?4:3); if(s.bothDirections&&c.reverse) legendPen.setStyle(Qt::DashLine); p.setPen(legendPen);
      p.drawLine(QPointF(box.right() + 18, top + 8), QPointF(box.right() + 33, top + 8));
      p.setPen(QColor("#172e47"));
      p.setFont(QFont("Segoe UI", 9, QFont::DemiBold));
      QString label = qs(c.channel) + (c.aggressor.empty() ? "" : " ← " + qs(c.aggressor)); if(s.bothDirections) label += c.reverse ? " · F→N" : " · N→F";
      if(c.metric==si::Metric::TDR)label+=" / "+qs(si::shortName(c.termination,c.parameter.starts_with("Sdd")));
      p.drawText(QRectF(box.right() + 40, top, 178, 16), Qt::AlignLeft, label);
      p.setFont(QFont("Segoe UI", 8));
      p.setPen(c.status == "NG" ? QColor("#cc3f50") : QColor("#64758b"));
      p.drawText(QRectF(box.right() + 40, top + 16, 178, 17), Qt::AlignLeft,
                 qs(c.revision) + " • " + num(c.worst, 2) + " • " +
                     qs(c.status));
    }
    if (int(s.curves.size()) > maxLegend)
      p.drawText(QRectF(box.right() + 18, box.bottom() + 8, 195, 20),
                 "+ " + QString::number(int(s.curves.size()) - maxLegend) +
                     " traces; select result rows");
  }
  for (int i = 0; i <= 6; ++i) {
    double x = lo + (hi - lo) * i / 6.;
    p.setPen(QColor("#64758b"));
    p.setFont(QFont("Segoe UI", 9));
    p.drawText(QRectF(px(x) - 42, box.bottom() + 9, 84, 20), Qt::AlignCenter,
               num(x * scale, 2));
  }
  p.setPen(QColor("#b8c7d7"));
  p.drawRect(box);
  p.setPen(QColor("#64758b"));
  p.drawText(QRectF(box.left(), box.bottom() + 32, box.width(), 22),
             Qt::AlignCenter,
             unit + (s.heatmap ? "" : " • wheel: X zoom | Ctrl: Y | drag: pan | Shift+drag: box"));
  p.restore();
}
QImage graphImage(const PlotSnapshot &s, QSize size) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  QPainter painter(&image);
  painter.scale(size.width() / 1200., size.height() / 700.);
  paintGraph(painter, QRectF(0, 0, 1200, 700), s);
  return image;
}
void savePngFile(const QString &path, const QImage &image,
                 si::Control *control) {
  if (control)
    control->check();
  QByteArray encoded;
  QBuffer buffer(&encoded);
  if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
    throw si::Error("PNG encoding failed");
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size())
    throw si::Error("PNG write failed");
  if (control)
    control->check();
  if (!file.commit())
    throw si::Error("PNG commit failed");
}
