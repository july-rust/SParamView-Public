void MainWindow::refreshPlot(bool selectedOnly) {
  ++viewGeneration; pendingView.reset();
  if (viewControl) viewControl->cancelled = true;

  auto rows = selectedOnly ? table->selectionModel()->selectedRows() : QModelIndexList{};
  const auto activeRevision = activeRevisionName();
  auto visibleRevision=[&](const si::Result &r){return r.revision==activeRevision;};
  auto visibleDirection=[this](const si::Result &r){return directionMode==2 || r.reverse==(directionMode==1);};
  if (selectedOnly && !rows.empty() && size_t(rows.front().row())<resultRows.size()) displayed=results[resultRows[size_t(rows.front().row())]].metric;

  // TDR 50-ohm single-ended and 100-ohm differential traces use separate
  // views. A shared Y axis makes either family unnecessarily compressed and
  // obscures small impedance changes.
  int tdrMode = tdrView->currentIndex() == 1 ? 1 : 0;
  const int previousTdrMode = tdrMode;
  bool hasSingle = false, hasDifferential = false;
  double singleTarget = si::NaN, differentialTarget = si::NaN;
  if (displayed == si::Metric::TDR) {
    for (const auto &r : results) {
      if (r.metric != si::Metric::TDR || !visibleRevision(r) || !visibleDirection(r)) continue;
      if (differentialTdrResult(r)) {
        hasDifferential = true;
        if (!std::isfinite(differentialTarget) && r.targetOhm > 0)
          differentialTarget = r.targetOhm;
      } else {
        hasSingle = true;
        if (!std::isfinite(singleTarget) && r.targetOhm > 0)
          singleTarget = r.targetOhm;
      }
    }
    if (selectedOnly && !rows.empty()) {
      const auto &firstSelected = results[resultRows.at(size_t(rows.front().row()))];
      if (firstSelected.metric == si::Metric::TDR)
        tdrMode = differentialTdrResult(firstSelected) ? 1 : 0;
    }
    if (!hasSingle && hasDifferential) tdrMode = 1;
    if (!hasDifferential && hasSingle) tdrMode = 0;
    auto label = [](QString family, double target) {
      return std::isfinite(target) && target > 0
                 ? family + " · " + num(target, target < 200 ? 0 : 1) + " Ω"
                 : family;
    };
    if (tdrMode != previousTdrMode)
      plot->resetView();
    {
      QSignalBlocker block(tdrView);
      tdrView->setItemText(0, label("Single-ended", singleTarget));
      tdrView->setItemText(1, label("Differential", differentialTarget));
      tdrView->setCurrentIndex(tdrMode);
    }
    tdrView->setVisible(hasSingle || hasDifferential);
    tdrView->setEnabled(hasSingle && hasDifferential);
  } else {
    tdrView->hide();
  }

  auto matchesTdrView = [&](const si::Result &r) {
    return displayed != si::Metric::TDR ||
           int(differentialTdrResult(r)) == tdrMode;
  };
  std::vector<si::Result> curves;
  if (selectedOnly && !rows.empty()) {
    for (auto row : rows) {
      if (size_t(row.row()) >= resultRows.size()) continue;
      const auto &r = results[resultRows[size_t(row.row())]];
      if (r.metric == displayed && visibleRevision(r) && visibleDirection(r) && matchesTdrView(r) && curves.size() < 64)
        curves.push_back(r);
    }
  }
  if (curves.empty())
    for (auto &r : results)
      if (r.metric == displayed && visibleRevision(r) && visibleDirection(r) && matchesTdrView(r) && curves.size() < 64)
        curves.push_back(r);

  { QSignalBlocker block(metricView); metricView->setCurrentIndex(int(displayed)); }
  const bool resetTdrView =
      displayed == si::Metric::TDR &&
      (plot->snapshot.metric != si::Metric::TDR ||
       tdrViewIdentity(plot->snapshot.curves) != tdrViewIdentity(curves));
  plot->snapshot.curves = std::move(curves);
  plot->snapshot.settings = lastSettings; plot->snapshot.settings.reverse = directionMode == 1;
  plot->snapshot.bothDirections = directionMode == 2; plot->snapshot.highlightChannelIds.clear();
  for (auto *item : channels->selectedItems()) plot->snapshot.highlightChannelIds.insert(ss(item->data(Qt::UserRole).toString()));
  plot->snapshot.project = projectName;
  plot->snapshot.metric = displayed;
  plot->snapshot.heatmap = heatCheck->isChecked();
  plot->snapshot.margin = marginCheck->isChecked();
  if (resetTdrView)
    plot->resetView();
  else
    plot->invalidateGraph();

  // When a large analysis was initially compressed, immediately reconstruct
  // only the currently visible TDR viewport. Do not inflate unused tail data.
  const bool needsVisibleTdrDetail =
      displayed == si::Metric::TDR &&
      std::any_of(plot->snapshot.curves.begin(), plot->snapshot.curves.end(),
                  [](const auto &c) { return !c.plotComplete; });
  const bool missingPlot = std::any_of(
      plot->snapshot.curves.begin(), plot->snapshot.curves.end(),
      [](const auto &c) { return c.plotX.empty() && !std::isnan(c.worst); });
  if (needsVisibleTdrDetail || missingPlot)
    zoom(plot->snapshot.viewStart, plot->snapshot.viewStop);
}
void MainWindow::zoom(double lo, double hi) {
  if (busy || plot->snapshot.curves.empty()) return;
  const auto generation = ++viewGeneration;
  if (viewControl) viewControl->cancelled = true;
  if (viewRunning) { pendingView = {lo, hi}; return; }
  const bool heat = plot->snapshot.heatmap && plot->snapshot.metric != si::Metric::TDR;
  const size_t tdrViewBuckets = std::clamp<size_t>(
      size_t(std::max(256, plot->width())) * 2, 512, 4096);
  if (std::all_of(plot->snapshot.curves.begin(), plot->snapshot.curves.end(),
      [heat](const auto &r) { return r.plotComplete && !r.plotX.empty() && (!heat || !r.heatRaw.empty()); })) return;
  auto snap = std::make_shared<PlotSnapshot>(plot->snapshot);
  std::vector<si::Job> jobs;
  for (auto &r : snap->curves) {
    auto j = jobFor(r); if (!j) return; jobs.push_back(*j);
  }
  auto set = lastSettings;
  auto state = std::make_shared<si::Control>(); viewControl = state;
  viewRunning = true; pendingView.reset();
  viewPool.setMaxThreadCount(1); viewPool.setExpiryTimeout(-1);
  auto watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this,
      [this, watcher, snap, state, generation, lo, hi] {
        viewRunning = false; watcher->deleteLater();
        auto same = [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); };
        if (!state->cancelled && generation == viewGeneration && !busy &&
            same(lo,plot->snapshot.viewStart) && same(hi,plot->snapshot.viewStop)) {
          // Replace only display buffers. Preserve axes changed while the worker ran.
          plot->snapshot.curves = std::move(snap->curves);
          plot->invalidateGraph();
        }
        if (pendingView && !busy) {
          auto next = *pendingView; pendingView.reset(); zoom(next.first,next.second);
        }
      });
  viewFuture = QtConcurrent::run(&viewPool,
      [snap, jobs, set, lo, hi, heat, state, tdrViewBuckets] {
    auto c = state.get();
    try {
      for (size_t i = 0; i < jobs.size(); ++i) {
        c->check();
        auto &r = snap->curves[i];
        if (r.plotComplete && !r.plotX.empty() && (!heat || !r.heatRaw.empty())) continue;
        try {
          auto &job = jobs[i]; auto jobSet=set; jobSet.reverse=job.reverse.value_or(set.reverse);
          auto t = si::loadTrace(*job.cache, job.victim, job.metric, jobSet,
              job.aggressor ? &*job.aggressor : nullptr, c, job.termination);
          std::vector<double> x, y;
          if (job.metric == si::Metric::TDR) {
            auto td = si::transformTdr(si::crop(t,t.x.front(),jobSet.stopHz),jobSet,c);
            x = std::move(td.time);
            y = jobSet.tdrReflection ? std::move(td.reflection) : std::move(td.impedance);
          } else {
            auto clipped = si::crop(t,jobSet.startHz,jobSet.stopHz); x = std::move(clipped.x);
            for (auto v : clipped.s) y.push_back(si::logMagnitude(v));
          }
          if (x.empty()) continue;
          // Include one source point just outside each edge, preserving crossing segments.
          double left = std::isfinite(lo) ? lo : x.front(), right = std::isfinite(hi) ? hi : x.back();
          auto first = std::lower_bound(x.begin(),x.end(),left);
          auto last = std::upper_bound(x.begin(),x.end(),right);
          if (first != x.begin()) left = *(first-1);
          if (last != x.end()) right = *last;
          // Allocate raw display detail to the visible viewport only. The final
          // TDR paint pass then performs display-only monotone interpolation at
          // pixel resolution, so an unused tail does not consume a global 8192-point budget.
          const size_t displayBuckets =
              job.metric == si::Metric::TDR ? tdrViewBuckets : 2048;
          auto range = si::envelope(x,y,displayBuckets,left,right);
          r.plotComplete = range.size() == x.size(); r.plotX.clear(); r.plotY.clear();
          for (auto k : range) { r.plotX.push_back(x[k]); r.plotY.push_back(y[k]); }
          if (heat && r.heatRaw.empty()) {
            auto analyzed = si::analyze(t,job.metric,jobSet,c);
            r.heatRaw = std::move(analyzed.heatRaw); r.heatMargin = std::move(analyzed.heatMargin);
            r.heatX = std::move(analyzed.heatX);
          }
        } catch (const si::Cancelled &) { throw; }
          catch (const std::exception &e) { r.note = e.what(); }
      }
    } catch (...) { state->cancelled = true; }
  });
  watcher->setFuture(viewFuture);
}
