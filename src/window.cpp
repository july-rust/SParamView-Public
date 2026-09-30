#include "app.hpp"
#include <cmath>
#include <map>
#include <set>
#include <regex>
#include "si/mapping_policy.hpp"
#include "si/direction_policy.hpp"
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

MainWindow::MainWindow() {
  settings.tdrStopSeconds=3e-9;
  auto title = "SParamView · " + qs(si::version);
  const auto architecture = QString::fromLatin1(architectureName());
  if (architecture == "ARM64")
    title += " · " + architecture;
  setWindowTitle(title);
  resize(1440, 980);
  setAcceptDrops(true);
  auto file = menuBar()->addMenu("File");
  file->addAction(
      "Open Touchstone...", this,
      [this] {
        openPaths(QFileDialog::getOpenFileNames(
            this, "Select Touchstone Files", {},
            "Touchstone (*.s*p *.ts);;All files (*)"));
      },
      QKeySequence::Open);
  file->addAction("Open Project...", this, [this] {
    auto p = QFileDialog::getOpenFileName(this, "Project", {},
                                          "SI Project (*.siproject)");
    if (!p.isEmpty())
      loadProject(p);
  });
  file->addAction(
      "Save Project", this, [this] { saveProject(); }, QKeySequence::Save);
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
    workspaceActions[i]->setCheckable(true);
    workspaceActions[i]->setShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)));
    modes->addAction(workspaceActions[i]);
    connect(workspaceActions[i], &QAction::triggered, this,
            [this, i] { setWorkspaceMode(i); });
  }
  workspaceActions[0]->setChecked(true);
  viewMenu->addSeparator();
  channelsAction = viewMenu->addAction("Channel List");
  channelsAction->setCheckable(true);
  channelsAction->setChecked(true);
  settingsAction = viewMenu->addAction("Analysis Settings");
  settingsAction->setCheckable(true);
  settingsAction->setChecked(true);
  fullscreenAction = viewMenu->addAction("Full Screen");
  fullscreenAction->setCheckable(true);
  fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
  connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
  auto exitFullscreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(exitFullscreen, &QShortcut::activated, this, [this] {
    if (isFullScreen()) toggleFullscreen();
    if (channels) channels->clearSelection();
  });
  viewMenu->addAction("Reset Layout", this, &MainWindow::resetWorkspaceLayout);
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
        [this] { notice->setText("Source SHA-256 verified"); });
  });
  menuBar()->addAction("About", this, [this] {
    QMessageBox::information(
        this, "About SParamView",
        QString("SParamView\nVersion %1\nBuild: Windows %2\n\nCopyright © 2026 july\nsparamview@gmail.com\nLicensed under the MIT License\n\nC++20 / Qt 6\nProcessing is local. Original Touchstone files are not modified.")
            .arg(si::version)
            .arg(QString::fromLatin1(architectureName())));
  });
  body = new QWidget;
  setCentralWidget(body);
  auto root = new QVBoxLayout(body);
  root->setContentsMargins(12, 10, 12, 8);
  root->setSpacing(8);
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
        this, "Touchstone", {}, "Touchstone (*.s*p *.ts);;All files (*)"));
  });
  button("Open Sample", banner, [this] { loadDemo(); });
  button("Export XLSX", banner, [this] { exportXlsx(); });
  root->addLayout(banner);
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
  right->setContentsMargins(12, 8, 8, 8);
  right->setSpacing(10);
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
  right->addLayout(workspaceBar);
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
    limitValues[i] = spin(-600,100,2); limitValues[i]->setSuffix(" dB"); limitValues[i]->setMaximumWidth(86);
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
  tolerance = spin(0,1000,1); tolerance->setValue(10); tolerance->setSuffix(" %"); tolerance->setMaximumWidth(72);
  target = spin(0,10000,1); target->setSpecialValueText("Auto Z₀"); target->setSuffix(" Ω"); target->setMaximumWidth(92);
  auto tdrOptions = new QToolButton; tdrOptions->setText("⚙"); tdrOptions->setToolTip("TDR options: reference impedance, terminations and reflection coefficient"); connect(tdrOptions, &QToolButton::clicked, this, [this] { chooseTerminations(); });
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
    advancedToggle->setText(checked ? "Advanced ▾" : "Advanced ▸");
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
  auto plotPage = new QWidget;
  auto pv = new QVBoxLayout(plotPage);
  pv->setContentsMargins(10, 8, 10, 8);
  pv->setSpacing(8);
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
  pv->addLayout(graphOptions);
  plot = new PlotWidget;
  pv->addWidget(plot, 1);
  auto readout = new QLabel("Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom");
  readout->setMinimumHeight(22);
  readout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  readout->setWordWrap(true);
  pv->addWidget(readout);
  plot->cursorChanged = [readout](const QString &text) {
    readout->setText(text.isEmpty() ? "Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom" : text);
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
  resultLayout->setContentsMargins(10, 8, 10, 8);
  resultLayout->setSpacing(8);
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
  resultLayout->addLayout(resultBar);
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
  table->setMinimumHeight(100);
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
  notice->setStyleSheet("color:#64758b;padding:4px 8px");
  root->addWidget(notice);
  progress = new QProgressBar;
  progress->setMaximumWidth(180);
  progress->setRange(0, 1000);
  progress->hide();
  cancelButton = new QPushButton("Cancel");
  cancelButton->hide();
  statusBar()->addPermanentWidget(progress);
  statusBar()->addPermanentWidget(cancelButton);
  statusBar()->showMessage("Ready · Local processing · Source read-only");
  connect(cancelButton, &QPushButton::clicked, this, [this] {
    if (control)
      control->cancelled = true;
    cancelButton->setEnabled(false);
    statusBar()->showMessage("Cancelling...");
  });
  connect(revisionBox, &QComboBox::currentIndexChanged, this, [this] {
    rebuildChannels();
    if (!busy) { refreshResults(); refreshPlot(); }
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
      dirty = true;
      if (!results.empty())
        notice->setText("Settings changed. Run analysis to apply them. Export uses the last completed analysis settings.");
    });
  restoreWorkspaceLayout();
}

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
    if (fullscreenWasMaximized) showMaximized();
    else showNormal();
  } else {
    fullscreenWasMaximized = isMaximized();
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
void MainWindow::run(std::vector<si::Metric> metrics) {
  if (busy)
    return;
  try {
    syncSettings();
    if (metrics.empty())
      throw si::Error("Select at least one analysis type.");
    auto selected = selectedChannels();
    if (selected.empty())
      throw si::Error("Select channels to analyze.");
    for (auto &r : revisions) {
      if (!r.confirmed)
        throw si::Error("Mapping confirmation required: " + ss(r.name));
      si::validateMapping(r.channels, r.cache->meta());
    }
    auto jobs = std::make_shared<std::vector<si::Job>>();
  auto appendDirections = [&](si::Job base) {
    for (bool reverse : {false, true}) {
      if (!si::directionAvailable(base, reverse)) continue;
      auto job = base;
      job.reverse = reverse;
      jobs->push_back(std::move(job));
    }
  };
  for (auto &rev : revisions) {
    auto matches = si::matchChannels(selected, rev.channels);
    for (size_t i = 0; i < selected.size(); ++i) {
      if (matches[i] < 0) continue;
      const si::Channel victim = rev.channels[size_t(matches[i])];
      for (auto metric : metrics) {
        if (metric == si::Metric::NEXT || metric == si::Metric::FEXT) {
          for (const auto &a : rev.channels) {
            if (a.id == victim.id || a.group != victim.group) continue;
            if (aggressorBox->currentIndex() != 0 &&
                qs(a.name) != aggressorBox->currentText()) continue;
            appendDirections(si::Job{rev.cache, victim, a, metric, ss(rev.name)});
          }
        } else {
          appendDirections(si::Job{rev.cache, victim, std::nullopt, metric,
                                   ss(rev.name)});
        }
      }
    }
  }
  if (jobs->empty())
    throw si::Error("No selected channels have valid mapped endpoints for the requested analysis.");
    *jobs=si::expandTdrJobs(*jobs,settings);
    auto set = settings;
    auto output = std::make_shared<std::vector<si::Result>>();
    displayed = metrics.front();
    task(
        [this, jobs, set, output](si::Control *c) {
          *output =
              si::runJobs(*jobs, set, c,
                          [this](size_t n, const std::function<void()> &work) { analysisPool.run(n, work); });
          for (size_t i = 0; i < jobs->size() && i < output->size(); ++i) {
          const auto &job = (*jobs)[i];
          if (!job.reverse || *job.reverse ||
              (job.metric != si::Metric::RL && job.metric != si::Metric::IL))
            continue;
          auto same = [&](const si::Job &other) {
            return other.reverse && *other.reverse && other.cache == job.cache &&
                   other.metric == job.metric && other.termination == job.termination &&
                   other.revision == job.revision && other.victim.id == job.victim.id &&
                   (other.aggressor ? other.aggressor->id : "") ==
                       (job.aggressor ? job.aggressor->id : "");
          };
          auto it = std::find_if(jobs->begin(), jobs->end(), same);
          if (it == jobs->end()) continue;
          const size_t j = size_t(it - jobs->begin());
          if (j >= output->size() || (*output)[i].plotX.empty() ||
              (*output)[j].plotX.empty())
            continue;
          try {
            const auto delta = si::directionDelta(*job.cache, job.victim,
                                                  job.metric, set, c);
            (*output)[i].directionDelta = (*output)[j].directionDelta = delta.maximumDb;
            (*output)[i].directionDeltaX = (*output)[j].directionDeltaX = delta.frequency;
          } catch (const si::Error &) {
            // Directional delta is diagnostic. Never turn an otherwise valid
            // one-direction analysis into a failed run when the pair is unavailable.
          }
        }
        },
        [this, jobs, set, output] {
          lastJobs = std::move(*jobs);
          results = std::move(*output);
          lastSettings = set;
          dirty = true;
          refreshResults();
          showMetric(displayed);
          notice->setText("Analysis complete. Frequency/time and margin values use full-resolution data. TDR starts from the lowest source frequency; check the preview quality notes.");
        });
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
void MainWindow::refreshResults() {
  QString sort = rankBox->currentIndex() == 0 ? "Margin" : rankBox->currentIndex() == 1 ? "Value" : rankBox->currentIndex() == 2 ? "Channel" : "Result";
  si::rankResults(results, ss(sort)); resultRows.clear();
  const auto activeRevision = activeRevisionName();
  for (size_t i=0;i<results.size();++i)
    if (results[i].revision == activeRevision &&
        (directionMode==2 || results[i].reverse==(directionMode==1)))
      resultRows.push_back(i);
  QSignalBlocker block(table); table->setRowCount(int(resultRows.size())); std::vector<std::string> statuses; int ng=0,na=0,ok=0;
  for (size_t rowIndex=0; rowIndex<resultRows.size(); ++rowIndex) {
    auto &r=results[resultRows[rowIndex]]; statuses.push_back(r.status); if(r.status=="NG")++ng; else if(r.status=="OK")++ok; else ++na;
    QStringList row{qs(r.channel),qs(r.revision),qs(si::name(r.metric)),qs(r.parameter),num(r.worst),num(r.worstX*(r.metric==si::Metric::TDR?1e9:1e-9),4),num(r.margin),(r.metric==si::Metric::RL||r.metric==si::Metric::IL)?num(r.directionDelta):"",(r.metric==si::Metric::RL||r.metric==si::Metric::IL)?num(r.directionDeltaX*1e-9,4):"",qs(r.status),qs(r.aggressor),qs(r.quality),r.metric==si::Metric::TDR?qs(si::name(r.termination,r.parameter.starts_with("Sdd")))+(r.reflection?" / rho":" / ohm"):"dB"};
    QString tip=qs(r.note)+"\nWorst margin at "+num(r.marginX*(r.metric==si::Metric::TDR?1e9:1e-9),6)+(r.metric==si::Metric::TDR?" ns":" GHz")+"\n"+qs(r.direction);
    if((r.metric==si::Metric::RL||r.metric==si::Metric::IL)&&!std::isnan(r.directionDelta)) tip += "\nMax directional Δ: "+num(r.directionDelta,4)+" dB @ "+num(r.directionDeltaX*1e-9,6)+" GHz";
    for(int j=0;j<row.size();++j){auto item=new QTableWidgetItem(row[j]);item->setToolTip(tip);if(r.status=="NG")item->setBackground(QColor("#fff0f1"));if(j==9)item->setForeground(r.status=="NG"?QColor("#c6374a"):r.status=="OK"?QColor("#00856c"):QColor("#6d7f94"));table->setItem(int(rowIndex),j,item);}
  }
  table->resizeColumnsToContents(); table->setColumnWidth(3,160); summary->setText("Overall "+qs(si::overall(statuses))+QString("   ·   OK %1   NG %2   N/A %3").arg(ok).arg(ng).arg(na));
}
void MainWindow::showMetric(si::Metric m) {
  displayed = m;
  { QSignalBlocker block(metricView); metricView->setCurrentIndex(int(m)); }
  for (int i=0;i<5;++i) metricButtons[i]->setChecked(i==int(m));
  plot->resetView();
  refreshPlot();
  tabs->setCurrentIndex(0);
}
const si::Job *MainWindow::jobFor(const si::Result &r) const {
  for (auto &j : lastJobs) {
    const bool reverse = j.reverse.value_or(lastSettings.reverse);
    if (j.metric == r.metric && j.termination == r.termination && j.victim.id == r.channelId && j.revision == r.revision && reverse == r.reverse && (j.aggressor ? j.aggressor->id : "") == r.aggressorId) return &j;
  }
  return nullptr;
}
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
  plot->snapshot.curves = std::move(curves);
  plot->snapshot.settings = lastSettings; plot->snapshot.settings.reverse = directionMode == 1;
  plot->snapshot.bothDirections = directionMode == 2; plot->snapshot.highlightChannelIds.clear();
  for (auto *item : channels->selectedItems()) plot->snapshot.highlightChannelIds.insert(ss(item->data(Qt::UserRole).toString()));
  plot->snapshot.project = projectName;
  plot->snapshot.metric = displayed;
  plot->snapshot.heatmap = heatCheck->isChecked();
  plot->snapshot.margin = marginCheck->isChecked();
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
