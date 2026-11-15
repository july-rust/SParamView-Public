#pragma once
// 012_04: GUI-only scaffolding; numerical engines are introduced in later steps.
#include "si/channel_analysis/common.hpp"
#include <QtConcurrent>
#include <QtWidgets>
#include <array>
#include <atomic>
#include <functional>
#include <memory>

namespace si::gui {
enum class Feature : int { Ild = 0, Icn = 1, Eye = 2, Com = 3 };
constexpr int featureCount = 4;

struct SourceItem {
  QString revision, sha256, mappingDigest;
  QStringList channels;
  bool mappingConfirmed = false;
};
struct AnalysisContext {
  quint64 generation = 0;
  QVector<SourceItem> sources;
  int activeSource = -1;
};
struct WorkOutcome {
  si::channel::Status status = si::channel::Status::NotEvaluated;
  QString diagnostic;
};
// Work MUST NOT touch a QObject or GUI object. Capture immutable value types and
// a verified SourceSnapshot by value when numerical engines are added.
using AsyncWork = std::function<WorkOutcome(const std::atomic_bool &,
                                            std::atomic_int &)>;
using WorkCompletion = std::function<void(const WorkOutcome &, bool stale)>;

class AsyncScheduler final : public QObject {
public:
  explicit AsyncScheduler(QObject *parent = nullptr);
  ~AsyncScheduler() override;
  quint64 submit(Feature feature, QObject *recipient,
                 AsyncWork work, WorkCompletion completion);
  void cancel(Feature feature);
  void invalidate();
  bool running(Feature feature) const;
  int progress(Feature feature) const;
  int inFlight() const;
  quint64 generation() const { return generation_; }

private:
  struct Ticket;
  QThreadPool pool_;
  QHash<quint64, std::shared_ptr<Ticket>> tickets_;
  quint64 serial_ = 0;
  quint64 generation_ = 0;
};

class AnalysisWindow final : public QWidget {
public:
  AnalysisWindow(Feature feature, AsyncScheduler *scheduler,
                 QWidget *owner = nullptr);
  Feature feature() const { return feature_; }
  void setContext(const AnalysisContext &context);
  // Engine adapters call this only after validated input snapshot creation.
  // This does not enable the public Run action without an installed engine.
  bool startWork(AsyncWork work);
  void cancelWork();
  si::channel::Status status() const { return status_; }
  QPushButton *runButton() const { return run_; }
  QPushButton *cancelButton() const { return cancel_; }

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void invalidateRequest();
  void setStatus(si::channel::Status status, const QString &details);
  QString selectionIdentity() const;
  Feature feature_;
  AsyncScheduler *scheduler_ = nullptr;
  AnalysisContext context_;
  quint64 requestGeneration_ = 0;
  si::channel::Status status_ = si::channel::Status::NotEvaluated;
  QComboBox *source_ = nullptr, *victim_ = nullptr, *aggressor_ = nullptr;
  QComboBox *direction_ = nullptr;
  QDoubleSpinBox *fromGhz_ = nullptr, *toGhz_ = nullptr;
  QLabel *statusLabel_ = nullptr, *graphPlaceholder_ = nullptr;
  QProgressBar *progress_ = nullptr;
  QPushButton *run_ = nullptr, *cancel_ = nullptr, *csv_ = nullptr, *png_ = nullptr;
  QTimer progressPoll_;
};

class AnalysisWindowManager final : public QObject {
public:
  explicit AnalysisWindowManager(QMainWindow *owner);
  ~AnalysisWindowManager() override;
  void installMenu();
  void setContext(AnalysisContext context);
  // Deferred engine registration; all four actions remain disabled in 012_04.
  void setEngineAvailable(Feature feature, bool available);
  AnalysisWindow *openWindow(Feature feature);
  QAction *action(Feature feature) const { return actions_.at(int(feature)); }
  AnalysisWindow *existingWindow(Feature feature) const {
    return windows_.at(int(feature));
  }
  AsyncScheduler &scheduler() { return scheduler_; }
  const AnalysisContext &context() const { return context_; }

private:
  QMainWindow *owner_ = nullptr;
  AsyncScheduler scheduler_;
  std::array<QPointer<AnalysisWindow>, featureCount> windows_{};
  std::array<QAction *, featureCount> actions_{};
  std::array<bool, featureCount> available_{};
  AnalysisContext context_;
  quint64 contextGeneration_ = 0;
};
} // namespace si::gui
