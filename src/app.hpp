#pragma once
#include "si/core.hpp"
#include "workers.hpp"
#include <QtConcurrent>
#include <QtWidgets>
#include <functional>
#include <set>

inline QString qs(const std::string &s) {
  return QString::fromUtf8(s.data(), qsizetype(s.size()));
}
inline std::string ss(const QString &s) { return s.toUtf8().toStdString(); }
inline si::fs::path fp(const QString &s) { return si::pathFromUtf8(ss(s)); }
inline QString num(double x, int precision = 3) {
  if (std::isnan(x))
    return "N/A";
  if (std::isinf(x))
    return x < 0 ? "−∞" : "+∞";
  return QString::number(x, 'f', precision);
}
void applyLightTheme(QApplication &);
struct Revision {
  QString name, source;
  QString mappingWarning, mappingSource = "Manual Mapping";
  std::shared_ptr<si::Cache> cache;
  std::vector<si::Channel> channels, candidates;
  bool confirmed = false;
};
QJsonObject channelJson(const si::Channel &);
si::Channel channelFromJson(const QJsonObject &);
QJsonObject settingsJson(const si::Settings &);
si::Settings settingsFromJson(const QJsonObject &);
struct PlotSnapshot {
  std::vector<si::Result> curves;
  si::Settings settings;
  QString project = "Untitled";
  si::Metric metric = si::Metric::RL;
  bool heatmap = false, margin = false, bothDirections = false;
  std::set<std::string> highlightChannelIds;
  double viewStart = si::NaN, viewStop = si::NaN;
  double viewBottom = si::NaN, viewTop = si::NaN;
};
QRectF graphArea(QRectF, const PlotSnapshot &);
std::pair<double, double> graphXRange(const PlotSnapshot &);
std::pair<double, double> graphYRange(const PlotSnapshot &);
QString graphParameterLabel(const PlotSnapshot &);
QString graphQualityLabel(const PlotSnapshot &);
void paintGraph(QPainter &, QRectF, const PlotSnapshot &, bool interactive = false);
QPolygonF simplifyGraphLine(const QPolygonF &, double tolerance);
std::vector<QPolygonF> tdrDisplaySegments(const std::vector<double> &,
                                         const std::vector<double> &, double,
                                         double, double, double, QRectF,
                                         double tolerancePixels = .12);
QImage graphImage(const PlotSnapshot &, QSize size = QSize(2400, 1400));
void savePngFile(const QString &, const QImage &, si::Control * = nullptr);
class PlotWidget : public QWidget {
public:
  PlotSnapshot snapshot;
  std::function<void(double, double)> rangeChanged;
  std::function<void(double)> markerAdded;
  std::function<void(const QString &)> cursorChanged;
  explicit PlotWidget(QWidget *p = nullptr);
  void invalidateGraph();
  size_t graphRenderCount() const { return renderCount; }
  void resetView();
  void fitView();
  void previousView();
  void zoomBy(double factor);
  void setBoxZoom(bool enabled);
  void editAxes();
  void setView(double left, double right, double bottom = si::NaN,
               double top = si::NaN, bool remember = true);

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void wheelEvent(QWheelEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;

private:
  struct View { double left, right, bottom, top; };
  QPixmap graphLayer;
  View cachedAxes{}, cachedView{};
  QSize cachedSize;
  qreal cachedDpr = 0;
  size_t renderCount = 0;
  int tdrDetailWidth = 0;
  bool graphLayerValid() const;
  std::vector<View> history;
  QTimer settleTimer;
  bool boxZoom = false, dragging = false, dragBox = false, pointerInside = false;
  QPointF anchor, pointer;
  View dragView{};
  QRectF selection;
  View currentView() const;
  void rememberView();
  void notifyRange();
  void zoomAt(double factor, QPointF pixel, bool x, bool y);
};
struct ReportOptions {
  bool summary = true, details = true, graphs = true, raw = false;
};
void writeReport(const QString &path, const std::vector<si::Result> &,
                 const std::vector<Revision> &, const si::Settings &,
                 const QString &project, const ReportOptions &,
                 const std::vector<si::Job> &, const PlotSnapshot &current,
                 si::Control *);

class MainWindow : public QMainWindow {
  friend struct NavigationProbe;
  friend struct WorkspaceProbe;
public:
  MainWindow();
  ~MainWindow() override;
  void openPaths(QStringList);
  void loadProject(QString);
  bool selftest(const QString &examples, const QString &output);
  bool validateProject(const QString &project, const QString &output);
  void installHeaderProjectButtons() {
    if (!centralWidget() || !centralWidget()->layout())
      return;
    if (findChild<QPushButton *>("headerOpenProjectButton"))
      return;
    auto *first = centralWidget()->layout()->itemAt(0);
    auto *banner = first ? dynamic_cast<QBoxLayout *>(first->layout()) : nullptr;
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
          this, "Project", {}, "SI Project (*.siproject)");
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

protected:
  void dragEnterEvent(QDragEnterEvent *) override;
  void dropEvent(QDropEvent *) override;
  void closeEvent(QCloseEvent *) override;

private:
  std::vector<Revision> revisions;
  std::vector<si::Result> results;
  std::vector<si::Job> lastJobs;
  std::vector<size_t> resultRows;
  si::Settings settings, lastSettings;
  QString projectName = "Untitled", projectPath;
  bool busy = false, dirty = false;
  std::shared_ptr<si::Control> control;
  QFuture<void> future;
  QThreadPool taskPool;
  QThreadPool viewPool;
  QFuture<void> viewFuture;
  std::shared_ptr<si::Control> viewControl;
  bool viewRunning = false;
  uint64_t viewGeneration = 0;
  std::optional<std::pair<double,double>> pendingView;
  QtWorkerPool analysisPool;
  QWidget *body;
  QComboBox *revisionBox, *groupBox, *markerBox, *performanceBox,
      *rankBox, *aggressorBox;
  QLineEdit *search, *manualMarkers;
  QListWidget *channels;
  QDoubleSpinBox *start, *stop, *target, *tolerance, *timeStart, *timeStop,
      *peakProminence;
  QCheckBox *limits[4], *tdrLimit, *quickChecks[5], *heatCheck, *marginCheck, *tdrReflection;
  QDoubleSpinBox *limitValues[4];
  QToolButton *metricButtons[5]{};
  QTableWidget *table, *compareTable;
  QLabel *summary, *selectionInfo, *notice, *terminationSummary;
  QPushButton *cancelButton, *quickButton, *directionButton;
  QProgressBar *progress;
  PlotWidget *plot;
  QTabWidget *tabs;
  QSplitter *horizontalSplit, *analysisSplit;
  QWidget *channelPanel, *settingsPanel, *resultsPanel;
  QAction *workspaceActions[3], *channelsAction, *settingsAction, *fullscreenAction;
  QComboBox *tableTextSize, *metricView, *tdrView;
  int workspaceMode = 0, savedWorkspaceTab = 0, directionMode = 0;
  bool normalChannelsVisible = true, normalSettingsVisible = true;
  bool fullscreenWasMaximized = false;
  QByteArray normalSplitState, normalHorizontalState;
  void setWorkspaceMode(int);
  void setResultsTextSize(int);
  void setDirectionMode(int);
  void restoreWorkspaceLayout();
  void saveWorkspaceLayout();
  void resetWorkspaceLayout();
  void toggleFullscreen();
  si::Metric displayed = si::Metric::RL;
  void task(std::function<void(si::Control *)> work,
            std::function<void()> done = {});
  void error(const QString &);
  void rebuildChannels();
  void syncSettings();
  void applySettings();
  void editMapping();
  void chooseTerminations();
  void importMapping();
  void exportMapping();
  void run(std::vector<si::Metric>);
  std::string activeRevisionName() const;
  bool hasCurrentResult(si::Metric) const;
  void invalidateRevisionResults(const QString &);
  void refreshResults();
  void showMetric(si::Metric);
  void refreshPlot(bool selectedOnly = false);
  void saveProject(bool as = false);
  void exportXlsx();
  void savePng();
  void saveCsv();
  void preset(bool save);
  void frequencyLimit();
  void compareRevisions();
  void loadDemo();
  void zoom(double, double);
  std::vector<si::Channel> selectedChannels() const;
  const si::Job *jobFor(const si::Result &) const;
};
