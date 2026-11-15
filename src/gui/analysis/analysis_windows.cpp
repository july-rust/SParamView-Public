#include "gui/analysis/analysis_windows.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace si::gui {
namespace {
QString labelFor(Feature feature) {
  switch (feature) {
    case Feature::Ild: return QStringLiteral("ILD");
    case Feature::Icn: return QStringLiteral("ICN");
    case Feature::Eye: return QStringLiteral("Eye Diagram");
    case Feature::Com: return QStringLiteral("COM");
  }
  return QStringLiteral("Unknown analysis");
}
QString statusName(si::channel::Status status) {
  using S = si::channel::Status;
  switch (status) {
    case S::Valid: return QStringLiteral("VALID");
    case S::Warning: return QStringLiteral("WARNING");
    case S::Stale: return QStringLiteral("STALE");
    case S::NotEvaluated: return QStringLiteral("NOT EVALUATED");
    case S::Unsupported: return QStringLiteral("UNSUPPORTED");
    case S::InvalidSource: return QStringLiteral("INVALID SOURCE");
    case S::Cancelled: return QStringLiteral("CANCELLED");
    case S::Error: return QStringLiteral("ERROR");
  }
  return QStringLiteral("ERROR");
}
}
struct AsyncScheduler::Ticket {
  Feature feature;
  quint64 generation;
  QPointer<QObject> recipient;
  std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
  std::shared_ptr<std::atomic_int> progress = std::make_shared<std::atomic_int>(0);
  QFutureWatcher<WorkOutcome> *watcher = nullptr;
  WorkCompletion completion;
};

AsyncScheduler::AsyncScheduler(QObject *parent) : QObject(parent) {
  pool_.setMaxThreadCount(4);
  pool_.setExpiryTimeout(30000);
}
AsyncScheduler::~AsyncScheduler() {
  invalidate();
  // Workers capture only value data/atomics, never QObject pointers. Shutdown
  // cooperatively; an engine must check cancellation at bounded intervals.
  pool_.waitForDone();
}
quint64 AsyncScheduler::submit(Feature feature, QObject *recipient,
                              AsyncWork work, WorkCompletion completion) {
  if (!recipient || !work || !completion)
    throw std::invalid_argument("Analysis job requires a live recipient, work and completion");
  if (running(feature) || tickets_.size() >= featureCount)
    throw std::runtime_error("Analysis worker capacity exceeded");
  const quint64 id = ++serial_;
  auto ticket = std::make_shared<Ticket>();
  ticket->feature = feature;
  ticket->generation = generation_;
  ticket->recipient = recipient;
  ticket->completion = std::move(completion);
  auto *watcher = new QFutureWatcher<WorkOutcome>(this);
  ticket->watcher = watcher;
  tickets_.insert(id, ticket);
  connect(watcher, &QFutureWatcher<WorkOutcome>::finished, this, [this, id] {
    auto ticket = tickets_.take(id);
    if (!ticket) return;
    const bool stale = ticket->generation != generation_;
    WorkOutcome outcome;
    try {
      outcome = ticket->watcher->result();
    } catch (const std::exception &e) {
      outcome = {si::channel::Status::Error, QString::fromUtf8(e.what())};
    } catch (...) {
      outcome = {si::channel::Status::Error, QStringLiteral("Unknown worker failure")};
    }
    ticket->watcher->deleteLater();
    // No callback into a destroyed window. A cancelled/stale calculation must
    // never be published as a valid result.
    if (ticket->recipient) ticket->completion(outcome, stale);
  });
  try {
    auto future = QtConcurrent::run(&pool_,
        [work = std::move(work), cancelled = ticket->cancelled,
         progress = ticket->progress]() -> WorkOutcome {
          try {
            if (cancelled->load(std::memory_order_acquire))
              return {si::channel::Status::Cancelled, QStringLiteral("Cancelled")};
            auto outcome = work(*cancelled, *progress);
            if (cancelled->load(std::memory_order_acquire))
              return {si::channel::Status::Cancelled, QStringLiteral("Cancelled")};
            return outcome;
          } catch (const std::exception &e) {
            return {si::channel::Status::Error, QString::fromUtf8(e.what())};
          } catch (...) {
            return {si::channel::Status::Error, QStringLiteral("Unknown worker failure")};
          }
        });
    watcher->setFuture(future);
  } catch (...) {
    tickets_.remove(id);
    delete watcher;
    throw;
  }
  return id;
}
void AsyncScheduler::cancel(Feature feature) {
  for (auto it = tickets_.begin(); it != tickets_.end(); ++it)
    if (it.value()->feature == feature)
      it.value()->cancelled->store(true, std::memory_order_release);
}
void AsyncScheduler::invalidate() {
  ++generation_;
  for (auto it = tickets_.begin(); it != tickets_.end(); ++it)
    it.value()->cancelled->store(true, std::memory_order_release);
}
bool AsyncScheduler::running(Feature feature) const {
  for (auto it = tickets_.cbegin(); it != tickets_.cend(); ++it)
    if (it.value()->feature == feature) return true;
  return false;
}
int AsyncScheduler::progress(Feature feature) const {
  for (auto it = tickets_.cbegin(); it != tickets_.cend(); ++it)
    if (it.value()->feature == feature)
      return std::clamp(it.value()->progress->load(std::memory_order_relaxed), 0, 100);
  return 0;
}
int AsyncScheduler::inFlight() const { return int(tickets_.size()); }

AnalysisWindow::AnalysisWindow(Feature feature, AsyncScheduler *scheduler,
                               QWidget *owner)
    : QWidget(owner, Qt::Window), feature_(feature), scheduler_(scheduler) {
  setAttribute(Qt::WA_DeleteOnClose);
  setObjectName(QStringLiteral("channelAnalysisWindow") + labelFor(feature));
  setWindowTitle(QStringLiteral("SParamView · ") + labelFor(feature));
  resize(940, 640);
  auto *outer = new QVBoxLayout(this);
  auto *header = new QLabel(QStringLiteral("<b>") + labelFor(feature_) +
       QStringLiteral("</b> — Channel Analysis"));
  header->setTextFormat(Qt::RichText);
  outer->addWidget(header);
  auto *form = new QFormLayout;
  source_ = new QComboBox;
  source_->setObjectName(QStringLiteral("analysisSource"));
  victim_ = new QComboBox;
  victim_->setObjectName(QStringLiteral("analysisVictim"));
  aggressor_ = new QComboBox;
  aggressor_->setObjectName(QStringLiteral("analysisAggressor"));
  direction_ = new QComboBox;
  direction_->addItems({QStringLiteral("Forward"), QStringLiteral("Reverse")});
  direction_->setObjectName(QStringLiteral("analysisDirection"));
  fromGhz_ = new QDoubleSpinBox;
  toGhz_ = new QDoubleSpinBox;
  for (auto *spin : {fromGhz_, toGhz_}) {
    spin->setRange(0, 100000);
    spin->setDecimals(6);
    spin->setSuffix(QStringLiteral(" GHz"));
  }
  toGhz_->setValue(30);
  form->addRow(QStringLiteral("Source / Revision"), source_);
  form->addRow(QStringLiteral("Victim Channel"), victim_);
  if (feature_ == Feature::Icn || feature_ == Feature::Com)
    form->addRow(QStringLiteral("Aggressor"), aggressor_);
  else aggressor_->hide();
  form->addRow(QStringLiteral("Direction"), direction_);
  form->addRow(QStringLiteral("Start frequency"), fromGhz_);
  form->addRow(QStringLiteral("Stop frequency"), toGhz_);
  outer->addLayout(form);
  auto *controls = new QHBoxLayout;
  run_ = new QPushButton(QStringLiteral("Run"));
  run_->setObjectName(QStringLiteral("analysisRun"));
  run_->setToolTip(QStringLiteral("Numerical engine pending in a later 012 stage"));
  run_->setEnabled(false);
  cancel_ = new QPushButton(QStringLiteral("Cancel"));
  cancel_->setObjectName(QStringLiteral("analysisCancel"));
  cancel_->setEnabled(false);
  controls->addWidget(run_);
  controls->addWidget(cancel_);
  progress_ = new QProgressBar;
  progress_->setRange(0, 100);
  progress_->setValue(0);
  progress_->setObjectName(QStringLiteral("analysisProgress"));
  controls->addWidget(progress_, 1);
  outer->addLayout(controls);
  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("analysisStatus"));
  statusLabel_->setWordWrap(true);
  outer->addWidget(statusLabel_);
  graphPlaceholder_ = new QLabel(QStringLiteral("No numerical data. Analysis engine is not installed."));
  graphPlaceholder_->setObjectName(QStringLiteral("analysisGraphPlaceholder"));
  graphPlaceholder_->setAlignment(Qt::AlignCenter);
  graphPlaceholder_->setMinimumHeight(180);
  graphPlaceholder_->setFrameStyle(QFrame::StyledPanel);
  outer->addWidget(graphPlaceholder_, 1);
  auto *exports = new QHBoxLayout;
  csv_ = new QPushButton(QStringLiteral("Export CSV"));
  png_ = new QPushButton(QStringLiteral("Export PNG"));
  csv_->setEnabled(false);
  png_->setEnabled(false);
  exports->addWidget(csv_);
  exports->addWidget(png_);
  exports->addStretch();
  outer->addLayout(exports);
  setStatus(si::channel::Status::NotEvaluated,
            QStringLiteral("Engine not yet implemented; no simulated or conformant results"));
  connect(cancel_, &QPushButton::clicked, this, [this] { cancelWork(); });
  connect(source_, &QComboBox::currentIndexChanged, this, [this] {
    QSignalBlocker block(victim_);
    QSignalBlocker aggressiveBlock(aggressor_);
    victim_->clear();
    aggressor_->clear();
    const int index = source_->currentIndex();
    if (index >= 0 && index < context_.sources.size()) {
      const auto &selected = context_.sources[index];
      victim_->addItems(selected.channels);
      aggressor_->addItem(QStringLiteral("Select aggressor..."));
      aggressor_->addItems(selected.channels);
    }
    invalidateRequest();
  });
  for (auto *combo : {victim_, aggressor_, direction_})
    connect(combo, &QComboBox::currentIndexChanged, this, [this] { invalidateRequest(); });
  for (auto *spin : {fromGhz_, toGhz_})
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { invalidateRequest(); });
  progressPoll_.setInterval(40);
  connect(&progressPoll_, &QTimer::timeout, this, [this] {
    if (scheduler_ && scheduler_->running(feature_))
      progress_->setValue(scheduler_->progress(feature_));
    else progressPoll_.stop();
  });
}
void AnalysisWindow::setStatus(si::channel::Status status, const QString &details) {
  status_ = status;
  statusLabel_->setText(statusName(status) + QStringLiteral(" — ") + details);
  statusLabel_->setToolTip(statusLabel_->text());
}
QString AnalysisWindow::selectionIdentity() const {
  const int index = source_->currentIndex();
  if (index < 0 || index >= context_.sources.size()) return {};
  const auto &item = context_.sources[index];
  return item.sha256 + QLatin1Char('|') + item.revision + QLatin1Char('|') +
    item.mappingDigest + QLatin1Char('|') + victim_->currentText() +
    QLatin1Char('|') + aggressor_->currentText() + QLatin1Char('|') +
    QString::number(direction_->currentIndex()) + QLatin1Char('|') +
    QString::number(fromGhz_->value(), 'g', 17) + QLatin1Char('|') +
    QString::number(toGhz_->value(), 'g', 17);
}
void AnalysisWindow::invalidateRequest() {
  const bool hadWork = scheduler_ && scheduler_->running(feature_);
  ++requestGeneration_;
  if (scheduler_) scheduler_->cancel(feature_);
  cancel_->setEnabled(false);
  progress_->setValue(0);
  setStatus(hadWork || status_ == si::channel::Status::Valid ||
                    status_ == si::channel::Status::Warning
        ? si::channel::Status::Stale : si::channel::Status::NotEvaluated,
            QStringLiteral("Inputs changed; previous analysis is invalid"));
}
void AnalysisWindow::setContext(const AnalysisContext &context) {
  const bool hadWork = scheduler_ && scheduler_->running(feature_);
  const bool hadResult = status_ == si::channel::Status::Valid ||
                         status_ == si::channel::Status::Warning;
  if (scheduler_) scheduler_->cancel(feature_);
  context_ = context;
  ++requestGeneration_;
  progress_->setValue(0);
  cancel_->setEnabled(false);
  QSignalBlocker sourceBlock(source_);
  QSignalBlocker victimBlock(victim_);
  QSignalBlocker aggressorBlock(aggressor_);
  source_->clear();
  victim_->clear();
  aggressor_->clear();
  for (const auto &item : context_.sources)
    source_->addItem(item.revision);
  if (context_.activeSource >= 0 && context_.activeSource < context_.sources.size()) {
    source_->setCurrentIndex(context_.activeSource);
    const auto &item = context_.sources[context_.activeSource];
    victim_->addItems(item.channels);
    aggressor_->addItem(QStringLiteral("Select aggressor..."));
    aggressor_->addItems(item.channels);
  } else source_->setCurrentIndex(-1);
  setStatus(hadWork || hadResult ? si::channel::Status::Stale
           : si::channel::Status::NotEvaluated,
           QStringLiteral("Context updated; analysis engine not installed"));
}
bool AnalysisWindow::startWork(AsyncWork work) {
  const int index = source_->currentIndex();
  if (!scheduler_ || !work || index < 0 || index >= context_.sources.size())
    return false;
  const auto &item = context_.sources[index];
  if (!item.mappingConfirmed || item.sha256.isEmpty() || item.mappingDigest.isEmpty() ||
      victim_->currentText().isEmpty() || fromGhz_->value() >= toGhz_->value()) {
    setStatus(si::channel::Status::InvalidSource,
              QStringLiteral("Confirmed source, mapping and frequency band required"));
    return false;
  }
  if (scheduler_->running(feature_)) return false;
  const quint64 requestGeneration = ++requestGeneration_;
  const quint64 contextGeneration = context_.generation;
  const QString selected = selectionIdentity();
  QPointer<AnalysisWindow> guard(this);
  try {
    scheduler_->submit(feature_, this, std::move(work),
      [guard, selected, requestGeneration, contextGeneration]
      (const WorkOutcome &outcome, bool stale) {
        if (!guard) return;
        guard->progressPoll_.stop();
        guard->cancel_->setEnabled(false);
        if (stale || requestGeneration != guard->requestGeneration_ ||
            contextGeneration != guard->context_.generation ||
            selected != guard->selectionIdentity()) {
          guard->setStatus(si::channel::Status::Stale,
              QStringLiteral("Older result discarded after context/settings change"));
          return;
        }
        guard->progress_->setValue(outcome.status == si::channel::Status::Valid ? 100 : 0);
        guard->setStatus(outcome.status, outcome.diagnostic);
      });
  } catch (const std::exception &e) {
    setStatus(si::channel::Status::Error, QString::fromUtf8(e.what()));
    return false;
  }
  cancel_->setEnabled(true);
  setStatus(si::channel::Status::NotEvaluated, QStringLiteral("Calculation running"));
  progressPoll_.start();
  return true;
}
void AnalysisWindow::cancelWork() {
  if (scheduler_) scheduler_->cancel(feature_);
  cancel_->setEnabled(false);
  setStatus(si::channel::Status::Cancelled,
            QStringLiteral("Cancellation requested; pending work cannot publish results"));
}
void AnalysisWindow::closeEvent(QCloseEvent *event) {
  if (scheduler_ && scheduler_->running(feature_)) cancelWork();
  progressPoll_.stop();
  QWidget::closeEvent(event);
}

AnalysisWindowManager::AnalysisWindowManager(QMainWindow *owner)
    : QObject(owner), owner_(owner), scheduler_(this) {}
AnalysisWindowManager::~AnalysisWindowManager() {
  scheduler_.invalidate();
  for (auto &window : windows_)
    if (window) window->close();
}
void AnalysisWindowManager::installMenu() {
  auto *menu = owner_->menuBar()->addMenu(QStringLiteral("Analysis"));
  menu->setObjectName(QStringLiteral("channelAnalysisMenu"));
  for (int i = 0; i < featureCount; ++i) {
    const auto feature = static_cast<Feature>(i);
    auto *action = menu->addAction(labelFor(feature));
    const QStringList names = {QStringLiteral("ILD"), QStringLiteral("ICN"),
                               QStringLiteral("Eye"), QStringLiteral("COM")};
    action->setObjectName(QStringLiteral("analysisAction") + names[i]);
    action->setEnabled(false);
    action->setToolTip(QStringLiteral("Available after the numerical engine is verified"));
    connect(action, &QAction::triggered, this, [this, feature] {
      if (available_[int(feature)]) openWindow(feature);
    });
    actions_[i] = action;
  }
}
void AnalysisWindowManager::setContext(AnalysisContext context) {
  context.generation = ++contextGeneration_;
  context_ = std::move(context);
  scheduler_.invalidate();
  for (auto &window : windows_)
    if (window) window->setContext(context_);
}
void AnalysisWindowManager::setEngineAvailable(Feature feature, bool available) {
  available_[int(feature)] = available;
  if (auto *action = actions_[int(feature)])
    action->setEnabled(available);
}
AnalysisWindow *AnalysisWindowManager::openWindow(Feature feature) {
  const int index = int(feature);
  if (windows_[index]) {
    windows_[index]->show();
    windows_[index]->raise();
    windows_[index]->activateWindow();
    return windows_[index];
  }
  auto *window = new AnalysisWindow(feature, &scheduler_, owner_);
  window->setContext(context_);
  windows_[index] = window;
  connect(window, &QObject::destroyed, this, [this, index] {
    windows_[index] = nullptr;
  });
  window->show();
  window->raise();
  window->activateWindow();
  return window;
}
} // namespace si::gui
