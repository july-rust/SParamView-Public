void MainWindow::setResultsTextSize(int pixels) {
  pixels = std::clamp(pixels, 12, 20);
  for (auto grid : {table, compareTable}) {
    grid->setStyleSheet(QString("QTableWidget { font-size:%1px; } "
        "QHeaderView::section { font-size:%1px; }").arg(pixels));
    grid->verticalHeader()->setDefaultSectionSize(pixels + 16);
    grid->resizeColumnsToContents();
  }
}
void MainWindow::setDirectionMode(int mode) {
  if(directionMode != (mode % 3 + 3) % 3) markDirty();
  directionMode = (mode % 3 + 3) % 3;
  if (directionButton) { directionButton->setText(QStringList{"Near → Far", "Far → Near", "Both"}[directionMode]); directionButton->setToolTip("Direction: Near → Far → Far → Near → Both"); }
  settings.reverse = directionMode == 1;
  if (!results.empty() && !busy) { refreshResults(); refreshPlot(); }
}
void MainWindow::setWorkspaceMode(int mode) {
  if (mode < 0 || mode > 2 || mode == workspaceMode) return;
  if (workspaceMode == 0) {
    normalSplitState = analysisSplit->saveState();
    normalHorizontalState = horizontalSplit->saveState();
    normalChannelsVisible = channelsAction->isChecked();
    normalSettingsVisible = settingsAction->isChecked();
    savedWorkspaceTab = tabs->currentIndex();
  }
  workspaceMode = mode;
  tabs->setVisible(mode != 2);
  resultsPanel->setVisible(mode != 1);
  if (mode == 0) {
    channelsAction->setChecked(normalChannelsVisible);
    settingsAction->setChecked(normalSettingsVisible);
    horizontalSplit->restoreState(normalHorizontalState);
    analysisSplit->restoreState(normalSplitState);
    tabs->setCurrentIndex(savedWorkspaceTab);
  } else {
    channelsAction->setChecked(false);
    settingsAction->setChecked(false);
    if (mode == 1) tabs->setCurrentIndex(0);
  }
  workspaceActions[mode]->setChecked(true);
  if (mode == 2) table->setFocus();
  else plot->setFocus();
  // Resize the same widgets: retain selection, axes and calculated results.
}
void MainWindow::toggleFullscreen() {
  if (isFullScreen()) {
    if (fullscreenWasMaximized) {
      showMaximized();
    } else {
      // Cocoa's native fullscreen transition can return a different window
      // size after showNormal(). Restore the actual pre-fullscreen geometry.
      const QRect restore = fullscreenNormalGeometry;
      showNormal();
      if (restore.isValid()) {
        setGeometry(restore);
        // Also restore after the event loop processes the platform state
        // transition. This is deliberately a single deferred correction.
        QTimer::singleShot(0, this, [this, restore] {
          if (!isFullScreen() && !isMaximized() &&
              fullscreenNormalGeometry == restore)
            setGeometry(restore);
        });
#ifdef Q_OS_MACOS
        // Cocoa's fullscreen Space closes asynchronously. The synchronous
        // and next-event-loop geometry restores can be overwritten by the
        // native window-transition callback. Apply one final correction after
        // the native transition, but only if this is still the same request.
        QTimer::singleShot(1600, this, [this, restore] {
          if (!isFullScreen() && !isMaximized() &&
              fullscreenNormalGeometry == restore &&
              geometry() != restore)
            setGeometry(restore);
        });
#endif
      }
    }
  } else {
    fullscreenWasMaximized = isMaximized();
    if (!fullscreenWasMaximized) fullscreenNormalGeometry = geometry();
    showFullScreen();
  }
  fullscreenAction->setChecked(isFullScreen());
}
void MainWindow::resetWorkspaceLayout() {
  setWorkspaceMode(0);
  channelsAction->setChecked(true);
  settingsAction->setChecked(true);
  channelPanel->show(); settingsPanel->show();
  horizontalSplit->setSizes({250, 1160});
  analysisSplit->setSizes({560, 260});
  tableTextSize->setCurrentIndex(1);
}
void MainWindow::restoreWorkspaceLayout() {
  QSettings prefs(QSettings::IniFormat, QSettings::UserScope, "SParamView", QCoreApplication::applicationName());
  prefs.beginGroup("workspace/v1");
  channelsAction->setChecked(prefs.value("channels", true).toBool());
  settingsAction->setChecked(prefs.value("settings", true).toBool());
  channelPanel->setVisible(channelsAction->isChecked());
  settingsPanel->setVisible(settingsAction->isChecked());
  auto restore = [&](QSplitter *split, const char *key) {
    auto values = prefs.value(key).toList();
    if (values.size() != 2) return;
    int first = values[0].toInt(), second = values[1].toInt();
    if (first >= 50 && second >= 50 && first <= 10000 && second <= 10000)
      split->setSizes({first, second});
  };
  restore(horizontalSplit, "horizontal"); restore(analysisSplit, "vertical");
  int index = tableTextSize->findData(prefs.value("tablePixels", 14).toInt());
  tableTextSize->setCurrentIndex(index < 0 ? 1 : index);
  setResultsTextSize(tableTextSize->currentData().toInt());
}
void MainWindow::saveWorkspaceLayout() {
  // Save the user's split layout, never the temporary focus/fullscreen mode.
  int mode = workspaceMode;
  if (mode != 0) setWorkspaceMode(0);
  QSettings prefs(QSettings::IniFormat, QSettings::UserScope, "SParamView", QCoreApplication::applicationName());
  prefs.beginGroup("workspace/v1");
  prefs.setValue("channels", channelsAction->isChecked());
  prefs.setValue("settings", settingsAction->isChecked());
  auto save = [&](QSplitter *split, const char *key) {
    auto sizes = split->sizes();
    if (sizes.size() == 2 && sizes[0] >= 50 && sizes[1] >= 50)
      prefs.setValue(key, QVariantList{sizes[0], sizes[1]});
  };
  save(horizontalSplit, "horizontal"); save(analysisSplit, "vertical");
  prefs.setValue("tablePixels", tableTextSize->currentData());
}
MainWindow::~MainWindow() {
  if (viewControl) viewControl->cancelled = true;
  viewPool.waitForDone();
  if (control)
    control->cancelled = true;
  taskPool.waitForDone();
}
void MainWindow::error(const QString &text) {
  QMessageBox::warning(this, "SParamView", text);
}
void MainWindow::task(std::function<void(si::Control *)> work,
                      std::function<void()> done) {
  if (busy)
    return;
  ++viewGeneration;
  pendingView.reset();
  if (viewControl) viewControl->cancelled = true;
  busy = true;
  body->setEnabled(false);
  menuBar()->setEnabled(false);
  progress->setValue(0);
  progress->show();
  cancelButton->setEnabled(true);
  cancelButton->show();
  control = std::make_shared<si::Control>();
  auto state = control;
  state->progress = [this](double f, const std::string &text) {
    QMetaObject::invokeMethod(
        this,
        [this, f, text] {
          progress->setValue(int(f * 1000));
          statusBar()->showMessage(qs(text));
        },
        Qt::QueuedConnection);
  };
  auto failure = std::make_shared<QString>();
  auto watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this,
          [this, watcher, failure, done, state] {
            busy = false;
            body->setEnabled(true);
            menuBar()->setEnabled(true);
            progress->hide();
            cancelButton->hide();
            watcher->deleteLater();
            if (!failure->isEmpty())
              error(*failure);
            else if (!state->cancelled && done)
              done();
            statusBar()->showMessage(state->cancelled ? "Cancelled · Ready"
                                                      : "Ready");
          });
  taskPool.setMaxThreadCount(1);
  taskPool.setExpiryTimeout(-1);
  future = QtConcurrent::run(&taskPool, [work, state, failure] {
    try {
      work(state.get());
    } catch (const si::Cancelled &) {
      state->cancelled = true;
    } catch (const std::exception &e) {
      *failure = qs(e.what());
    } catch (...) {
      *failure = "Unexpected operation failure";
    }
  });
  watcher->setFuture(future);
}
