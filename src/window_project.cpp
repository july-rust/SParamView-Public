void MainWindow::saveProject(bool as) {
  if (busy || revisions.empty())
    return;
  try {
    syncSettings();
    QString path = projectPath;
    if (path.isEmpty() || as)
      path =
          withExtension(QFileDialog::getSaveFileName(
                            this, "Save Project", projectName + ".siproject",
                            "SI Project (*.siproject)"),
                        ".siproject");
    if (path.isEmpty())
      return;
    QJsonArray sources;
    QDir base = QFileInfo(path).absoluteDir();
    for (auto &r : revisions) {
      QJsonArray revisionChannels, candidates;
      for (auto &c : r.channels)
        revisionChannels.append(channelJson(c));
      for (auto &c : r.candidates)
        candidates.append(channelJson(c));
      sources.append(QJsonObject{{"name", r.name},
                                 {"source", base.relativeFilePath(r.source)},
                                 {"absoluteSource", r.source},
                                 {"channels", revisionChannels},
                                 {"mappingCandidates", candidates},
                                 {"mappingSource", r.mappingSource},
                                 {"signalType", r.signalType},
                                 {"signalConfidence", r.signalConfidence},
                                 {"signalEvidence", r.signalEvidence},
                                 {"signalTypeReviewed", r.signalTypeReviewed},
                                 {"inferNamedDifferential", r.inferNamedDifferential},
                                 {"confirmed", r.confirmed},
                                 {"sourceSha256", qs(r.cache->meta().sha256)}});
    }
    QJsonArray selected;
    for (int i=0;i<channels->count();++i)
      if(channels->item(i)->checkState()==Qt::Checked)
        selected.append(channels->item(i)->data(Qt::UserRole).toString());
    QJsonObject obj{
        {"format", "SIAnalyzerProject"},
        {"schema", 1},
        {"programVersion", si::version},
        {"name", projectName},
        {"settings", settingsJson(settings)},
        {"revisions", sources},
        {"ui", QJsonObject{{"revision", revisionBox->currentIndex()},
                           {"selected", selected},
                           {"group", groupBox->currentText()},
                           {"search", search->text()},
                           {"aggressor", aggressorBox->currentText()},
                           {"metric", int(displayed)},
                           {"directionMode", directionMode},
                           {"geometry",
                            QString::fromLatin1(saveGeometry().toBase64())}}}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(obj).toJson()) < 0 || !file.commit())
      throw si::Error("Project save failed");
    projectPath = path;
    dirty = false;
    notice->setText("Project saved: " + QFileInfo(path).fileName());
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
void MainWindow::loadProject(QString path) {
  if (busy)
    return;
  try {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      throw si::Error("Cannot open project");
    if (file.size() > 16 * 1024 * 1024)
      throw si::Error("Project file is too large");
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError ||
        doc["format"].toString() != "SIAnalyzerProject" ||
        doc["schema"].toInt() != 1)
      throw si::Error("Invalid/unsupported project format");
    auto obj = doc.object();
    auto sources = obj["revisions"].toArray();
    auto loaded = std::make_shared<std::vector<Revision>>();
    QDir base = QFileInfo(path).absoluteDir();
    for (auto v : sources) {
      auto r = v.toObject();
      QString source = base.absoluteFilePath(r["source"].toString());
      if (!QFileInfo::exists(source))
        source = r["absoluteSource"].toString();
      if (!QFileInfo::exists(source)) {
        source = QFileDialog::getOpenFileName(
            this, "Locate Source File: " + r["name"].toString(), {},
            "Touchstone (*.s*p *.ts)");
        if (source.isEmpty())
          return;
      }
      Revision rev;
      rev.source = source;
      rev.name = r["name"].toString();
      for (auto c : r["channels"].toArray())
        rev.channels.push_back(channelFromJson(c.toObject()));
      for (auto c : r["mappingCandidates"].toArray())
        rev.candidates.push_back(channelFromJson(c.toObject()));
      rev.confirmed = r["confirmed"].toBool();
      rev.mappingSource = r["mappingSource"].toString(
          rev.confirmed ? "Legacy Confirmed Mapping" : "Manual Mapping");
      rev.signalType = r["signalType"].toString(
          rev.confirmed ? signalTypeForChannels(rev.channels) : "Unknown");
      rev.signalConfidence = r["signalConfidence"].toString(
          rev.confirmed ? "Confirmed" : "Low");
      rev.signalEvidence = r["signalEvidence"].toString(
          rev.confirmed ? "Restored confirmed Channel Mapping." : QString{});
      rev.signalTypeReviewed =
          r.contains("signalTypeReviewed") ? r["signalTypeReviewed"].toBool()
                                           : rev.confirmed;
      rev.inferNamedDifferential = r.contains("inferNamedDifferential")
          ? r["inferNamedDifferential"].toBool(true) : rev.signalType != "Single-ended";
      loaded->push_back(rev);
    }
    auto settingsLoaded = settingsFromJson(obj["settings"].toObject());
    // Invalid/missing sources and a cancelled relocation leave the old
    // workspace and its unsaved state untouched.
    if (!confirmProjectTransition()) return;
    auto staged = std::make_shared<QTemporaryDir>(
        QDir(cacheSession.path()).filePath("workspace-XXXXXX"));
    if (!staged->isValid())
      throw si::Error("Cannot create staged project cache");
    const QString cacheRoot = staged->path();
    task(
        [staged, loaded, cacheRoot, sources](si::Control *c) {
          for (size_t i = 0; i < loaded->size(); ++i) {
            auto &r = (*loaded)[i];
            r.cache = si::Cache::open(fp(r.source), fp(cacheRoot), c, true);
            const auto original=sources[int(i)].toObject()["sourceSha256"].toString();
            const bool changed=!original.isEmpty() && original!=qs(r.cache->meta().sha256);
            bool valid=true;
            try {si::validateMapping(r.channels,r.cache->meta());}
            catch(const si::Error &){valid=false;}
            if(changed || !valid) {
              r.channels.clear();r.candidates.clear();r.confirmed=false;
              r.signalTypeReviewed=false;r.inferNamedDifferential=true;
              detectRevision(r);
              if(!r.channels.empty()){r.candidates=std::move(r.channels);r.channels.clear();}
              r.confirmed=false;r.signalTypeReviewed=false;
              r.mappingWarning="Saved mapping invalidated: source changed or stored ports are no longer valid. " + r.mappingWarning;
              r.signalEvidence="Current source re-evaluated; previous mapping was discarded. " + r.signalEvidence;
            }
            r.sourceVerification=changed?"Changed; current source fully verified SHA-256":
                original.isEmpty()?"Current SHA-256 verified; legacy project has no saved hash":
                                   "Saved SHA-256 matches fully verified current source";
          }
        },
        [this, staged, loaded, settingsLoaded, path, obj] {
          QScopedValueRollback<bool> restoring(restoringProject,true);
          // The viewport worker can still hold Jobs into the previous cache.
          // Wait for cancellation before dropping those handles on Windows.
          if (viewControl) viewControl->cancelled = true;
          viewPool.waitForDone();
          viewFuture = QFuture<void>{};
          pendingView.reset();
          plot->snapshot.curves.clear();
          results.clear();
          lastJobs.clear();
          lastRevisions.clear();
          revisions.clear();
          auto previousWorkspace = std::move(workspaceCache);
          workspaceCache = staged;
          revisions = std::move(*loaded);
          settings = settingsLoaded;
          lastSettings = settingsLoaded;
          compareTable->clear();
          compareTable->setRowCount(0);
          compareTable->setColumnCount(0);
          plot->resetView();
          // Only remove an owned workspace directory; never delete an
          // active parallel process's session or a user-managed source.
          if (previousWorkspace && !previousWorkspace->remove())
            qWarning() << "SParamView: retired workspace cache cleanup deferred";

          projectName = obj["name"].toString("Untitled");
          projectPath = path;
          QSignalBlocker b(revisionBox);
          revisionBox->clear();
          for (auto &r : revisions) {
            revisionBox->addItem(r.name);
            revisionBox->setItemData(revisionBox->count() - 1, r.source,
                                     Qt::ToolTipRole);
          }
          auto ui = obj["ui"].toObject();
          revisionBox->setCurrentIndex(
              std::clamp(ui["revision"].toInt(), 0,
                         std::max(0, int(revisions.size()) - 1)));
          updateActiveTouchstoneTitle();
          rebuildChannels();
          applySettings();
          setDirectionMode(ui.contains("directionMode") ? std::clamp(ui["directionMode"].toInt(),0,2) : (settings.reverse?1:0));
          groupBox->setCurrentText(ui["group"].toString("All groups"));
          search->setText(ui["search"].toString());
          aggressorBox->setCurrentText(ui["aggressor"].toString("AUTO (same group)"));
          auto selected = ui["selected"].toArray();
          for (int i = 0; i < channels->count(); ++i)
            channels->item(i)->setCheckState(
                selected.contains(
                    channels->item(i)->data(Qt::UserRole).toString())
                    ? Qt::Checked
                    : Qt::Unchecked);
          int metric = ui["metric"].toInt();
          displayed = si::Metric(std::clamp(metric, 0, 4));
          restoreGeometry(
              QByteArray::fromBase64(ui["geometry"].toString().toLatin1()));
          refreshResults();
          refreshPlot();
          dirty = false;
          notice->setText("Project restored. Reconfirm mapping for revisions whose source files have changed.");
        });
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
void MainWindow::preset(bool save) {
  try {
    if (save)
      syncSettings();
    QString path =
        save ? withExtension(QFileDialog::getSaveFileName(
                                 this, "Save Limit Preset", "SI_Rule.json",
                                 "JSON (*.json)"),
                             ".json")
             : QFileDialog::getOpenFileName(this, "Load Limit Preset", {},
                                            "JSON (*.json)");
    if (path.isEmpty())
      return;
    if (save) {
      auto obj = settingsJson(settings);
      QJsonObject only{{"format", "SIAnalyzerLimitPreset"},
                       {"limits", obj["limits"]},
                       {"targetOhm", obj["targetOhm"]},
                       {"tdrLimit", obj["tdrLimit"]},
                       {"tolerancePercent", obj["tolerancePercent"]}};
      QSaveFile f(path);
      if (!f.open(QIODevice::WriteOnly) ||
          f.write(QJsonDocument(only).toJson()) < 0 || !f.commit())
        throw si::Error("Preset save failed");
    } else {
      QFile f(path);
      if (!f.open(QIODevice::ReadOnly))
        throw si::Error("Cannot open preset");
      auto obj = QJsonDocument::fromJson(f.readAll()).object();
      if (obj["format"].toString() != "SIAnalyzerLimitPreset")
        throw si::Error("Not an SParamView limit preset");
      syncSettings();
      auto current = settingsJson(settings);
      for (auto key : {"limits", "targetOhm", "tdrLimit", "tolerancePercent"})
        current[key] = obj[key];
      settings = settingsFromJson(current);
      applySettings();
      dirty = true;
    }
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
void MainWindow::frequencyLimit() {
  if (displayed == si::Metric::TDR) {
    error("TDR uses Target ± Tolerance.");
    return;
  }
  auto *limit = displayed == si::Metric::RL     ? &settings.rl
                : displayed == si::Metric::IL   ? &settings.il
                : displayed == si::Metric::NEXT ? &settings.next
                                                : &settings.fext;
  QStringList lines;
  for (auto [f, v] : limit->points)
    lines << num(f * 1e-9, 6) + ", " + num(v, 3);
  bool ok;
  auto text = QInputDialog::getMultiLineText(
      this, "Frequency Limit: " + qs(si::name(displayed)),
      "Enter GHz, dB on each line. Clear the text to restore a constant limit.\nExample: 1, -10\nValues outside the defined range are N/A; limits are not extrapolated.",
      lines.join('\n'), &ok);
  if (!ok)
    return;
  try {
    si::Limit revised = *limit;
    revised.points.clear();
    for (auto line : text.split('\n', Qt::SkipEmptyParts)) {
      auto cells = line.split(',');
      if (cells.size() != 2)
        throw si::Error("Expected format: GHz, dB");
      bool a, b;
      double f = cells[0].trimmed().toDouble(&a),
             v = cells[1].trimmed().toDouble(&b);
      if (!a || !b)
        throw si::Error("Invalid limit number");
      revised.points.push_back({f * 1e9, v});
    }
    revised.validate();
    *limit = revised;
    dirty = true;
    notice->setText("Frequency limits saved. Enable the corresponding limit checkbox and rerun analysis.");
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
