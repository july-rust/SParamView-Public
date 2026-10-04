bool MainWindow::selftest(const QString &examples, const QString &output) {
  QDir().mkpath(output);
  loadProject(examples + "/demo.siproject");
  QElapsedTimer timer;
  timer.start();
  while (busy && timer.elapsed() < 120000)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (busy || revisions.size() != 2)
    throw si::Error("GUI project load failed");
  run(settings.quick);
  while (busy && timer.elapsed() < 120000)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (busy || results.size() != 144)
    throw si::Error("GUI Quick Analysis result count mismatch: " +
                    std::to_string(results.size()));
  showMetric(si::Metric::RL);
  QApplication::processEvents();
  savePngFile(output + "/SParamView_screen.png", grab().toImage());
  savePngFile(output + "/SParamView_graph.png", graphImage(plot->snapshot));
  auto snapshot = plot->snapshot;
  ReportOptions options;
  options.raw = true;
  si::Control c;
  writeReport(output + "/SParamView_sample.xlsx", results, revisions,
              lastSettings, projectName, options, lastJobs, snapshot, &c);
  compareRevisions();
  while (busy && timer.elapsed() < 120000)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (compareTable->rowCount() != 4)
    throw si::Error("Revision compare GUI test failed");
  displayed = si::Metric::NEXT;
  compareRevisions();
  while (busy && timer.elapsed() < 120000)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (compareTable->rowCount() != 12)
    throw si::Error("Crosstalk revision matching workflow failed");
  projectPath = output + "/roundtrip.siproject";
  saveProject();
  loadProject(projectPath);
  while (busy && timer.elapsed() < 120000)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (busy || revisions.size() != 2 || revisions[0].channels.size() != 4 ||
      !settings.rl.enabled)
    throw si::Error("Project roundtrip failed");
  dirty = false;
  return true;
}

bool MainWindow::validateProject(const QString &project,
                                 const QString &output) {
  QDir().mkpath(output);
  QElapsedTimer total;
  total.start();
  auto settle = [&] {
    while ((busy || viewRunning) && total.elapsed() < 600000)
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (busy || viewRunning)
      throw si::Error("Project validation timeout");
    QApplication::processEvents();
  };
  loadProject(project);
  settle();
  if (revisions.empty())
    throw si::Error("Project did not load");
  run(settings.quick);
  settle();
  if (results.empty())
    throw si::Error("No analysis results");
  QJsonObject log{{"application", si::version},
                  {"project", projectName},
                  {"revisions", int(revisions.size())},
                  {"results", int(results.size())}};
  QJsonArray values;
  for (const auto &r : results) {
    values.append(QJsonObject{{"channel", qs(r.channel)},
                              {"revision", qs(r.revision)},
                              {"metric", qs(si::name(r.metric))},
                              {"parameter", qs(r.parameter)},
                              {"termination", int(r.termination)},
                              {"termination_name",qs(si::name(r.termination,r.parameter.starts_with("Sdd")))},
                              {"tdr_reflection",r.reflection},
                              {"direction", qs(r.direction)},
                              {"reverse", r.reverse},
                              {"direction_delta_db", r.directionDelta},
                              {"direction_delta_hz", r.directionDeltaX},
                              {"aggressor", qs(r.aggressor)},
                              {"worst", r.worst},
                              {"worst_x", r.worstX},
                              {"margin", r.margin},
                              {"status", qs(r.status)},
                              {"quality", qs(r.quality)},
                              {"note", qs(r.note)},
                              {"reference_ohm", r.referenceOhm},
                              {"minimum", r.minimum},
                              {"maximum", r.maximum}});
  }
  log["analysis"] = values;
  const auto roundtrip=settingsFromJson(settingsJson(settings));
  log["termination_settings_roundtrip"]=roundtrip.tdrTerminations==settings.tdrTerminations&&roundtrip.tdrReflection==settings.tdrReflection;
  for (auto metric : settings.quick) {
    showMetric(metric);
    settle();
    savePngFile(output + "/" + qs(si::name(metric)) + ".png",
                graphImage(plot->snapshot));
  }
  if (qEnvironmentVariableIsSet("SI_VALIDATION_UI")) {
    QTimer::singleShot(100,this,[&]{
      if(auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
        savePngFile(output+"/Termination_Dialog.png",dialog->grab().toImage());dialog->reject();
      }
    });
    chooseTerminations();
    showMetric(si::Metric::TDR);
    settle();
    savePngFile(output + "/TDR_Application.png", grab().toImage());
    const auto full = graphXRange(plot->snapshot);
    auto y = graphYRange(plot->snapshot);
    plot->setView(0, .8e-9, y.first, y.second);
    savePngFile(output + "/TDR_Navigation.png", grab().toImage());
    plot->fitView();
    log["tdr_start_fraction"] = (0-full.first)/(full.second-full.first);
    for (auto tool : findChildren<QToolButton *>())
      if (tool->text().startsWith("Advanced")) tool->setChecked(true);
    QApplication::processEvents();
    savePngFile(output + "/Advanced_Application.png", grab().toImage());
    revisionBox->showPopup();
    QApplication::processEvents();
    savePngFile(output + "/Revision_Popup.png", revisionBox->view()->window()->grab().toImage());
    revisionBox->hidePopup();
    auto menu = menuBar()->actions().front()->menu();
    menu->popup(mapToGlobal(QPoint(20,60)));
    QApplication::processEvents();
    savePngFile(output + "/File_Menu.png", menu->grab().toImage());
    menu->hide();
    for (auto tool : findChildren<QToolButton *>())
      if (tool->text().startsWith("Advanced")) tool->setChecked(false);
  }
  showMetric(si::Metric::RL);
  settle();
  savePngFile(output + "/Application.png", grab().toImage());
  ReportOptions options;
  options.raw = false;
  si::Control c;
  writeReport(output + "/Analysis.xlsx", results, revisions, lastSettings,
              projectName, options, lastJobs, plot->snapshot, &c);
  if (revisions.size() > 1) {
    QJsonObject comparisons;
    for (auto metric :
         {si::Metric::RL, si::Metric::IL, si::Metric::NEXT, si::Metric::FEXT}) {
      displayed = metric;
      compareRevisions();
      settle();
      QJsonArray rows;
      for (int i = 0; i < compareTable->rowCount(); ++i) {
        QJsonObject row;
        for (int j = 0; j < compareTable->columnCount(); ++j) {
          auto cell = compareTable->item(i, j);
          row[compareTable->horizontalHeaderItem(j)->text()] =
              cell ? cell->text() : "";
        }
        rows.append(row);
      }
      comparisons[qs(si::name(metric))] = rows;
    }
    log["comparisons"] = comparisons;
  }
  log["elapsed_seconds"] = total.elapsed() / 1000.0;
  QSaveFile file(output + "/Validation.json");
  if (!file.open(QIODevice::WriteOnly))
    throw si::Error("Validation log open failed");
  file.write(QJsonDocument(log).toJson());
  if (!file.commit())
    throw si::Error("Validation log commit failed");
  dirty = false;
  return true;
}
