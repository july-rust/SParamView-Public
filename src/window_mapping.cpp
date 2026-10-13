static QString mixedDescriptorType(const si::Metadata &meta) {
  bool d=false,s=false;
  for (const auto &token:meta.mixedOrder) {
    if (token.empty()) continue;
    const char c=char(std::tolower(static_cast<unsigned char>(token[0])));
    d |= c=='d'||c=='c'; s |= c=='s';
  }
  return d ? (s?"Mixed":"Differential") : (s?"Single-ended":"Unknown");
}
static int unmappedPorts(const Revision &r) {
  std::set<int> used;
  const auto &mapped=r.channels.empty()?r.candidates:r.channels;
  for(const auto &c:mapped)for(int p:{c.nearP,c.nearN,c.farP,c.farN})if(p>=0)used.insert(p);
  return int(r.cache->meta().ports)-int(used.size());
}
static void detectRevision(Revision &r) {
          try {
            std::string mappingDiagnostic;
            auto proposed = si::suggestMapping(r.cache->meta(), &mappingDiagnostic);
            const auto &meta = r.cache->meta();
            const bool explicitPowerSI = !meta.differentialHints.empty();
            const bool explicitMixedMode = !meta.mixedOrder.empty();
            const bool proposedDifferential = hasDifferentialChannels(proposed);
            const bool mixedVerified = explicitMixedMode &&
                                       mixedModeConfirmsDifferentialPairs(meta, proposed);

            const QString proposedType = signalTypeForChannels(proposed);
            if (!mappingDiagnostic.empty() && explicitPowerSI) {
              r.signalType = "Differential";
              r.signalConfidence = "High";
              r.signalEvidence = "Explicit .DiffChannels metadata is present, but the declared P/N polarity could not be validated.";
              r.signalTypeReviewed = true;
              r.candidates.clear();
              r.mappingWarning =
                  qs(mappingDiagnostic) +
                  " Explicit differential metadata takes priority, so the compatibility single-ended fallback was not accepted. Confirm P/N and Near/Far manually.";
              r.mappingSource = "PowerSI .DiffChannels / P/N Review";
            } else if (!mappingDiagnostic.empty() && explicitMixedMode) {
              r.signalType = "Differential";
              r.signalConfidence = "High";
              r.signalEvidence = "Touchstone [Mixed-Mode Order] declares differential data, but physical P/N linkage could not be validated.";
              r.signalTypeReviewed = true;
              r.candidates.clear();
              r.mappingWarning =
                  qs(mappingDiagnostic) +
                  " Explicit mixed-mode metadata takes priority, so a single-ended fallback was not accepted. Confirm P/N and Near/Far manually.";
              r.mappingSource = "Mixed-Mode / P/N Review";
            } else if (!mappingDiagnostic.empty()) {
              r.candidates = std::move(proposed);
              r.signalType = signalTypeForChannels(r.candidates);
              r.signalConfidence = "Low";
              r.signalEvidence = "Automatic mapping returned a diagnostic and requires review.";
              r.mappingWarning = qs(mappingDiagnostic);
              r.mappingSource = "Auto Mapping Failed / Manual Mapping";
            } else if (explicitPowerSI && proposedDifferential) {
              r.channels = std::move(proposed);
              r.confirmed = true;
              r.signalType = signalTypeForChannels(r.channels);
              r.signalConfidence = "High";
              r.signalEvidence = "Validated .DiffChannels metadata and physical port labels.";
              r.signalTypeReviewed = true;
              r.mappingSource = "PowerSI .DiffChannels";
            } else if (explicitPowerSI) {
              r.signalType = "Differential";
              r.signalConfidence = "High";
              r.signalEvidence = "Explicit .DiffChannels metadata declares a differential channel.";
              r.signalTypeReviewed = true;
              r.candidates.clear();
              r.mappingWarning =
                  "PowerSI .DiffChannels was present but did not produce a validated differential P/N mapping. Single-ended fallback was intentionally suppressed; confirm P/N and Near/Far manually.";
              r.mappingSource = "PowerSI .DiffChannels / Manual Mapping";
            } else if (explicitMixedMode) {
              r.signalType = proposedDifferential ? proposedType : "Differential";
              r.signalConfidence = "High";
              r.signalEvidence = "Touchstone [Mixed-Mode Order] is explicit differential metadata.";
              r.signalTypeReviewed = true;
              if (proposedDifferential)
                r.candidates = std::move(proposed);
              else
                r.candidates.clear();
              r.mappingWarning = mixedVerified
                  ? "Touchstone [Mixed-Mode Order] confirms the differential P/N pairs, but it does not establish which endpoint is Near versus Far. Confirm Near/Far manually."
                  : "Touchstone mixed-mode metadata declares differential data, but P/N plus Near/Far channel linkage could not be proven. Single-ended fallback was not accepted; confirm it manually.";
              r.mappingSource = mixedVerified
                  ? "Touchstone [Mixed-Mode Order] / Direction Review"
                  : "Mixed-Mode / Manual Mapping";
            } else if (proposedDifferential) {
              r.candidates = std::move(proposed);
              r.signalType = signalTypeForChannels(r.candidates);
              r.signalConfidence = "Medium";
              r.signalEvidence =
                  "Regular repeated port labels differ by exactly one P/N character; no explicit differential metadata is present.";
              r.mappingWarning =
                  "Regular P/N naming was found, but naming alone is evidence rather than an explicit declaration. Confirm the detected signal type before Channel Mapping.";
              r.mappingSource = "Name-based Differential Candidate";
            } else if (si::requiresManualDirectionConfirmation(meta, proposed)) {
              r.candidates = std::move(proposed);
              r.signalType = "Single-ended";
              r.signalConfidence = "Medium";
              r.signalEvidence = "Matching endpoint names were found without explicit differential metadata.";
              r.mappingWarning =
                  "Single-ended labels identify matching endpoints, but Near/Far direction is inferred from source order. Confirm the signal type and Near/Far manually.";
              r.mappingSource = "Single-ended Direction Candidate";
            } else {
              r.channels = std::move(proposed);
              r.confirmed = !r.channels.empty();
              r.signalType = r.confirmed ? "Single-ended" : "Unknown";
              r.signalConfidence = r.confirmed ? "High" : "Low";
              r.signalEvidence = r.confirmed
                  ? "Direction-free reflection ports require no inferred Near/Far transmission direction."
                  : "No unambiguous signal-type evidence was found.";
              r.signalTypeReviewed = r.confirmed;
              r.mappingSource = r.confirmed ? "Single-ended Reflection Ports" : "Manual Mapping";
              if (!r.confirmed)
                r.mappingWarning = "No unambiguous channel mapping was found. Confirm the signal type, then create or import a mapping manually.";
            }
          } catch (const si::Cancelled &) {
            throw;
          } catch (const si::Error &e) {
            // Valid data remains accessible; do not invent fallback polarity.
            r.mappingWarning = qs(e.what());
            r.mappingSource = "Auto Mapping Failed / Manual Mapping";
            r.channels.clear();
            r.candidates.clear();
            const auto &meta = r.cache->meta();
            if (!meta.differentialHints.empty() || !meta.mixedOrder.empty()) {
              r.signalType = "Differential";
              r.signalConfidence = "High";
              r.signalEvidence = "Explicit differential metadata is present, but automatic physical mapping failed.";
              r.signalTypeReviewed = true;
            } else {
              r.signalType = "Unknown";
              r.signalConfidence = "Low";
              r.signalEvidence = "Automatic mapping failed without explicit differential metadata.";
            }
          }

  const auto &meta=r.cache->meta();
  if (!meta.mixedOrder.empty()) {
    r.signalType=mixedDescriptorType(meta);
    r.signalConfidence="High";r.signalTypeReviewed=true;
    r.signalEvidence="Touchstone [Mixed-Mode Order] S/D/C descriptors declare " + r.signalType + ". P/N membership and Near/Far direction are separate.";
    auto &mapped=r.channels.empty()?r.candidates:r.channels;
    if (r.signalType=="Single-ended") {
      std::string diagnostic;
      r.inferNamedDifferential=false;
      try { r.candidates=si::suggestMapping(meta,&diagnostic,false); }
      catch(const si::Error &e) { r.candidates.clear();diagnostic=e.what(); }
      if(hasDifferentialChannels(r.candidates)) {
        r.candidates.clear();
        diagnostic="Conflicting .DiffChannels and S-only descriptors require manual mapping.";
      }
      r.channels.clear();r.confirmed=false;
      r.mappingSource="Touchstone S descriptors / Direction Review";
      r.mappingWarning="S descriptors declare single-ended data. Confirm the physical mapping and Near/Far direction. " + qs(diagnostic);
    } else if (!mixedModeConfirmsDifferentialPairs(meta,mapped)) {
      r.channels.clear();r.candidates.clear();r.confirmed=false;
      r.mappingSource="Mixed-Mode / P/N Review";
      r.mappingWarning="Name-derived physical mapping does not match all declared D/C pairs. Enter P/N and Near/Far manually.";
    } else {
      if(!r.channels.empty()){r.candidates=std::move(r.channels);r.channels.clear();}
      r.confirmed=false;
    }
  }
}

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
          detectRevision(r);
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
  QScopedValueRollback<bool> restoring(restoringProject,true);
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
      QString("%1 ports · %2 points\n%3 · %4\nSignal: %5 (%6)\nSource: %7\nMapping: %8")
          .arg(m.ports)
          .arg(m.points)
          .arg(qs(m.version))
          .arg(r.cache->reused() ? "Cache reused" : "Imported")
          .arg(r.signalType)
          .arg(r.signalConfidence)
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
  try {
    const auto requested = requestedJobs({metric});
    auto sameChannel = [](const si::Channel &a, const si::Channel &b) {
      return channelJson(a) == channelJson(b);
    };
    auto sameJob = [&](const si::Job &a, const si::Job &b) {
      return a.cache == b.cache && a.cache->meta().sha256 == b.cache->meta().sha256 &&
             a.revision == b.revision && a.metric == b.metric &&
             a.reverse == b.reverse && a.termination == b.termination &&
             sameChannel(a.victim,b.victim) && bool(a.aggressor)==bool(b.aggressor) &&
             (!a.aggressor || sameChannel(*a.aggressor,*b.aggressor));
    };
    // Other metrics may coexist after Quick Analysis. Compare the complete
    // requested metric job set, not merely one result for the active revision.
    if (requested.empty() || size_t(std::count_if(lastJobs.begin(),lastJobs.end(),
          [&](const si::Job &j){return j.metric==metric;})) != requested.size()) return false;
    for (const auto &job : requested) {
      if (std::none_of(lastJobs.begin(),lastJobs.end(),[&](const auto &j){return sameJob(job,j);})) return false;
      if (std::none_of(results.begin(),results.end(),[&](const auto &r){
          return r.metric==job.metric && r.revision==job.revision &&
                 r.channelId==job.victim.id && r.aggressorId==(job.aggressor?job.aggressor->id:"") &&
                 r.termination==job.termination && r.reverse==job.reverse.value_or(settings.reverse);
          })) return false;
    }
    return true;
  } catch (const si::Error &) { return false; }
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
  QScopedValueRollback<bool> restoring(restoringProject,true);
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

bool MainWindow::reviewSignalType(Revision &rev) {
  if (rev.signalTypeReviewed || rev.confirmed)
    return true;
  QDialog dialog(this);
  dialog.setWindowTitle("Signal Type Detection — " + rev.name);
  dialog.setObjectName("signalTypeDetectionDialog");
  dialog.resize(560, 300);
  auto *layout = new QVBoxLayout(&dialog);
  auto *signalSummary = new QLabel(
      QString("<b>Detected:</b> %1 &nbsp;&nbsp; <b>Confidence:</b> %2")
          .arg(rev.signalType, rev.signalConfidence));
  layout->addWidget(signalSummary);
  auto *evidence = new QLabel(
      "<b>Evidence:</b> " + rev.signalEvidence +
      "<br><br>SParamView recognizes a name-based P/N pair only when two net names have the same length and differ at exactly one character, where that character is P versus N. The P/N position may be anywhere in the name. Irregular or ambiguous names are not guessed.");
  evidence->setWordWrap(true);
  layout->addWidget(evidence);
  layout->addWidget(new QLabel(
      "Confirm the electrical signal type before Channel Mapping. You can still edit every physical P/N and Near/Far assignment in the next step."));
  auto *selector = new QComboBox;
  selector->setObjectName("signalTypeSelector");
  selector->addItem("Select signal type…", "");
  selector->addItem("Differential", "Differential");
  selector->addItem("Single-ended", "Single-ended");
  selector->addItem("Mixed (Differential + Single-ended)", "Mixed");
  const int detected = selector->findData(rev.signalType);
  if (detected > 0)
    selector->setCurrentIndex(detected);
  layout->addWidget(selector);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText("Continue to Channel Mapping");
  buttons->button(QDialogButtonBox::Ok)->setEnabled(!selector->currentData().toString().isEmpty());
  connect(selector, &QComboBox::currentIndexChanged, &dialog, [=] {
    buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!selector->currentData().toString().isEmpty());
  });
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted)
    return false;

  const QString selected = selector->currentData().toString();
  rev.inferNamedDifferential = selected != "Single-ended";
  if (selected == "Single-ended") {
    std::string diagnostic;
    auto proposed = si::suggestMapping(rev.cache->meta(), &diagnostic, false);
    rev.candidates = std::move(proposed);
    if (!diagnostic.empty())
      rev.mappingWarning = qs(diagnostic);
    else
      rev.mappingWarning =
          "Signal type confirmed as Single-ended. Name-based P/N pairing is disabled for this revision; verify Near/Far in Channel Mapping.";
  } else if (selected == "Differential") {
    if (!hasDifferentialChannels(rev.candidates))
      rev.candidates.clear();
    rev.mappingWarning =
        "Signal type confirmed as Differential. Verify or enter the physical P/N and Near/Far mapping.";
  } else {
    rev.mappingWarning =
        "Signal type confirmed as Mixed. Verify both differential and single-ended channel assignments.";
  }
  rev.signalType = selected;
  rev.signalConfidence = "User Confirmed";
  rev.signalEvidence += " User confirmed the signal type before Channel Mapping.";
  rev.signalTypeReviewed = true;
  dirty = true;
  return true;
}

void MainWindow::editMapping() {
  int index = revisionBox->currentIndex();
  if (index < 0 || busy)
    return;
  auto &rev = revisions[size_t(index)];
  if (!reviewSignalType(rev)) {
    notice->setText("Signal type confirmation is required before Channel Mapping.");
    return;
  }
  QDialog dialog(this);
  dialog.setWindowTitle("Channel Mapping — " + rev.name);
  dialog.setObjectName("channelMappingDialog");
  dialog.resize(980, 540);
  auto layout = new QVBoxLayout(&dialog);
  auto info = new QLabel(
      QString("Signal Type: %1 (%2)\nEvidence: %3\nSource: %4\nPort numbering starts at 1. Leave Near−/Far− blank for single-ended channels.\nVerify Near/Far and P/N against the physical connections. A blank Far end supports reflection/TDR only.")
          .arg(rev.signalType)
          .arg(rev.signalConfidence)
          .arg(rev.signalEvidence)
          .arg(rev.mappingSource.isEmpty() ? "Unknown" : rev.mappingSource));
  info->setWordWrap(true);
  info->setText(info->text()+"\nUnmapped physical ports: "+QString::number(unmappedPorts(rev)));
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
      auto proposed = si::suggestMapping(rev.cache->meta(), &diagnostic, rev.inferNamedDifferential);
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
      const auto mappedType = signalTypeForChannels(cs);
      if (rev.signalTypeReviewed && rev.signalType != mappedType &&
          QMessageBox::question(&dialog, "Signal Type Changed",
              "The physical mapping is " + mappedType + ", but the reviewed signal type was " + rev.signalType + ". Accept this change?",
              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
      rev.channels = std::move(cs);
      rev.candidates.clear();
      rev.confirmed = true;
      rev.signalType = signalTypeForChannels(rev.channels);
      rev.signalConfidence = "User Confirmed";
      rev.signalEvidence = "User-confirmed physical Channel Mapping.";
      rev.signalTypeReviewed = true;
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
    const auto mappedType=signalTypeForChannels(cs);
    const auto &prior=revisions[size_t(i)];
    if(prior.signalTypeReviewed && prior.signalType!=mappedType &&
       QMessageBox::question(this,"Signal Type Changed",
         "Imported mapping is "+mappedType+", but the reviewed signal type was "+prior.signalType+". Accept this change?",
         QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    revisions[size_t(i)].channels = std::move(cs);
    revisions[size_t(i)].candidates.clear();
    revisions[size_t(i)].confirmed = true;
    revisions[size_t(i)].signalType =
        signalTypeForChannels(revisions[size_t(i)].channels);
    revisions[size_t(i)].signalConfidence = "User Confirmed";
    revisions[size_t(i)].signalEvidence = "Imported and validated Mapping CSV.";
    revisions[size_t(i)].signalTypeReviewed = true;
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
