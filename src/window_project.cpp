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
      QJsonArray channels, candidates;
      for (auto &c : r.channels)
        channels.append(channelJson(c));
      for (auto &c : r.candidates)
        candidates.append(channelJson(c));
      sources.append(QJsonObject{{"name", r.name},
                                 {"source", base.relativeFilePath(r.source)},
                                 {"absoluteSource", r.source},
                                 {"channels", channels},
                                 {"mappingCandidates", candidates},
                                 {"mappingSource", r.mappingSource},
                                 {"confirmed", r.confirmed},
                                 {"cache", qs(si::utf8(r.cache->path()))},
                                 {"sourceSha256", qs(r.cache->meta().sha256)}});
    }
    QJsonArray selected;
    for (auto &c : selectedChannels())
      selected.append(qs(c.id));
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
      loaded->push_back(rev);
    }
    auto settingsLoaded = settingsFromJson(obj["settings"].toObject());
    QString cacheRoot =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/v3";
    task(
        [loaded, cacheRoot, sources](si::Control *c) {
          for (size_t i = 0; i < loaded->size(); ++i) {
            auto &r = (*loaded)[i];
            r.cache = si::Cache::open(fp(r.source), fp(cacheRoot), c);
            si::validateMapping(r.channels, r.cache->meta());
            auto original =
                sources[int(i)].toObject()["sourceSha256"].toString();
            if (!original.isEmpty() && original != qs(r.cache->meta().sha256))
              r.confirmed = false;
          }
        },
        [this, loaded, settingsLoaded, path, obj] {
          revisions = std::move(*loaded);
          settings = settingsLoaded;
          projectName = obj["name"].toString("Untitled");
          projectPath = path;
          results.clear();
          lastJobs.clear();
          QSignalBlocker b(revisionBox);
          revisionBox->clear();
          for (auto &r : revisions)
            revisionBox->addItem(r.name);
          auto ui = obj["ui"].toObject();
          revisionBox->setCurrentIndex(
              std::clamp(ui["revision"].toInt(), 0,
                         std::max(0, int(revisions.size()) - 1)));
          rebuildChannels();
          applySettings();
          setDirectionMode(ui.contains("directionMode") ? std::clamp(ui["directionMode"].toInt(),0,2) : (settings.reverse?1:0));
          groupBox->setCurrentText(ui["group"].toString("All groups"));
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
