// Compiler target macros describe this executable, not the host OS.
static constexpr const char *architectureName() {
#if defined(_M_ARM64) || defined(__aarch64__)
  return "ARM64";
#elif defined(_M_X64) || defined(__x86_64__)
  return "x64";
#elif defined(_M_IX86) || defined(__i386__)
  return "x86";
#elif defined(_M_ARM) || defined(__arm__)
  return "ARM";
#else
  return "Unknown";
#endif
}
static bool differentialTdrResult(const si::Result &r) {
  return r.metric == si::Metric::TDR && r.parameter.starts_with("Sdd");
}
static bool hasDifferentialChannels(const std::vector<si::Channel> &channels) {
  return std::any_of(channels.begin(), channels.end(),
                     [](const auto &c) { return c.differential(); });
}
static QString signalTypeForChannels(const std::vector<si::Channel> &channels) {
  bool differential = false, singleEnded = false;
  for (const auto &c : channels)
    (c.differential() ? differential : singleEnded) = true;
  if (differential && singleEnded) return "Mixed";
  if (differential) return "Differential";
  if (singleEnded) return "Single-ended";
  return "Unknown";
}
static std::vector<std::string>
tdrViewIdentity(const std::vector<si::Result> &results) {
  std::vector<std::string> keys;
  for (const auto &r : results) {
    if (r.metric != si::Metric::TDR)
      continue;
    keys.push_back(r.revision + "|" + r.channelId + "|" + r.aggressorId + "|" +
                   std::to_string(int(r.termination)) + "|" +
                   std::to_string(int(r.reverse)) + "|" +
                   std::to_string(int(r.reflection)));
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}
static bool mixedModeConfirmsDifferentialPairs(
    const si::Metadata &m, const std::vector<si::Channel> &channels) {
  std::set<std::pair<int, int>> declared, mapped;
  const std::regex differential(R"(^[dD]([0-9]+),([0-9]+)$)");
  for (const auto &token : m.mixedOrder) {
    std::smatch match;
    if (!std::regex_match(token, match, differential)) continue;
    int p = std::stoi(match[1].str()) - 1;
    int n = std::stoi(match[2].str()) - 1;
    if (p < 0 || n < 0 || p == n) return false;
    declared.insert({p, n});
  }
  if (declared.empty()) return false;
  for (const auto &c : channels) {
    if (!c.differential()) continue;
    mapped.insert({c.nearP, c.nearN});
    if (c.farP >= 0 && c.farN >= 0) mapped.insert({c.farP, c.farN});
  }
  return mapped == declared;
}
QJsonObject channelJson(const si::Channel &c) {
  return {{"id", qs(c.id)},       {"name", qs(c.name)}, {"alias", qs(c.alias)},
          {"group", qs(c.group)}, {"nearP", c.nearP},   {"nearN", c.nearN},
          {"farP", c.farP},       {"farN", c.farN}};
}
si::Channel channelFromJson(const QJsonObject &o) {
  for(auto key:{"nearP","nearN","farP","farN"})
    if(o.contains(key) && (!o[key].isDouble() || o[key].toDouble()!=o[key].toInt(-2) || o[key].toDouble() < -1))
      throw si::Error("Invalid integer Channel Mapping port: " + std::string(key));
  si::Channel c;
  c.id = ss(o["id"].toString());
  c.name = ss(o["name"].toString());
  c.alias = ss(o["alias"].toString());
  c.group = ss(o["group"].toString("Default"));
  c.nearP = o["nearP"].toInt(-1);
  c.nearN = o["nearN"].toInt(-1);
  c.farP = o["farP"].toInt(-1);
  c.farN = o["farN"].toInt(-1);
  return c;
}
QJsonObject settingsJson(const si::Settings &s) {
  QJsonObject o{
      {"startHz", s.startHz},
      {"stopHz", std::isfinite(s.stopHz) ? QJsonValue(s.stopHz) : QJsonValue()},
      {"reverse", s.reverse},
      {"markers", s.markers},
      {"prominence", s.prominence},
      {"targetOhm", s.targetOhm},
      {"tdrLimit", s.tdrLimitEnabled},
      {"tolerancePercent", s.tolerancePercent},
      {"tdrStart", s.tdrStartSeconds},
      {"tdrStop", std::isfinite(s.tdrStopSeconds) ? QJsonValue(s.tdrStopSeconds)
                                                  : QJsonValue()},
      {"kaiserBeta", s.kaiserBeta},
      {"performance", s.performance}};
  QJsonArray manual, quick;
  for (double f : s.manualMarkersHz)
    manual.append(f);
  for (auto m : s.quick)
    quick.append(int(m));
  o["manualMarkersHz"] = manual;
  o["quick"] = quick;
  QJsonArray terms;
  for(auto t:s.tdrTerminations)terms.append(int(t));
  o["tdrTerminations"]=terms;
  o["tdrReflection"]=s.tdrReflection;
  QJsonArray limits;
  for (auto *l : {&s.rl, &s.il, &s.next, &s.fext}) {
    QJsonArray points;
    for (auto [f, v] : l->points)
      points.append(QJsonArray{f, v});
    limits.append(QJsonObject{{"enabled", l->enabled},
                              {"constant", l->constant},
                              {"points", points}});
  }
  o["limits"] = limits;
  return o;
}
static bool sameAnalysisContext(const si::Settings &a, const si::Settings &b) {
  auto left = settingsJson(a), right = settingsJson(b);
  // Direction is a display choice because valid forward/reverse jobs are calculated
  // together. Quick checkboxes only choose which metrics to request.
  left.remove("reverse"); right.remove("reverse");
  left.remove("quick"); right.remove("quick");
  return left == right;
}
si::Settings settingsFromJson(const QJsonObject &o) {
  // QJsonValue::toDouble defaults to zero for wrong types. Never silently turn
  // a damaged limit or project setting into a different electrical condition.
  for(auto key:{"startHz","markers","prominence","targetOhm","tolerancePercent","tdrStart","kaiserBeta","performance"})
    if(o.contains(key) && (!o[key].isDouble() || !std::isfinite(o[key].toDouble())))
      throw si::Error("Invalid numeric project setting: " + std::string(key));
  for(auto key:{"stopHz","tdrStop"})
    if(o.contains(key) && !o[key].isNull() && (!o[key].isDouble() || !std::isfinite(o[key].toDouble())))
      throw si::Error("Invalid numeric range setting: " + std::string(key));
  for(auto key:{"reverse","tdrLimit","tdrReflection"})
    if(o.contains(key) && !o[key].isBool()) throw si::Error("Invalid boolean project setting: " + std::string(key));
  for(auto key:{"markers","performance"})
    if(o.contains(key) && o[key].toDouble()!=o[key].toInt(-1)) throw si::Error("Invalid integer project setting");
  for(auto key:{"manualMarkersHz","quick","limits","tdrTerminations"})
    if(o.contains(key) && !o[key].isArray()) throw si::Error("Invalid project setting array: " + std::string(key));
  for(auto f:o["manualMarkersHz"].toArray())
    if(!f.isDouble() || !std::isfinite(f.toDouble()) || f.toDouble()<0) throw si::Error("Invalid manual marker frequency");
  for(auto m:o["quick"].toArray())
    if(!m.isDouble() || m.toDouble()!=m.toInt(-1) || m.toInt(-1)<0 || m.toInt(-1)>4) throw si::Error("Invalid Quick Analysis metric");
  if(o["limits"].toArray().size()>4) throw si::Error("Too many limit definitions");
  for(auto value:o["limits"].toArray()) {
    if(!value.isObject()) throw si::Error("Invalid limit definition");
    auto l=value.toObject();
    if((l.contains("enabled")&&!l["enabled"].isBool()) ||
       (l.contains("constant")&&(!l["constant"].isDouble() || !std::isfinite(l["constant"].toDouble()))) ||
       (l.contains("points")&&!l["points"].isArray())) throw si::Error("Invalid limit fields");
    for(auto point:l["points"].toArray()) {
      auto p=point.toArray();
      if(p.size()!=2 || !p[0].isDouble() || !p[1].isDouble()) throw si::Error("Invalid numeric limit point");
    }
  }
  si::Settings s;
  s.startHz = o["startHz"].toDouble();
  s.stopHz = o["stopHz"].isDouble() ? o["stopHz"].toDouble()
                                    : std::numeric_limits<double>::infinity();
  s.reverse = o["reverse"].toBool();
  s.markers = o["markers"].toInt(3);
  s.prominence = o["prominence"].toDouble(.15);
  s.targetOhm = o["targetOhm"].toDouble();
  s.tdrLimitEnabled = o["tdrLimit"].toBool();
  s.tolerancePercent = o["tolerancePercent"].toDouble(10);
  s.tdrStartSeconds = o["tdrStart"].toDouble();
  s.tdrStopSeconds = o["tdrStop"].isDouble()
                         ? o["tdrStop"].toDouble()
                         : std::numeric_limits<double>::infinity();
  s.tdrReflection=o["tdrReflection"].toBool();
  s.tdrTerminations={si::Termination::Reference}; // Preserve legacy projects.
  if(o.contains("tdrTerminations")) {
    s.tdrTerminations.clear();
    for(auto t:o["tdrTerminations"].toArray()) {
      if(!t.isDouble()||t.toDouble()!=t.toInt(-1)||t.toInt(-1)<0||t.toInt(-1)>4)
        throw si::Error("Invalid TDR termination setting");
      s.tdrTerminations.push_back(si::Termination(t.toInt()));
    }
    if(s.tdrTerminations.empty())throw si::Error("Select at least one TDR termination");
  }
  s.kaiserBeta = o["kaiserBeta"].toDouble(6);
  s.performance = std::clamp(o["performance"].toInt(1), 0, 2);
  if(s.startHz<0 || s.stopHz<s.startHz || s.tdrStartSeconds<0 || s.tdrStopSeconds<s.tdrStartSeconds ||
     s.prominence<0 || s.targetOhm<0 || s.tolerancePercent<0 || s.kaiserBeta<0 || s.kaiserBeta>13 ||
     s.markers<0 || s.markers>5 || o["performance"].toInt(1)<0 || o["performance"].toInt(1)>2)
    throw si::Error("Project setting is outside the supported range");
  for (auto f : o["manualMarkersHz"].toArray())
    s.manualMarkersHz.push_back(f.toDouble());
  if (o.contains("quick")) {
    s.quick.clear();
    for (auto m : o["quick"].toArray())
      if (m.toInt() >= 0 && m.toInt() <= 4)
        s.quick.push_back(si::Metric(m.toInt()));
  }
  auto limits = o["limits"].toArray();
  int i = 0;
  for (auto *l : {&s.rl, &s.il, &s.next, &s.fext}) {
    if (i < limits.size()) {
      auto v = limits[i].toObject();
      l->enabled = v["enabled"].toBool();
      l->constant = v["constant"].toDouble();
      for (auto p : v["points"].toArray()) {
        auto a = p.toArray();
        if (a.size() != 2)
          throw si::Error("Invalid limit point");
        l->points.push_back({a[0].toDouble(), a[1].toDouble()});
      }
      l->validate();
    }
    ++i;
  }
  return s;
}
static QPushButton *button(QString text, QLayout *l, std::function<void()> fn) {
  auto b = new QPushButton(text);
  l->addWidget(b);
  QObject::connect(b, &QPushButton::clicked, b, std::move(fn));
  return b;
}
static QDoubleSpinBox *spin(double min, double max, int decimals = 3) {
  auto v = new QDoubleSpinBox;
  v->setRange(min, max);
  v->setDecimals(decimals);
  v->setButtonSymbols(QAbstractSpinBox::NoButtons);
  return v;
}
static QString withExtension(QString path, QString ext) {
  if (!path.isEmpty() && !path.endsWith(ext, Qt::CaseInsensitive))
    path += ext;
  return path;
}
class ChannelListWidget final : public QListWidget {
public:
  using QListWidget::QListWidget;
protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!itemAt(event->position().toPoint())) clearSelection();
    QListWidget::mousePressEvent(event);
  }
};

// Keep a control row's natural layout, but allow horizontal navigation on
// narrow screens instead of forcing the entire main window wider than the
// available desktop. Reserve one scrollbar extent so translated text and
// native scrollbar styles cannot clip the controls vertically.
class WorkspaceControlStrip final : public QScrollArea {
public:
  WorkspaceControlStrip(QHBoxLayout *row, const QString &name) {
    setObjectName(name);
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *content = new QWidget;
    content->setObjectName(name + "Contents");
    // Use compact padding without shrinking the font or the clickable text.
    // This also offsets the scrollbar height in short desktop work areas.
    content->setStyleSheet(
        "QPushButton,QToolButton { padding:3px 6px; }"
        "QComboBox,QDoubleSpinBox,QSpinBox,QLineEdit { padding:2px 4px; }");
    row->setContentsMargins(0, 0, 0, 0);
    content->setLayout(row);
    setWidget(content);
    content->installEventFilter(this);
  }
  QSize sizeHint() const override {
    if (!widget()) return QSize(0, 0);
    const auto natural = widget()->sizeHint();
    return QSize(natural.width(), rowHeight());
  }
  QSize minimumSizeHint() const override { return QSize(0, rowHeight()); }
protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == widget() && event->type() == QEvent::LayoutRequest)
      updateGeometry();
    return QScrollArea::eventFilter(watched, event);
  }
private:
  int rowHeight() const {
    if (!widget()) return 0;
    return std::max(widget()->sizeHint().height(), widget()->minimumSizeHint().height())
        + style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this)
        + 2 * frameWidth();
  }
};

void MainWindow::installHeaderProjectButtons() {
  if (!centralWidget() || !centralWidget()->layout())
    return;
  if (findChild<QPushButton *>("headerOpenProjectButton"))
    return;
  auto *contents = findChild<QWidget *>("workspaceHeaderContents");
  auto *banner = contents ? qobject_cast<QBoxLayout *>(contents->layout()) : nullptr;
  if (!banner)
    return;
  int insertAt = banner->count();
  for (int i = 0; i < banner->count(); ++i) {
    auto *item = banner->itemAt(i);
    auto *existing = item ? qobject_cast<QPushButton *>(item->widget()) : nullptr;
    if (existing && existing->text().contains("Open Touchstone")) {
      insertAt = i + 1;
      break;
    }
  }
  auto *openProject = new QPushButton("Open Project");
  openProject->setObjectName("headerOpenProjectButton");
  openProject->setToolTip("Open an SParamView project (.siproject)");
  connect(openProject, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getOpenFileName(
        this, sparamview::ui_language::textFor(this, "Project"), sparamview::preferences::workingDirectory(), sparamview::ui_language::textFor(this, "SI Project (*.siproject)"));
    if (!path.isEmpty())
      loadProject(path);
  });
  auto *saveProjectButton = new QPushButton("Save Project");
  saveProjectButton->setObjectName("headerSaveProjectButton");
  saveProjectButton->setToolTip("Save the current SParamView project");
  connect(saveProjectButton, &QPushButton::clicked, this,
          [this] { saveProject(); });
  banner->insertWidget(insertAt, openProject);
  banner->insertWidget(insertAt + 1, saveProjectButton);
}

MainWindow::MainWindow() {
  workspaceCache = std::make_shared<QTemporaryDir>(
      QDir(cacheSession.path()).filePath("workspace-XXXXXX"));
  if (!workspaceCache->isValid())
    throw std::runtime_error("Cannot create private workspace cache");
  settings.tdrStopSeconds=3e-9;
  auto title = "SParamView · " + qs(si::version);
  const auto architecture = QString::fromLatin1(architectureName());
  if (architecture == "ARM64")
    title += " · " + architecture;
  setWindowTitle(title);
  resize(1440, 980);
  setAcceptDrops(true);
  auto file = menuBar()->addMenu("File");
  auto *newAction = file->addAction("New", QKeySequence::New, this, &MainWindow::newWorkspace);
  newAction->setObjectName("newWorkspaceAction");
  file->addSeparator();
  file->addAction(
      "Open Touchstone...", QKeySequence::Open, this,
      [this] {
        openPaths(QFileDialog::getOpenFileNames(
            this, sparamview::ui_language::textFor(this, "Select Touchstone Files"), sparamview::preferences::workingDirectory(),
            sparamview::ui_language::textFor(this, "Touchstone (*.s*p *.ts)") + ";;" + sparamview::ui_language::textFor(this, "All files (*)")));
      });
  file->addAction("Open Project...", this, [this] {
    auto p = QFileDialog::getOpenFileName(this, sparamview::ui_language::textFor(this, "Project"), sparamview::preferences::workingDirectory(),
                                          sparamview::ui_language::textFor(this, "SI Project (*.siproject)"));
    if (!p.isEmpty())
      loadProject(p);
  });
  file->addAction(
      "Save Project", QKeySequence::Save, this, [this] { saveProject(); });
  file->addAction("Save Project As...", this, [this] { saveProject(true); });
  file->addSeparator();
  file->addAction("Export Excel Workbook...", this, [this] { exportXlsx(); });
  file->addAction("Export Plot as PNG...", this, [this] { savePng(); });
  file->addAction("Export Selected Trace as CSV...", this, [this] { saveCsv(); });
  auto mapping = menuBar()->addMenu("Channel Mapping");
  mapping->addAction("Edit / Confirm Mapping...", this, [this] { editMapping(); });
  mapping->addAction("Import Mapping CSV...", this, [this] { importMapping(); });
  mapping->addAction("Export Mapping CSV...", this, [this] { exportMapping(); });
  auto viewMenu = menuBar()->addMenu("View");
  auto modes = new QActionGroup(this);
  for (int i = 0; i < 3; ++i) {
    workspaceActions[i] = viewMenu->addAction(
        QStringList{"Split View", "Maximize Plot", "Maximize Results"}[i]);
    workspaceActions[i]->setObjectName(QString("workspaceMode%1").arg(i));
    // QToolButton::setDefaultAction() re-reads QAction::iconText whenever the
    // action's checked/enabled state changes. Give toolbar labels their own
    // stable short text instead of allowing long menu titles to expand the
    // workspace's minimum width during Split/Plot/Results transitions.
    workspaceActions[i]->setIconText(QStringList{"Split", "Plot", "Results"}[i]);
    workspaceActions[i]->setCheckable(true);
    workspaceActions[i]->setShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)));
    modes->addAction(workspaceActions[i]);
    connect(workspaceActions[i], &QAction::triggered, this,
            [this, i] { setWorkspaceMode(i); });
  }
  workspaceActions[0]->setChecked(true);
  viewMenu->addSeparator();
  channelsAction = viewMenu->addAction("Channel List");
  channelsAction->setIconText("Channels");
  channelsAction->setCheckable(true);
  channelsAction->setChecked(true);
  settingsAction = viewMenu->addAction("Analysis Settings");
  settingsAction->setIconText("Settings");
  settingsAction->setCheckable(true);
  settingsAction->setChecked(true);
  fullscreenAction = viewMenu->addAction("Full Screen");
  fullscreenAction->setIconText("⛶");
  fullscreenAction->setToolTip("Full Screen (F11)");
  fullscreenAction->setCheckable(true);
  fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
  connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
  auto exitFullscreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(exitFullscreen, &QShortcut::activated, this, [this] {
    if (isFullScreen()) toggleFullscreen();
    if (channels) channels->clearSelection();
  });
  viewMenu->addAction("Reset Layout", this, &MainWindow::resetWorkspaceLayout);
  analysisWindows = std::make_unique<si::gui::AnalysisWindowManager>(this);
  analysisWindows->installMenu();
  // Only GUI-thread provider access may read MainWindow revisions. The returned
  // cache/mapping are immutable worker inputs and SHA-verified on the worker.
  auto channelSourceProvider = [this](int index)
      -> std::optional<si::gui::IldSourceInput> {
    if(index<0 || size_t(index)>=revisions.size()) return std::nullopt;
    const auto &revision=revisions[size_t(index)];
    if(!revision.confirmed || !revision.cache || revision.channels.empty())
      return std::nullopt;
    const auto generation=std::max<quint64>(1,analysisWindows->context().generation);
    return si::gui::IldSourceInput{
      revision.cache,revision.channels,ss(revision.name),
      revision.cache->meta().sha256,generation,generation};
  };
  analysisWindows->setIldSourceProvider(channelSourceProvider);
  analysisWindows->setIcnSourceProvider(channelSourceProvider);
  analysisWindows->setEyeSourceProvider(channelSourceProvider);
  auto rules = menuBar()->addMenu("Limit");
  rules->addAction("Save Preset...", this, [this] { preset(true); });
  rules->addAction("Load Preset...", this, [this] { preset(false); });
  rules->addAction("Edit Frequency Limits...", this, [this] { frequencyLimit(); });
  auto tools = menuBar()->addMenu("Tools");
  tools->addAction("Open Sample Project", this, [this] { loadDemo(); });
  tools->addAction("Verify Source SHA-256", this, [this] {
    int i = revisionBox->currentIndex();
    if (i < 0 || busy)
      return;
    auto rev = revisions[size_t(i)];
    task(
        [rev](si::Control *c) {
          if (si::sha256File(fp(rev.source), c) != rev.cache->meta().sha256)
            throw si::Error("Source hash changed. Reopen the Touchstone file.");
        },
        [this] { sparamview::ui_language::message(notice,"Source SHA-256 verified"); });
  });
  menuBar()->addAction("About", this, [this] {
#if defined(Q_OS_WIN)
    const QString buildPlatform = "Windows";
#elif defined(Q_OS_MACOS)
    const QString buildPlatform = "macOS";
#else
    const QString buildPlatform = QSysInfo::productType();
#endif
    QMessageBox::information(
        this, sparamview::ui_language::textFor(this, "About SParamView"),
        sparamview::ui_language::textFor(this,"SParamView\nVersion %1\nBuild: %2\n\nCopyright © 2026 july\nsparamview@gmail.com\nLicensed under the MIT License\n\nC++20 / Qt 6\nProcessing is local. Original Touchstone files are not modified.")
            .arg(si::version)
            .arg(buildPlatform + " " + QString::fromLatin1(architectureName())));
  });
  body = new QWidget;
  setCentralWidget(body);
  auto root = new QVBoxLayout(body);
  root->setContentsMargins(12, 6, 12, 6);
  root->setSpacing(4);
  auto banner = new QHBoxLayout;
  banner->setSpacing(8);
  auto brand = new QLabel(
      "SParam<b>View</b> <span style='color:#6f8297;font-size:12px'> / "
      "VERSION " + qs(si::version) + "</span>");
  brand->setStyleSheet("font-size:23px;color:#172e47");
  banner->addWidget(brand);
  banner->addStretch();
  button("+ Open Touchstone", banner, [this] {
    openPaths(QFileDialog::getOpenFileNames(
        this, sparamview::ui_language::textFor(this, "Select Touchstone Files"), sparamview::preferences::workingDirectory(), sparamview::ui_language::textFor(this, "Touchstone (*.s*p *.ts)") + ";;" + sparamview::ui_language::textFor(this, "All files (*)")));
  });
  button("Open Sample", banner, [this] { loadDemo(); });
  button("Export XLSX", banner, [this] { exportXlsx(); });
  root->addWidget(new WorkspaceControlStrip(banner, "workspaceHeader"));
  auto split = horizontalSplit = new QSplitter;
  split->setObjectName("workspaceHorizontalSplit");
  split->setHandleWidth(8);
  split->setChildrenCollapsible(false);
  split->setOpaqueResize(false);
  root->addWidget(split, 1);
  auto sidebar = new QWidget;
  auto sidebarScroll = new QScrollArea;
  auto channelContainer = new QWidget;
  channelPanel = channelContainer;
  channelContainer->setMinimumWidth(225);
  channelContainer->setMaximumWidth(420);
  auto channelLayout = new QVBoxLayout(channelContainer);
  channelLayout->setContentsMargins(0, 0, 0, 0);
  channelLayout->setSpacing(8);
  channelLayout->addWidget(sidebarScroll, 1);
  sidebarScroll->setWidgetResizable(true);
  sidebarScroll->setFrameShape(QFrame::NoFrame);
  sidebarScroll->setWidget(sidebar);
  sidebarScroll->setMinimumWidth(205);
  sidebarScroll->setMaximumWidth(420);
  auto left = new QVBoxLayout(sidebar);
  left->setContentsMargins(8, 8, 12, 8);
  left->setSpacing(8);
  left->addWidget(new QLabel("REVISION / CHANNEL"));
  revisionBox = new QComboBox;
  revisionBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  revisionBox->setMinimumContentsLength(16);
  left->addWidget(revisionBox);
  groupBox = new QComboBox;
  groupBox->addItem("All groups");
  left->addWidget(groupBox);
  search = new QLineEdit;
  search->setPlaceholderText("Search Channels...");
  left->addWidget(search);
  channels = new ChannelListWidget;
  channels->setMinimumHeight(100);
  channels->setSelectionMode(QAbstractItemView::ExtendedSelection);
  left->addWidget(channels, 1);
  auto choose = new QHBoxLayout;
  button("Select All", choose, [this] {
    for (int i = 0; i < channels->count(); ++i)
      if (!channels->item(i)->isHidden())
        channels->item(i)->setCheckState(Qt::Checked);
  });
  button("Clear Selection", choose, [this] {
    for (int i = 0; i < channels->count(); ++i)
      channels->item(i)->setCheckState(Qt::Unchecked);
  });
  left->addLayout(choose);
  selectionInfo = new QLabel("Open a Touchstone file");
  selectionInfo->setWordWrap(true);
  left->addWidget(selectionInfo);
  button("Channel Mapping…", left, [this] { editMapping(); });
  auto quickGroup = new QGroupBox("Quick Analysis");
  auto ql = new QGridLayout(quickGroup);
  ql->setContentsMargins(8, 6, 8, 6);
  ql->setHorizontalSpacing(10);
  ql->setVerticalSpacing(4);
  QStringList labels{"Return Loss", "Insertion Loss", "NEXT", "FEXT", "TDR"};
  QStringList shortLabels{"RL", "IL", "NEXT", "FEXT", "TDR"};
  for (int i = 0; i < 5; ++i) {
    quickChecks[i] = new QCheckBox(shortLabels[i]);
    quickChecks[i]->setObjectName(QString("quickMetric%1").arg(i));
    quickChecks[i]->setToolTip(labels[i]);
    quickChecks[i]->setChecked(true);
    ql->addWidget(quickChecks[i], i / 3, i % 3);
  }
  left->addWidget(quickGroup);
  auto footer = new QWidget;
  auto footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(8, 0, 12, 8);
  channelLayout->addWidget(footer);
  quickButton = button("QUICK ANALYSIS", footerLayout, [this] {
    std::vector<si::Metric> kinds;
    for (int i = 0; i < 5; ++i)
      if (quickChecks[i]->isChecked())
        kinds.push_back(si::Metric(i));
    run(kinds);
  });
  quickButton->setObjectName("primary");
  split->addWidget(channelContainer);
  auto content = new QWidget;
  auto right = new QVBoxLayout(content);
  right->setContentsMargins(12, 4, 8, 4);
  right->setSpacing(4);
  auto workspaceBar = new QHBoxLayout;
  workspaceBar->setSpacing(5);
  int viewIndex = 0;
  for (auto action : workspaceActions) {
    auto tool = new QToolButton;
    tool->setDefaultAction(action);
    tool->setText(QStringList{"Split", "Plot", "Results"}[viewIndex++]);
    tool->setToolTip(action->text() + " (" + action->shortcut().toString() + ")");
    workspaceBar->addWidget(tool);
  }
  auto advancedToggle = new QToolButton; advancedToggle->setObjectName("advancedToggle"); advancedToggle->setText("Advanced ▸"); advancedToggle->setCheckable(true); advancedToggle->setToolTip("Show advanced analysis settings"); workspaceBar->addWidget(advancedToggle);
  workspaceBar->addSpacing(8);
  workspaceBar->addWidget(new QLabel("Freq"));
  start = spin(0, 1e6, 6); stop = spin(0, 1e6, 6);
  start->setMaximumWidth(92); stop->setMaximumWidth(92); stop->setSpecialValueText("Auto");
  workspaceBar->addWidget(start); workspaceBar->addWidget(new QLabel("→")); workspaceBar->addWidget(stop); workspaceBar->addWidget(new QLabel("GHz"));
  directionButton = button("Near → Far", workspaceBar, [this] { setDirectionMode((directionMode + 1) % 3); });
  directionButton->setObjectName("directionToggle");
  directionButton->setToolTip("Direction: Near → Far → Far → Near → Both");
  markerBox = new QComboBox; markerBox->addItems({"Worst", "Top 3", "Top 5", "Off"}); markerBox->setCurrentIndex(1); markerBox->setMaximumWidth(78); workspaceBar->addWidget(markerBox);
  workspaceBar->addStretch();
  for (auto action : {channelsAction, settingsAction, fullscreenAction}) {
    auto tool = new QToolButton; tool->setDefaultAction(action);
    if (action == channelsAction) tool->setText("Channels");
    if (action == settingsAction) tool->setText("Settings");
    if (action == fullscreenAction) { tool->setText("⛶"); tool->setToolTip("Full Screen (F11)"); }
    workspaceBar->addWidget(tool);
  }
  right->addWidget(new WorkspaceControlStrip(workspaceBar, "workspaceToolbar"));
  auto settingsScroll = new QScrollArea;
  settingsPanel = settingsScroll; settingsScroll->setObjectName("analysisSettingsPanel"); settingsScroll->setWidgetResizable(true); settingsScroll->setFrameShape(QFrame::NoFrame); settingsScroll->setMinimumHeight(44); settingsScroll->setMaximumHeight(74);
  auto settingsContent = new QWidget; settingsScroll->setWidget(settingsContent);
  auto config = new QVBoxLayout(settingsContent); config->setContentsMargins(6, 4, 6, 4); config->setSpacing(4); right->addWidget(settingsScroll);
  auto controls = new QHBoxLayout; controls->setSpacing(5); QStringList operators{"≤", "≥", "≤", "≤"};
  for (int i = 0; i < 4; ++i) {
    auto segment = new QWidget; auto row = new QHBoxLayout(segment); row->setContentsMargins(4,0,4,0); row->setSpacing(3);
    limits[i] = new QCheckBox; limits[i]->setToolTip("Enable " + shortLabels[i] + " PASS/FAIL limit");
    metricButtons[i] = new QToolButton; metricButtons[i]->setText(shortLabels[i]); metricButtons[i]->setCheckable(true); metricButtons[i]->setToolTip(labels[i] + " plot");
    connect(metricButtons[i], &QToolButton::clicked, this, [this, i] {
    const auto metric = si::Metric(i);
    try { syncSettings(); }
    catch (const std::exception &e) { error(qs(e.what())); return; }
    if (sameAnalysisContext(settings, lastSettings) && hasCurrentResult(metric))
      showMetric(metric);
    else
      run({metric});
  });
    limitValues[i] = spin(-600,100,2);
    const si::Limit *defaults[] = {&settings.rl, &settings.il, &settings.next, &settings.fext};
    limitValues[i]->setValue(defaults[i]->constant);
    limits[i]->setChecked(defaults[i]->enabled);
    limitValues[i]->setSuffix(" dB"); limitValues[i]->setMaximumWidth(86);
    row->addWidget(limits[i]); row->addWidget(metricButtons[i]); row->addWidget(new QLabel(operators[i])); row->addWidget(limitValues[i]); controls->addWidget(segment,1);
  }
  auto tdrSegment = new QWidget; auto tdrRow = new QHBoxLayout(tdrSegment); tdrRow->setContentsMargins(4,0,4,0); tdrRow->setSpacing(3);
  tdrLimit = new QCheckBox; tdrLimit->setToolTip("Enable TDR impedance tolerance PASS/FAIL limit");
  metricButtons[4] = new QToolButton; metricButtons[4]->setText("TDR"); metricButtons[4]->setCheckable(true); connect(metricButtons[4], &QToolButton::clicked, this, [this] {
  const auto metric = si::Metric::TDR;
  try { syncSettings(); }
  catch (const std::exception &e) { error(qs(e.what())); return; }
  if (sameAnalysisContext(settings, lastSettings) && hasCurrentResult(metric))
    showMetric(metric);
  else
    run({metric});
});
  tolerance = spin(0,1000,1); tolerance->setValue(settings.tolerancePercent); tdrLimit->setChecked(settings.tdrLimitEnabled); tolerance->setSuffix(" %"); tolerance->setMaximumWidth(72);
  target = spin(0,10000,1); target->setSpecialValueText("Auto Z₀"); target->setSuffix(" Ω"); target->setMaximumWidth(92);
  auto tdrOptions = new QToolButton; tdrOptions->setObjectName("tdrOptionButton"); tdrOptions->setText("⚙"); tdrOptions->setToolTip("TDR options: reference impedance, terminations and reflection coefficient"); connect(tdrOptions, &QToolButton::clicked, this, [this] { chooseTerminations(); });
  terminationSummary = new QLabel; terminationSummary->setMaximumWidth(72); tdrReflection = new QCheckBox; tdrReflection->hide();
  tdrRow->addWidget(tdrLimit); tdrRow->addWidget(metricButtons[4]); tdrRow->addWidget(new QLabel("±")); tdrRow->addWidget(tolerance); tdrRow->addWidget(target); tdrRow->addWidget(terminationSummary); tdrRow->addWidget(tdrOptions); controls->addWidget(tdrSegment,2); config->addLayout(controls);
  auto advanced = new QWidget;
  advanced->setObjectName("advancedSettings");
  auto av = new QGridLayout(advanced);
  av->setContentsMargins(0, 6, 0, 4);
  av->setHorizontalSpacing(8);
  av->setVerticalSpacing(8);
  manualMarkers = new QLineEdit;
  manualMarkers->setPlaceholderText("1, 3, 6, 9");
  av->addWidget(new QLabel("Marker [GHz]"), 0, 0);
  av->addWidget(manualMarkers, 0, 1);
  performanceBox = new QComboBox;
  performanceBox->addItems({"Low", "Normal", "Maximum"});
  performanceBox->setCurrentIndex(1);
  av->addWidget(new QLabel("Performance"), 0, 2);
  av->addWidget(performanceBox, 0, 3);
  aggressorBox = new QComboBox;
  aggressorBox->addItem("AUTO (same group)");
  av->addWidget(new QLabel("Aggressor"), 0, 4);
  av->addWidget(aggressorBox, 0, 5);
  timeStart = spin(0, 1e6, 4);
  timeStop = spin(0, 1e6, 4);
  timeStop->setSpecialValueText("Auto");
  av->addWidget(new QLabel("TDR time [ns]"), 1, 0);
  av->addWidget(timeStart, 1, 1);
  av->addWidget(timeStop, 1, 2);
  peakProminence = spin(0, 100, 3);
  peakProminence->setValue(.15);
  av->addWidget(new QLabel("Peak prominence"), 1, 3);
  av->addWidget(peakProminence, 1, 4);
  auto maskButton = new QPushButton("Frequency Limit…");
  av->addWidget(maskButton, 1, 5);
  connect(maskButton, &QPushButton::clicked, this,
          [this] { frequencyLimit(); });
  config->addWidget(advanced);
  advanced->hide();
  connect(advancedToggle, &QToolButton::toggled, this,
          [this, advanced, advancedToggle, settingsScroll](bool checked) {
    advanced->setVisible(checked);
    advancedToggle->setText(sparamview::ui_language::textFor(this, checked ? "Advanced ▾" : "Advanced ▸"));
    settingsScroll->setMaximumHeight(checked ? 190 : 74);
    if (checked) {
      settingsAction->setChecked(true);
      QTimer::singleShot(0, settingsScroll, [settingsScroll, advanced] {
        settingsScroll->ensureWidgetVisible(advanced);
      });
    }
  });
  connect(settingsAction, &QAction::toggled, advancedToggle,
          [advancedToggle](bool visible) {
    if (!visible) advancedToggle->setChecked(false);
  });
  tabs = new QTabWidget;
  tabs->setObjectName("analysisTabs");
  tabs->setStyleSheet("QTabBar::tab { padding:5px 12px; }");
  auto plotPage = new QWidget;
  auto pv = new QVBoxLayout(plotPage);
  pv->setContentsMargins(10, 4, 10, 4);
  pv->setSpacing(4);
  auto graphOptions = new QHBoxLayout;
  graphOptions->setSpacing(6);
  metricView = new QComboBox;
  metricView->setObjectName("graphMetricView");
  metricView->addItems(labels);
  metricView->setToolTip("Select a plot from the completed analysis. Run Quick Analysis to recalculate.");
  graphOptions->addWidget(metricView);
  metricView->hide();
  tdrView = new QComboBox;
  tdrView->setObjectName("tdrView");
  tdrView->addItems({"Single-ended TDR", "Differential TDR"});
  tdrView->setToolTip(
      "TDR is shown on separate impedance scales for single-ended and differential channels. "
      "With Auto target these are nominally 50 Ω and 100 Ω.");
  tdrView->hide();
  graphOptions->addWidget(tdrView);
  heatCheck = new QCheckBox("Heatmap");
  marginCheck = new QCheckBox("Margin");
  graphOptions->addWidget(heatCheck);
  graphOptions->addWidget(marginCheck);
  graphOptions->addStretch();
  auto areaZoom = new QToolButton;
  areaZoom->setText("Box Zoom"); areaZoom->setCheckable(true);
  graphOptions->addWidget(areaZoom);
  button("+", graphOptions, [this] { plot->zoomBy(.8); });
  button("−", graphOptions, [this] { plot->zoomBy(1.25); });
  button("Previous View", graphOptions, [this] { plot->previousView(); });
  button("Axis Limits", graphOptions, [this] { plot->editAxes(); });
  button("Fit All", graphOptions, [this] { plot->fitView(); });
  pv->addWidget(new WorkspaceControlStrip(graphOptions, "workspaceGraphControls"));
  plot = new PlotWidget;
  pv->addWidget(plot, 1);
  auto readout = new QLabel("Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom");
  readout->setMinimumHeight(18);
  readout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  readout->setWordWrap(true);
  pv->addWidget(readout);
  plot->cursorChanged = [readout](const QString &text) {
    readout->setText(text.isEmpty() ? sparamview::ui_language::textFor(readout, "Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom") : text);
  };
  connect(areaZoom, &QToolButton::toggled, plot, &PlotWidget::setBoxZoom);
  tabs->addTab(plotPage, "Plot");
  compareTable = new QTableWidget;
  compareTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  auto comparePage = new QWidget;
  auto compareLayout = new QVBoxLayout(comparePage);
  compareLayout->setContentsMargins(10, 10, 10, 10);
  compareLayout->addWidget(compareTable);
  tabs->addTab(comparePage, "Compare Revisions");
  analysisSplit = new QSplitter(Qt::Vertical);
  analysisSplit->setObjectName("analysisVerticalSplit");
  analysisSplit->setHandleWidth(10);
  analysisSplit->setChildrenCollapsible(false);
  // Render only when the divider is released, avoiding repeated graph paints.
  analysisSplit->setOpaqueResize(false);
  analysisSplit->addWidget(tabs);
  right->addWidget(analysisSplit, 1);
  resultsPanel = new QWidget;
  resultsPanel->setObjectName("resultsPanel");
  auto resultLayout = new QVBoxLayout(resultsPanel);
  resultLayout->setContentsMargins(10, 4, 10, 4);
  resultLayout->setSpacing(4);
  auto resultBar = new QHBoxLayout;
  resultBar->setSpacing(8);
  summary = new QLabel("No limits configured → N/A");
  summary->setStyleSheet("font-size:15px;font-weight:600");
  resultBar->addWidget(summary);
  resultBar->addStretch();
  resultBar->addWidget(new QLabel("Table Font"));
  tableTextSize = new QComboBox;
  tableTextSize->setObjectName("resultTextSize");
  for (int size : {12, 14, 16, 18, 20})
    tableTextSize->addItem(QString::number(size) + " px", size);
  tableTextSize->setCurrentIndex(1);
  resultBar->addWidget(tableTextSize);
  rankBox = new QComboBox;
  rankBox->addItems({"Worst Margin", "Worst Value", "Channel", "Result"});
  resultBar->addWidget(rankBox);
  button("Compare Revisions", resultBar, [this] { compareRevisions(); });
  resultLayout->addWidget(new WorkspaceControlStrip(resultBar, "workspaceResultControls"));
  table = new QTableWidget;
  table->setObjectName("analysisResults");
  table->setColumnCount(13);
  table->setHorizontalHeaderLabels({"Channel", "Revision", "Analysis",
                                    "Parameter", "Worst", "GHz / ns", "Margin",
                                    "Max ΔDir [dB]", "Δ @ GHz", "Result",
                                    "Aggressor", "Quality", "Termination / Unit"});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->hide();
  table->horizontalHeader()->setStretchLastSection(true);
  table->setMinimumHeight(84);
  table->setAlternatingRowColors(true);
  resultLayout->addWidget(table, 1);
  analysisSplit->addWidget(resultsPanel);
  analysisSplit->setStretchFactor(0, 3);
  analysisSplit->setStretchFactor(1, 2);
  split->addWidget(content);
  split->setStretchFactor(1, 1);
  split->setSizes({250, 1160});
  analysisSplit->setSizes({560, 260});
  notice = new QLabel("Open file → Confirm mapping → Select channels → QUICK ANALYSIS. Pass/fail evaluation requires user-defined limits.");
  notice->setWordWrap(true);
  notice->setStyleSheet("color:#64758b;padding:2px 8px");
  root->addWidget(notice);
  progress = new QProgressBar;
  progress->setMaximumWidth(180);
  progress->setRange(0, 1000);
  progress->hide();
  cancelButton = new QPushButton("Cancel");
  cancelButton->hide();
  statusBar()->addPermanentWidget(progress);
  statusBar()->addPermanentWidget(cancelButton);
  sparamview::ui_language::status(statusBar(),"Ready · Local processing · Source read-only");
  connect(cancelButton, &QPushButton::clicked, this, [this] {
    if (control)
      control->cancelled = true;
    cancelButton->setEnabled(false);
    sparamview::ui_language::status(statusBar(),"Cancelling...");
  });
  connect(revisionBox, &QComboBox::currentIndexChanged, this, [this] {
    updateActiveTouchstoneTitle();
    rebuildChannels();
    if (!busy) { refreshResults(); refreshPlot(); }
    refreshAnalysisContext();
  });
  connect(groupBox, &QComboBox::currentIndexChanged, this, [this] {
    QString group = groupBox->currentText();
    for (int i = 0; i < channels->count(); ++i) {
      auto item = channels->item(i);
      item->setHidden(
          (groupBox->currentIndex() > 0 &&
           item->data(Qt::UserRole + 1).toString() != group) ||
          !item->text().contains(search->text(), Qt::CaseInsensitive));
    }
  });
  connect(channels, &QListWidget::itemSelectionChanged, this, [this] { if (!busy) refreshPlot(); });
  connect(search, &QLineEdit::textChanged, this, [this] {
    QString group = groupBox->currentText();
    for (int i = 0; i < channels->count(); ++i) {
      auto item = channels->item(i);
      item->setHidden(
          !item->text().contains(search->text(), Qt::CaseInsensitive) ||
          (groupBox->currentIndex() > 0 &&
           item->data(Qt::UserRole + 1).toString() != group));
    }
  });
  connect(heatCheck, &QCheckBox::toggled, this, [this] {
    plot->snapshot.heatmap = heatCheck->isChecked();
    plot->invalidateGraph();
    zoom(plot->snapshot.viewStart, plot->snapshot.viewStop);
  });
  connect(marginCheck, &QCheckBox::toggled, this, [this] {
    plot->snapshot.margin = marginCheck->isChecked();
    plot->invalidateGraph();
  });
  connect(rankBox, &QComboBox::currentIndexChanged, this,
          [this] { refreshResults(); });
  connect(metricView, &QComboBox::currentIndexChanged, this,
          [this](int i) { if (!busy && i >= 0 && i < 5) showMetric(si::Metric(i)); });
  connect(tdrView, &QComboBox::currentIndexChanged, this, [this](int) {
    if (!busy && displayed == si::Metric::TDR) {
      plot->resetView();
      refreshPlot();
    }
  });
  connect(channelsAction, &QAction::toggled, channelPanel, &QWidget::setVisible);
  connect(settingsAction, &QAction::toggled, settingsPanel, &QWidget::setVisible);
  connect(tableTextSize, &QComboBox::currentIndexChanged, this,
          [this] { setResultsTextSize(tableTextSize->currentData().toInt()); });
  connect(table, &QTableWidget::itemSelectionChanged, this, [this] {
    if (!busy)
      refreshPlot(true);
  });
  plot->rangeChanged = [this](double a, double b) { zoom(a, b); };
  plot->markerAdded = [this](double f) {
    QString v = num(f * 1e-9, 6);
    if (!manualMarkers->text().isEmpty())
      v = manualMarkers->text() + ", " + v;
    manualMarkers->setText(v);
    run({displayed});
  };
  for (auto w : body->findChildren<QDoubleSpinBox *>())
    connect(w, &QDoubleSpinBox::valueChanged, this, [this] {
      markDirty();
      if (!results.empty() && !restoringProject)
        sparamview::ui_language::message(notice,"Settings changed. Run analysis to apply them. Export uses the last completed analysis settings.");
    });
  for (auto *box : body->findChildren<QCheckBox *>())
    connect(box,&QCheckBox::toggled,this,[this]{markDirty();});
  for (auto *box : {markerBox,performanceBox,revisionBox,groupBox,aggressorBox,metricView})
    connect(box,&QComboBox::currentIndexChanged,this,[this]{markDirty();});
  connect(manualMarkers,&QLineEdit::textChanged,this,[this]{markDirty();});
  connect(search,&QLineEdit::textChanged,this,[this]{markDirty();});
  connect(channels,&QListWidget::itemChanged,this,[this]{markDirty();});
  restoreWorkspaceLayout();
  updateActiveTouchstoneTitle();
  refreshAnalysisContext();
  // Install the header buttons before capturing fixed English UI strings.
  // Startup and test callers can still call the idempotent helper.
  installHeaderProjectButtons();
  using namespace sparamview::ui_language;
  Bindings languageBindings;
  bindMainFixedWidgets(languageBindings, body);
  bindUiText(languageBindings, cancelButton, cancelButton->text(),
       [](QPushButton *w, const QString &s) { w->setText(s); });
  // These combos use indices, not translated text, as their model keys.
  for (auto *box : {markerBox, performanceBox, metricView, rankBox})
    bindCombo(languageBindings, box);
  bindUiText(languageBindings, metricView, metricView->toolTip(),
       [](QComboBox *w, const QString &s) { w->setToolTip(s); });
  bindUiText(languageBindings, tdrView, tdrView->toolTip(),
       [](QComboBox *w, const QString &s) { w->setToolTip(s); });
  const QStringList originalTabs = {"Plot", "Compare Revisions"};
  QStringList originalHeaders;
  for (int i = 0; i < table->columnCount(); ++i)
    originalHeaders << table->horizontalHeaderItem(i)->text();
  languageBindings.push_back([this, advancedToggle, readout, originalTabs, originalHeaders](Language value) {
    const QSignalBlocker blockTable(table);
    quickButton->setText(menuText(QStringLiteral("QUICK ANALYSIS"), value));
    for (int i = 0; i < originalTabs.size(); ++i)
      tabs->setTabText(i, menuText(originalTabs[i], value));
    for (int i = 0; i < originalHeaders.size(); ++i)
      table->horizontalHeaderItem(i)->setText(menuText(originalHeaders[i], value));
    advancedToggle->setText(menuText(advancedToggle->isChecked() ? "Advanced ▾" : "Advanced ▸", value));
    directionButton->setText(menuText(QStringList{"Near → Far", "Far → Near", "Both"}[directionMode], value));
    directionButton->setToolTip(menuText("Direction: Near → Far → Far → Near → Both", value));
    const QString hint = "Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom";
    for (auto previous : {Language::English, Language::Korean, Language::Japanese})
      if (readout->text() == menuText(hint, previous)) {
        readout->setText(menuText(hint, value));
        break;
      }
  });
  message(notice,notice->text());
  message(summary,summary->text());
  message(selectionInfo,selectionInfo->text());
  languageBindings.push_back([this,tdrOptions](Language value){
    tdrOptions->setText(menuText("Option",value));
    tdrOptions->setToolTip(menuText("TDR options: reference impedance, terminations and reflection coefficient",value));
    terminationSummary->setToolTip(menuText("Apply the termination at the opposite end of the selected channel. All other physical ports are matched to their source reference impedances.",value));
    refreshMessage(notice,value);
    refreshMessage(summary,value);
    refreshMessage(selectionInfo,value);
    refreshCompareLanguage(compareTable,value);
    refreshResultLanguage();
    refreshTdrViewLanguage(tdrView,value);
    { const QSignalBlocker blockGroup(groupBox),blockAggressor(aggressorBox);
      if(groupBox->count()) groupBox->setItemText(0,menuText("All groups",value));
      if(aggressorBox->count()) aggressorBox->setItemText(0,menuText("AUTO (same group)",value)); }
    plot->snapshot.displayLanguage=code(value);
    plot->invalidateGraph();
    plot->update();
    refreshStatus(statusBar(),value);
    analysisWindows->refreshLanguage(code(value));
  });
  installMenu(*this, std::move(languageBindings));
}
void MainWindow::markDirty() {
  if(!restoringProject && !revisions.empty()) dirty=true;
}
