void MainWindow::exportXlsx() {
  if (busy || results.empty()) {
    error("Run analysis first.");
    return;
  }
  QDialog d(this);
  d.setWindowTitle("Excel Export");
  auto layout = new QVBoxLayout(&d);
  auto a = new QCheckBox("Summary"), b = new QCheckBox("Detailed results"),
       c = new QCheckBox("Graphs"),
       raw = new QCheckBox("Raw trace data (large)");
  for (auto check : {a, b, c, raw})
    layout->addWidget(check);
  a->setChecked(true);
  b->setChecked(true);
  c->setChecked(true);
  auto buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
  if (d.exec() != QDialog::Accepted)
    return;
  QString path = withExtension(
      QFileDialog::getSaveFileName(
          this, "Save Excel Workbook",
          projectName + "_SI_Analysis_" +
              QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") +
              ".xlsx",
          "Excel (*.xlsx)"),
      ".xlsx");
  if (path.isEmpty())
    return;
  ReportOptions options{a->isChecked(), b->isChecked(), c->isChecked(),
                        raw->isChecked()};
  auto rs = results;
  auto revs = revisions;
  auto set = lastSettings;
  auto jobs = lastJobs;
  auto snap = plot->snapshot;
  auto title = projectName;
  task(
      [path, rs, revs, set, options, jobs, snap, title](si::Control *control) {
        writeReport(path, rs, revs, set, title, options, jobs, snap, control);
      },
      [this, path] {
        notice->setText("Excel workbook saved: " + QFileInfo(path).fileName());
      });
}
void MainWindow::savePng() {
  if (results.empty() || busy)
    return;
  QString path =
      withExtension(QFileDialog::getSaveFileName(this, "Save Plot",
                                                 "SI_Graph.png", "PNG (*.png)"),
                    ".png");
  if (path.isEmpty())
    return;
  auto snap = plot->snapshot;
  task(
      [path, snap](si::Control *c) { savePngFile(path, graphImage(snap), c); });
}
void MainWindow::saveCsv() {
  if (busy)
    return;
  auto selection = table->selectionModel()->selectedRows();
  if (selection.size() != 1) {
    error("Select one result row to export as raw CSV.");
    return;
  }
  auto r = results[resultRows.at(size_t(selection[0].row()))];
  auto ptr = jobFor(r);
  if (!ptr)
    return;
  auto job = *ptr;
  auto settings = lastSettings;
  settings.reverse = job.reverse.value_or(settings.reverse);
  QString path = withExtension(
      QFileDialog::getSaveFileName(
          this, "Raw CSV",
          qs(r.channel) + "_" + qs(si::name(r.metric)) + ".csv", "CSV (*.csv)"),
      ".csv");
  if (path.isEmpty())
    return;
  task([job, settings, path](si::Control *c) {
    auto t = si::loadTrace(*job.cache, job.victim, job.metric, settings,
                           job.aggressor ? &*job.aggressor : nullptr, c, job.termination);
    si::exportRawCsv(fp(path), t, job.metric, settings, c);
  });
}
void MainWindow::compareRevisions() {
  if (busy)
    return;
  if (revisions.size() < 2) {
    error("Add at least two revisions.");
    return;
  }
  for (const auto &revision : revisions)
    if (!revision.confirmed) {
      error("Mapping confirmation required: " + revision.name);
      return;
    }
  if (displayed == si::Metric::TDR) {
    error("Compare TDR using plot overlays. Use dB deltas for RL/IL/NEXT/FEXT.");
    return;
  }
  auto selected = selectedChannels();
  if (selected.empty())
    return;
  auto revs = revisions;
  int baseIndex = revisionBox->currentIndex();
  auto metric = displayed;
  auto set = results.empty() ? settings : lastSettings;
  set.reverse = directionMode == 1;
  auto rows = std::make_shared<std::vector<QVariantList>>();
  task(
      [revs, baseIndex, selected, metric, set, rows](si::Control *control) {
        const auto &base = revs[size_t(baseIndex)];
        for (const auto &other : revs) {
          if (other.name == base.name)
            continue;
          const auto matches = si::matchChannels(selected, other.channels);
          const auto basematches = si::matchChannels(selected, base.channels);
          for (size_t i = 0; i < selected.size(); ++i) {
            std::vector<const si::Channel *> aggressors;
            if (metric == si::Metric::NEXT || metric == si::Metric::FEXT) {
              for (const auto &ag : base.channels)
                if (ag.id != selected[i].id && ag.group == selected[i].group)
                  aggressors.push_back(&ag);
            } else
              aggressors.push_back(nullptr);
            if (aggressors.empty())
              aggressors.push_back(nullptr);
            for (const auto *aggressor : aggressors) {
              const QString channel =
                  qs(selected[i].name) +
                  (aggressor ? " ← " + qs(aggressor->name) : "");
              try {
                control->check();
                if (matches[i] < 0 || basematches[i] < 0)
                  throw si::Error("Unmatched logical channel");
                const si::Channel *otherAggressor = nullptr;
                if (aggressor) {
                  auto agMatches =
                      si::matchChannels({*aggressor}, other.channels);
                  if (agMatches[0] < 0)
                    throw si::Error("Unmatched logical aggressor");
                  otherAggressor = &other.channels[size_t(agMatches[0])];
                }
                auto a = si::crop(
                    si::loadTrace(*base.cache,
                                  base.channels[size_t(basematches[i])], metric,
                                  set, aggressor, control),
                    set.startHz, set.stopHz);
                auto b = si::crop(
                    si::loadTrace(*other.cache,
                                  other.channels[size_t(matches[i])], metric,
                                  set, otherAggressor, control),
                    set.startHz, set.stopHz);
                auto delta = si::compare(a, b, metric);
                const double lo = delta.frequency.front(),
                             hi = delta.frequency.back();
                auto ar =
                         si::analyze(si::crop(a, lo, hi), metric, set, control),
                     br =
                         si::analyze(si::crop(b, lo, hi), metric, set, control);
                const double raw = br.worst - ar.worst,
                             imp = metric == si::Metric::IL ? raw : -raw;
                rows->push_back({channel, base.name, other.name, ar.worst,
                                 br.worst, raw, imp, delta.worstImprovement,
                                 imp > 0   ? "Candidate improved"
                                 : imp < 0 ? "Baseline better"
                                           : "Equal",
                                 QString("%1 – %2 GHz")
                                     .arg(num(lo * 1e-9))
                                     .arg(num(hi * 1e-9))});
              } catch (const si::Cancelled &) {
                throw;
              } catch (const std::exception &e) {
                rows->push_back({channel, base.name, other.name, "N/A", "N/A",
                                 "N/A", "N/A", "N/A", qs(e.what())});
              }
            }
          }
        }
      },
      [this, rows] {
        compareTable->setColumnCount(10);
        compareTable->setHorizontalHeaderLabels(
            {"Channel / Aggressor", "Baseline", "Candidate", "Base Worst",
             "Candidate Worst", "Raw Δ dB", "Improvement dB",
             "Min pointwise improvement", "Best / Note", "Common band"});
        compareTable->setRowCount(int(rows->size()));
        for (size_t i = 0; i < rows->size(); ++i)
          for (int j = 0; j < (*rows)[i].size(); ++j) {
            auto v = (*rows)[i][j];
            compareTable->setItem(
                int(i), j,
                new QTableWidgetItem(v.metaType().id() == QMetaType::Double
                                         ? num(v.toDouble())
                                         : v.toString()));
          }
        compareTable->resizeColumnsToContents();
        // The comparison tab must be visible when invoked from results focus.
        if (workspaceMode == 2) setWorkspaceMode(0);
        tabs->setCurrentIndex(1);
      });
}
void MainWindow::loadDemo() {
  if (busy)
    return;
  QString base =
      QCoreApplication::applicationDirPath() + "/examples/demo.siproject";
  if (!QFileInfo::exists(base))
    base = QDir::currentPath() + "/examples/demo.siproject";
  if (!QFileInfo::exists(base)) {
    error("Cannot find examples/demo.siproject.");
    return;
  }
  loadProject(base);
}
void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasUrls() && !busy)
    e->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent *e) {
  QStringList files;
  for (auto u : e->mimeData()->urls())
    if (u.isLocalFile())
      files << u.toLocalFile();
  openPaths(files);
}
void MainWindow::closeEvent(QCloseEvent *e) {
  if (busy) {
    if (control)
      control->cancelled = true;
    e->ignore();
    notice->setText("Cancelling the active task. Close the window after cancellation completes.");
    return;
  }
  if (dirty && !revisions.empty()) {
    auto choice = QMessageBox::question(
        this, "Save Project", "Save the modified settings and channel mapping?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Cancel) {
      e->ignore();
      return;
    }
    if (choice == QMessageBox::Save) {
      saveProject();
      if (dirty) {
        e->ignore();
        return;
      }
    }
  }
  saveWorkspaceLayout();
  e->accept();
}
