void MainWindow::openPaths(QStringList paths) {
  if (paths.isEmpty() || busy)
    return;
  if (paths.size() == 1 &&
      paths[0].endsWith(".siproject", Qt::CaseInsensitive)) {
    loadProject(paths[0]);
    return;
  }
  auto loaded = std::make_shared<std::vector<Revision>>();
  QString cacheRoot =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/v3";
  task(
      [paths, loaded, cacheRoot](si::Control *c) {
        for (auto &p : paths) {
          Revision r;
          r.source = QFileInfo(p).absoluteFilePath();
          r.name = QFileInfo(p).completeBaseName();
          r.cache = si::Cache::open(fp(r.source), fp(cacheRoot), c);
          try {
            std::string mappingDiagnostic;
            auto proposed = si::suggestMapping(r.cache->meta(), &mappingDiagnostic);
            const auto &meta = r.cache->meta();
            const bool explicitPowerSI = !meta.differentialHints.empty();
            const bool explicitMixedMode = !meta.mixedOrder.empty();
            const bool proposedDifferential = hasDifferentialChannels(proposed);
            const bool mixedVerified = explicitMixedMode &&
                                       mixedModeConfirmsDifferentialPairs(meta, proposed);

            if (!mappingDiagnostic.empty()) {
              r.candidates = std::move(proposed);
              r.mappingWarning = qs(mappingDiagnostic);
              r.mappingSource = "Auto Mapping Failed / Manual Mapping";
            } else if (explicitPowerSI && proposedDifferential) {
              r.channels = std::move(proposed);
              r.confirmed = true;
              r.mappingSource = "PowerSI .DiffChannels";
            } else if (explicitPowerSI) {
              r.candidates = std::move(proposed);
              r.mappingWarning =
                  "PowerSI .DiffChannels was present but did not produce a validated differential mapping.";
              r.mappingSource = "PowerSI .DiffChannels / Manual Mapping";
            } else if (explicitMixedMode) {
              r.candidates = std::move(proposed);
              r.mappingWarning = mixedVerified
                  ? "Touchstone [Mixed-Mode Order] confirms the differential P/N pairs, but it does not establish which endpoint is Near versus Far. Confirm Near/Far manually."
                  : "Touchstone mixed-mode data is valid, but P/N plus Near/Far channel linkage could not be proven from metadata and labels. Confirm it manually.";
              r.mappingSource = mixedVerified
                  ? "Touchstone [Mixed-Mode Order] / Direction Review"
                  : "Mixed-Mode / Manual Mapping";
            } else if (proposedDifferential) {
              r.candidates = std::move(proposed);
              r.mappingWarning =
                  "P/N-like names were found, but naming alone does not prove a differential channel. Review the candidate and confirm P/N and Near/Far manually.";
              r.mappingSource = "Name-based Differential Candidate";
            } else if (si::requiresManualDirectionConfirmation(meta, proposed)) {
              r.candidates = std::move(proposed);
              r.mappingWarning =
                  "Single-ended labels identify matching endpoints, but Near/Far direction is inferred from source order. Confirm Near/Far manually.";
              r.mappingSource = "Single-ended Direction Candidate";
            } else {
              r.channels = std::move(proposed);
              r.confirmed = !r.channels.empty();
              r.mappingSource = r.confirmed ? "Single-ended Reflection Ports" : "Manual Mapping";
              if (!r.confirmed)
                r.mappingWarning = "No unambiguous channel mapping was found. Create or import a mapping manually.";
            }
          } catch (const si::Cancelled &) {
            throw;
          } catch (const si::Error &e) {
            // Valid data remains accessible; do not invent fallback polarity.
            r.mappingWarning = qs(e.what());
            r.mappingSource = "Auto Mapping Failed / Manual Mapping";
            r.channels.clear();
            r.candidates.clear();
          }
          loaded->push_back(r);
        }
      },
      [this, loaded] {
        for (auto r : *loaded) {
          QString base = r.name;
          int n = 2;
          while (std::any_of(revisions.begin(), revisions.end(),
                             [&](auto &a) { return a.name == r.name; }))
            r.name = base + " (" + QString::number(n++) + ")";
          revisions.push_back(r);
          revisionBox->addItem(r.name);
        }
        revisionBox->setCurrentIndex(int(revisions.size()) - 1);
        if (projectName == "Untitled")
          projectName = revisions.front().name;
        dirty = true;
        auto &current = revisions.back();
        if (current.confirmed) {
          rebuildChannels();
          notice->setText("Touchstone data loaded. Mapping validated from " +
                          current.mappingSource + ". Select channels and run analysis.");
        } else {
          notice->setText("Touchstone data loaded. Mapping review is required before analysis.");
          editMapping();
        }
      });
}
void MainWindow::rebuildChannels() {
  QSignalBlocker block(groupBox);
  channels->clear();
  groupBox->clear();
  groupBox->addItem("All groups");
  aggressorBox->clear();
  aggressorBox->addItem("AUTO (same group)");
  int index = revisionBox->currentIndex();
  if (index < 0 || size_t(index) >= revisions.size())
    return;
  auto &r = revisions[size_t(index)];
  std::set<std::string> groups;
  for (auto &c : r.channels) {
    auto item = new QListWidgetItem(qs(c.name), channels);
    item->setData(Qt::UserRole, qs(c.id));
    item->setData(Qt::UserRole + 1, qs(c.group));
    item->setCheckState(Qt::Checked);
    item->setToolTip(qs(c.group) + (c.differential() ? " · Differential"
                                                     : " · Single-ended"));
    groups.insert(c.group);
    aggressorBox->addItem(qs(c.name), qs(c.id));
  }
  for (auto &g : groups)
    groupBox->addItem(qs(g));
  auto &m = r.cache->meta();
  selectionInfo->setText(
      QString("%1 ports · %2 points\n%3 · %4\nSource: %5\nMapping: %6")
          .arg(m.ports)
          .arg(m.points)
          .arg(qs(m.version))
          .arg(r.cache->reused() ? "Cache reused" : "Imported")
          .arg(r.mappingSource.isEmpty() ? "Unknown" : r.mappingSource)
          .arg(r.confirmed ? "Confirmed" : "Review required"));
}
std::vector<si::Channel> MainWindow::selectedChannels() const {
  std::vector<si::Channel> out;
  int index = revisionBox->currentIndex();
  if (index < 0)
    return out;
  for (int i = 0; i < channels->count(); ++i) {
    auto item = channels->item(i);
    if (item->isHidden() || item->checkState() != Qt::Checked)
      continue;
    for (auto &c : revisions[size_t(index)].channels)
      if (qs(c.id) == item->data(Qt::UserRole).toString())
        out.push_back(c);
  }
  return out;
}
std::string MainWindow::activeRevisionName() const {
  const int index = revisionBox ? revisionBox->currentIndex() : -1;
  if (index < 0 || size_t(index) >= revisions.size()) return {};
  return ss(revisions[size_t(index)].name);
}
bool MainWindow::hasCurrentResult(si::Metric metric) const {
  const auto revision = activeRevisionName();
  return !revision.empty() &&
         std::any_of(results.begin(), results.end(), [&](const si::Result &r) {
           return r.metric == metric && r.revision == revision;
         });
}
void MainWindow::invalidateRevisionResults(const QString &revision) {
  const auto name = ss(revision);
  std::erase_if(results, [&](const si::Result &r) { return r.revision == name; });
  std::erase_if(lastJobs, [&](const si::Job &j) { return j.revision == name; });
  if (!busy) { refreshResults(); refreshPlot(); }
}
void MainWindow::syncSettings() {
  settings.startHz = start->value() * 1e9;
  settings.stopHz = stop->value() == 0 ? std::numeric_limits<double>::infinity()
                                       : stop->value() * 1e9;
  settings.reverse = directionMode == 1;
  settings.markers = markerBox->currentIndex() == 0   ? 1
                     : markerBox->currentIndex() == 1 ? 3
                     : markerBox->currentIndex() == 2 ? 5
                                                      : 0;
  settings.performance = performanceBox->currentIndex();
  settings.targetOhm = target->value();
  settings.tdrLimitEnabled = tdrLimit->isChecked();
  settings.tdrReflection=tdrReflection->isChecked();
  settings.tolerancePercent = tolerance->value();
  settings.tdrStartSeconds = timeStart->value() * 1e-9;
  settings.tdrStopSeconds = timeStop->value() == 0
                                ? std::numeric_limits<double>::infinity()
                                : timeStop->value() * 1e-9;
  settings.prominence = peakProminence->value();
  int i = 0;
  for (auto *l : {&settings.rl, &settings.il, &settings.next, &settings.fext}) {
    l->enabled = limits[i]->isChecked();
    l->constant = limitValues[i]->value();
    ++i;
  }
  settings.manualMarkersHz.clear();
  for (auto v : manualMarkers->text().split(',', Qt::SkipEmptyParts)) {
    bool ok;
    double f = v.trimmed().toDouble(&ok);
    if (!ok || f < 0 || !std::isfinite(f))
      throw si::Error("Enter marker frequencies in GHz, separated by commas.");
    settings.manualMarkersHz.push_back(f * 1e9);
  }
  settings.quick.clear();
  for (int j = 0; j < 5; ++j)
    if (quickChecks[j]->isChecked())
      settings.quick.push_back(si::Metric(j));
}
void MainWindow::applySettings() {
  start->setValue(settings.startHz * 1e-9);
  stop->setValue(std::isfinite(settings.stopHz) ? settings.stopHz * 1e-9 : 0);
  markerBox->setCurrentIndex(settings.markers == 1   ? 0
                             : settings.markers == 3 ? 1
                             : settings.markers == 5 ? 2
                                                     : 3);
  performanceBox->setCurrentIndex(settings.performance);
  target->setValue(settings.targetOhm);
  tdrLimit->setChecked(settings.tdrLimitEnabled);
  tdrReflection->setChecked(settings.tdrReflection);
  QStringList labels;
  for(auto t:settings.tdrTerminations)labels<<qs(si::shortName(t,true));
  terminationSummary->setText(labels.join(" + "));
  terminationSummary->setToolTip("Apply the termination at the opposite end of the selected channel. All other physical ports are matched to their source reference impedances.");
  tolerance->setValue(settings.tolerancePercent);
  timeStart->setValue(settings.tdrStartSeconds * 1e9);
  timeStop->setValue(std::isfinite(settings.tdrStopSeconds)
                         ? settings.tdrStopSeconds * 1e9
                         : 0);
  peakProminence->setValue(settings.prominence);
  int i = 0;
  for (auto *l : {&settings.rl, &settings.il, &settings.next, &settings.fext}) {
    limits[i]->setChecked(l->enabled);
    limitValues[i]->setValue(l->constant);
    ++i;
  }
  QStringList markers;
  for (double f : settings.manualMarkersHz)
    markers << num(f * 1e-9, 6);
  manualMarkers->setText(markers.join(", "));
  for (int j = 0; j < 5; ++j)
    quickChecks[j]->setChecked(std::find(settings.quick.begin(),
                                         settings.quick.end(), si::Metric(j)) !=
                               settings.quick.end());
}

void MainWindow::chooseTerminations() {
  if(busy)return;
  try{syncSettings();}catch(const std::exception &e){error(qs(e.what()));return;}
  QDialog dialog(this);dialog.setWindowTitle("TDR Options");
  auto layout=new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel("Select multiple conditions to overlay TDR traces.\nForward: terminate Far / Reverse: terminate Near\nAll other physical ports are matched to their source reference impedances."));
  QCheckBox *choices[5];
  QStringList labels{"Reference: matched to source reference impedance",
    "Resistive: SE 50 Ω / Diff P–N 100 Ω (floating)",
    "Grounded: SE 50 Ω / Diff P–GND 50 Ω + N–GND 50 Ω",
    "Open: SE open / Diff P and N both open",
    "Short: SE–GND short / Diff P–N short (floating)"};
  for(int i=0;i<5;++i) {
    choices[i]=new QCheckBox(labels[i]);layout->addWidget(choices[i]);
    choices[i]->setChecked(std::find(settings.tdrTerminations.begin(),settings.tdrTerminations.end(),si::Termination(i))!=settings.tdrTerminations.end());
  }
  layout->addWidget(new QLabel("The two SE 50 Ω options are equivalent and are calculated once.\nReflection coefficient ρ is recommended for open circuits. These are ideal resistive/open/short conditions."));
  auto reflection = new QCheckBox("Show Reflection Coefficient ρ"); reflection->setChecked(settings.tdrReflection); reflection->setToolTip("Useful for open/short comparisons. Impedance tolerance limits do not apply to ρ."); layout->addWidget(reflection);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    bool any=false;for(auto c:choices)any|=c->isChecked();
    if(!any){QMessageBox::information(&dialog,"Select Terminations","Select at least one termination condition.");return;}dialog.accept();
  });
  if(dialog.exec()!=QDialog::Accepted)return;
  settings.tdrTerminations.clear();
  for(int i=0;i<5;++i)if(choices[i]->isChecked())settings.tdrTerminations.push_back(si::Termination(i));
  settings.tdrReflection = reflection->isChecked(); tdrReflection->setChecked(settings.tdrReflection);
  dirty=true;applySettings();notice->setText("TDR options changed. Run TDR or Quick Analysis to update results.");
}

void MainWindow::editMapping() {
  int index = revisionBox->currentIndex();
  if (index < 0 || busy)
    return;
  auto &rev = revisions[size_t(index)];
  QDialog dialog(this);
  dialog.setWindowTitle("Channel Mapping — " + rev.name);
  dialog.setObjectName("channelMappingDialog");
  dialog.resize(980, 540);
  auto layout = new QVBoxLayout(&dialog);
  auto info = new QLabel(
      QString("Source: %1\nPort numbering starts at 1. Leave Near−/Far− blank for single-ended channels.\nVerify Near/Far and P/N against the physical connections. A blank Far end supports reflection/TDR only.")
          .arg(rev.mappingSource.isEmpty() ? "Unknown" : rev.mappingSource));
  info->setWordWrap(true);
  layout->addWidget(info);
  if (!rev.mappingWarning.isEmpty()) {
    auto warning = new QLabel(
        "File data loaded, but automatic mapping could not be confirmed. Check port labels and enter the mapping manually or import a Mapping CSV.\n" +
        rev.mappingWarning);
    warning->setObjectName("mappingWarning");
    warning->setWordWrap(true);
    layout->addWidget(warning);
  }
  auto grid = new QTableWidget;
  grid->setColumnCount(8);
  grid->setHorizontalHeaderLabels({"Channel", "Near+", "Near−", "Far+", "Far−",
                                   "Group", "Alias", "Mapping ID"});
  grid->horizontalHeader()->setStretchLastSection(true);
  grid->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(grid, 1);
  auto fill = [&](const std::vector<si::Channel> &channels) {
    grid->setRowCount(int(channels.size()));
    for (size_t i = 0; i < channels.size(); ++i) {
      auto &c = channels[i];
      QStringList values{qs(c.name),
                         c.nearP >= 0 ? QString::number(c.nearP + 1) : "",
                         c.nearN >= 0 ? QString::number(c.nearN + 1) : "",
                         c.farP >= 0 ? QString::number(c.farP + 1) : "",
                         c.farN >= 0 ? QString::number(c.farN + 1) : "",
                         qs(c.group),
                         qs(c.alias),
                         qs(c.id)};
      for (int j = 0; j < values.size(); ++j)
        grid->setItem(int(i), j, new QTableWidgetItem(values[j]));
    }
  };
  if (!rev.channels.empty())
    fill(rev.channels);
  else
    fill(rev.candidates);
  auto actions = new QHBoxLayout;
  button("Add Row", actions, [&] {
    int i = grid->rowCount();
    grid->insertRow(i);
    grid->setItem(i, 0, new QTableWidgetItem("CH" + QString::number(i + 1)));
    grid->setItem(i, 5, new QTableWidgetItem("Default"));
    grid->setItem(i, 7,
                  new QTableWidgetItem(
                      QUuid::createUuid().toString(QUuid::WithoutBraces)));
  });
  button("Delete Selected Rows", actions, [&] {
    auto rows = grid->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(),
              [](auto a, auto b) { return a.row() > b.row(); });
    for (auto r : rows)
      grid->removeRow(r.row());
  });
  button("Suggest from Labels", actions, [&] {
    try {
      std::string diagnostic;
      auto proposed = si::suggestMapping(rev.cache->meta(), &diagnostic);
      if (!diagnostic.empty()) {
        error(qs(diagnostic)); // Preserve manually entered rows on uncertain fallback.
        return;
      }
      fill(proposed);
    } catch (const std::exception &e) {
      error(qs(e.what())); // Preserve manually entered rows on failure.
    }
  });
  button("Suggest SE Half-Split", actions, [&] {
    std::vector<si::Channel> cs;
    auto n = rev.cache->meta().ports;
    if (n % 2) {
      error("An even number of ports is required.");
      return;
    }
    for (uint64_t i = 0; i < n / 2; ++i) {
      si::Channel c;
      c.id = ss(QUuid::createUuid().toString(QUuid::WithoutBraces));
      c.name = "CH" + std::to_string(i + 1);
      c.nearP = int(i);
      c.farP = int(i + n / 2);
      cs.push_back(c);
    }
    fill(cs);
  });
  layout->addLayout(actions);

  auto manualGroup = new QGroupBox("Manual Differential Pairing");
  auto manual = new QGridLayout(manualGroup);
  auto channelName = new QLineEdit;
  channelName->setPlaceholderText("Channel name");
  manual->addWidget(new QLabel("Channel"), 0, 0);
  manual->addWidget(channelName, 0, 1, 1, 3);
  QComboBox *manualPorts[4];
  QStringList portTitles{"Near P", "Near N", "Far P", "Far N"};
  for (int k = 0; k < 4; ++k) {
    manualPorts[k] = new QComboBox;
    manualPorts[k]->addItem("—", -1);
    for (size_t port = 0; port < rev.cache->meta().ports; ++port) {
      QString label = port < rev.cache->meta().labels.size()
                          ? qs(rev.cache->meta().labels[port])
                          : QString{};
      manualPorts[k]->addItem(
          QString("P%1  %2").arg(port + 1).arg(label), int(port));
    }
    manual->addWidget(new QLabel(portTitles[k]), 1, k);
    manual->addWidget(manualPorts[k], 2, k);
  }
  auto manualActions = new QHBoxLayout;
  button("Create Differential Channel", manualActions, [&] {
    int ports[4];
    for (int k = 0; k < 4; ++k) ports[k] = manualPorts[k]->currentData().toInt();
    if (ports[0] < 0 || ports[1] < 0) {
      error("Select both Near P and Near N.");
      return;
    }
    if ((ports[2] < 0) != (ports[3] < 0)) {
      error("Select both Far P and Far N, or leave both blank.");
      return;
    }
    std::set<int> selected;
    for (int port : ports)
      if (port >= 0 && !selected.insert(port).second) {
        error("A physical port cannot be used twice in one differential channel.");
        return;
      }
    int row = grid->rowCount();
    grid->insertRow(row);
    QString name = channelName->text().trimmed();
    if (name.isEmpty()) name = "DIFF" + QString::number(row + 1);
    QStringList values{name,
                       QString::number(ports[0] + 1),
                       QString::number(ports[1] + 1),
                       ports[2] >= 0 ? QString::number(ports[2] + 1) : QString{},
                       ports[3] >= 0 ? QString::number(ports[3] + 1) : QString{},
                       "Differential", "",
                       QUuid::createUuid().toString(QUuid::WithoutBraces)};
    for (int col = 0; col < values.size(); ++col)
      grid->setItem(row, col, new QTableWidgetItem(values[col]));
    grid->selectRow(row);
  });
  auto selectedRows = [&] {
    std::vector<int> rows;
    for (const auto &index : grid->selectionModel()->selectedRows())
      rows.push_back(index.row());
    if (rows.empty() && grid->currentRow() >= 0) rows.push_back(grid->currentRow());
    return rows;
  };
  auto swapCells = [&](int row, int a, int b) {
    QString left = grid->item(row, a) ? grid->item(row, a)->text() : QString{};
    QString right = grid->item(row, b) ? grid->item(row, b)->text() : QString{};
    if (!grid->item(row, a)) grid->setItem(row, a, new QTableWidgetItem);
    if (!grid->item(row, b)) grid->setItem(row, b, new QTableWidgetItem);
    grid->item(row, a)->setText(right);
    grid->item(row, b)->setText(left);
  };
  button("Swap P/N", manualActions, [&] {
    auto rows = selectedRows();
    if (rows.empty()) { error("Select a mapping row first."); return; }
    for (int row : rows) { swapCells(row, 1, 2); swapCells(row, 3, 4); }
  });
  button("Swap Near/Far", manualActions, [&] {
    auto rows = selectedRows();
    if (rows.empty()) { error("Select a mapping row first."); return; }
    for (int row : rows) { swapCells(row, 1, 3); swapCells(row, 2, 4); }
  });
  manual->addLayout(manualActions, 3, 0, 1, 4);
  layout->addWidget(manualGroup);
  auto portLabels = new QComboBox;
  portLabels->addItem("Physical Ports / Labels");
  for (size_t i = 0; i < rev.cache->meta().labels.size(); ++i)
    portLabels->addItem("P" + QString::number(i + 1) + "  " +
                        qs(rev.cache->meta().labels[i]));
  layout->addWidget(portLabels);
  auto bottom =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  layout->addWidget(bottom);
  connect(bottom, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(bottom, &QDialogButtonBox::accepted, &dialog, [&] {
    try {
      std::vector<si::Channel> cs;
      for (int i = 0; i < grid->rowCount(); ++i) {
        auto cell = [&](int col) {
          return grid->item(i, col) ? grid->item(i, col)->text().trimmed()
                                    : QString{};
        };
        auto port = [&](int col) {
          auto text = cell(col);
          if (text.isEmpty())
            return -1;
          bool ok;
          int value = text.toInt(&ok);
          if (!ok || value < 1)
            throw si::Error("Port numbers must be integers greater than or equal to 1.");
          return value - 1;
        };
        si::Channel c;
        c.name = ss(cell(0));
        c.nearP = port(1);
        c.nearN = port(2);
        c.farP = port(3);
        c.farN = port(4);
        c.group = ss(cell(5));
        if (c.group.empty())
          c.group = "Default";
        c.alias = ss(cell(6));
        c.id = ss(cell(7));
        if (c.id.empty())
          c.id = ss(QUuid::createUuid().toString(QUuid::WithoutBraces));
        cs.push_back(c);
      }
      if (cs.empty())
        throw si::Error("Define at least one channel.");
      si::validateMapping(cs, rev.cache->meta());
      rev.channels = std::move(cs);
      rev.candidates.clear();
      rev.confirmed = true;
      rev.mappingSource = "Manual Mapping";
      rev.mappingWarning.clear();
      dirty = true;
      dialog.accept();
    } catch (const std::exception &e) {
      error(qs(e.what()));
    }
  });
  if (dialog.exec() == QDialog::Accepted) {
    invalidateRevisionResults(rev.name);
    rebuildChannels();
    notice->setText("Mapping confirmed. Select channels and run analysis.");
  }
}
void MainWindow::importMapping() {
  int i = revisionBox->currentIndex();
  if (i < 0 || busy)
    return;
  auto path =
      QFileDialog::getOpenFileName(this, "Mapping CSV", {}, "CSV (*.csv)");
  if (path.isEmpty())
    return;
  try {
    auto cs = si::readMappingCsv(fp(path));
    si::validateMapping(cs, revisions[size_t(i)].cache->meta());
    revisions[size_t(i)].channels = std::move(cs);
    revisions[size_t(i)].candidates.clear();
    revisions[size_t(i)].confirmed = true;
    revisions[size_t(i)].mappingSource = "Mapping CSV";
    revisions[size_t(i)].mappingWarning.clear();
    dirty = true;
    invalidateRevisionResults(revisions[size_t(i)].name);
    rebuildChannels();
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
void MainWindow::exportMapping() {
  int i = revisionBox->currentIndex();
  if (i < 0 || busy)
    return;
  auto path =
      withExtension(QFileDialog::getSaveFileName(this, "Mapping CSV",
                                                 "mapping.csv", "CSV (*.csv)"),
                    ".csv");
  if (path.isEmpty())
    return;
  try {
    si::writeMappingCsv(fp(path), revisions[size_t(i)].channels);
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
