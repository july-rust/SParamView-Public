#pragma once
// 012_04: GUI-only scaffolding; numerical engines are introduced in later steps.
#include "si/channel_analysis/common.hpp"
#include "si/channel_analysis/ild.hpp"
#include "si/channel_analysis/icn.hpp"
#include "si/channel_analysis/eye.hpp"
#include <QtConcurrent>
#include <QTemporaryDir>
#include <QScreen>
#include <QShowEvent>
#include <vector>
#include <QtWidgets>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace si::gui {
enum class Feature : int { Ild = 0, Icn = 1, Eye = 2, Com = 3 };
constexpr int featureCount = 4;

struct SourceItem {
  QString revision, sha256, mappingDigest;
  QStringList channels, channelIds;
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
  std::shared_ptr<const si::channel::IldData> ild;
  std::shared_ptr<const si::channel::IcnData> icn;
  std::shared_ptr<const si::channel::EyeData> eye;
  QString eyeSourceFilename;
  si::channel::Provenance provenance;
  bool eyeDcConfirmation = false, eyeAutoPreview = false;
};
struct IldSourceInput {
  std::shared_ptr<const si::Cache> cache;
  std::vector<si::Channel> mapping;
  std::string revision, expectedSha;
  uint64_t workspaceGeneration = 0, mappingGeneration = 0;
};
using IldSourceProvider = std::function<std::optional<IldSourceInput>(int)>;
using IcnSourceProvider = IldSourceProvider;
using EyeSourceProvider = IldSourceProvider;
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
  void setIldSourceProvider(IldSourceProvider provider);
  void setIcnSourceProvider(IcnSourceProvider provider);
  void setEyeSourceProvider(EyeSourceProvider provider);
  bool saveEyeCsv(const QString &filename);
  bool saveEyePng(const QString &filename);
  bool saveIcnCsv(const QString &filename);
  bool saveIcnPng(const QString &filename);
  bool saveIldCsv(const QString &filename);
  bool saveIldPng(const QString &filename);
  void cancelWork();
  si::channel::Status status() const { return status_; }
  QPushButton *runButton() const { return run_; }
  QPushButton *cancelButton() const { return cancel_; }

protected:
  void closeEvent(QCloseEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void fitToScreen();
  void watchScreen(QScreen *screen);
  void clampToScreen(QScreen *screen);
  QMetaObject::Connection screenGeometryConnection_;
  void invalidateRequest();
  void setStatus(si::channel::Status status, const QString &details);
  QString selectionIdentity() const;
  void launchIld();
  void showIld(const WorkOutcome &outcome);
  void exportIldCsv();
  void exportIldPng();
  void clearIld();
  void launchIcn();
  void showIcn(const WorkOutcome &outcome);
  void exportIcnCsv();
  void exportIcnPng();
  void clearIcn();
  void populateIcnAggressors();
  bool readManualEyeSettings(si::channel::EyeSettings &settings);
  void launchEye(bool autoPreview = false, bool acceptLimitedDc = false);
  void showEye(const WorkOutcome &outcome);
  void clearEye();
  void exportEyeCsv();
  void exportEyePng();
  void selectEyeProfile();
  Feature feature_;
  AsyncScheduler *scheduler_ = nullptr;
  AnalysisContext context_;
  quint64 requestGeneration_ = 0;
  QPointer<QDialog> eyeDcPrompt_;
  void dismissEyeDcPrompt();
  si::channel::Status status_ = si::channel::Status::NotEvaluated;
  QComboBox *source_ = nullptr, *victim_ = nullptr, *aggressor_ = nullptr;
  QComboBox *direction_ = nullptr, *fitModel_ = nullptr;
  QListWidget *icnAggressors_ = nullptr;
  QCheckBox *icnIndependent_ = nullptr;
  QDoubleSpinBox *icnBaud_ = nullptr, *icnVpp_ = nullptr,
                 *icnRise_ = nullptr, *icnRx_ = nullptr;
  QDoubleSpinBox *fromGhz_ = nullptr, *toGhz_ = nullptr;
  QDoubleSpinBox *eyeBitRate_ = nullptr, *eyeReference_ = nullptr;
  QComboBox *eyeProfile_ = nullptr, *eyeMode_ = nullptr, *eyePattern_ = nullptr;
  QComboBox *eyeFft_ = nullptr, *eyeSamples_ = nullptr, *eyeView_ = nullptr;
  QLineEdit *eyeBits_ = nullptr;
  std::array<QLineEdit *,7> eyeElectrical_{};
  bool eyeFrequencySync_ = false;
  QLabel *eyeMetrics_ = nullptr;
  QWidget *eyeGraph_ = nullptr;
  EyeSourceProvider eyeProvider_;
  std::shared_ptr<const si::channel::EyeData> eyeResult_;
  si::channel::Provenance eyeProvenance_;
  QString eyeSourceFilename_;
  QLabel *statusLabel_ = nullptr, *graphPlaceholder_ = nullptr;
  QLabel *icnContributions_ = nullptr;
  QWidget *ildGraph_ = nullptr, *icnGraph_ = nullptr;
  IldSourceProvider ildProvider_;
  IcnSourceProvider icnProvider_;
  std::shared_ptr<const si::channel::IcnData> icnResult_;
  si::channel::Provenance icnProvenance_;
  std::shared_ptr<const si::channel::IldData> ildResult_;
  si::channel::Provenance ildProvenance_;
  QProgressBar *progress_ = nullptr;
  QPushButton *run_ = nullptr, *cancel_ = nullptr, *csv_ = nullptr, *png_ = nullptr, *eyePreview_ = nullptr;
  QTimer progressPoll_;
};

class AnalysisWindowManager final : public QObject {
public:
  explicit AnalysisWindowManager(QMainWindow *owner);
  ~AnalysisWindowManager() override;
  void installMenu();
  void setContext(AnalysisContext context);
  // Keep an old project cache directory alive while any cancelled worker
  // may still own a SourceSnapshot backed by its files.
  void retireWorkspace(std::shared_ptr<QTemporaryDir> workspace);
  // Register only release-supported engines; COM is deferred in 012_43.
  void setEngineAvailable(Feature feature, bool available);
  void setIldSourceProvider(IldSourceProvider provider);
  void setIcnSourceProvider(IcnSourceProvider provider);
  void setEyeSourceProvider(EyeSourceProvider provider);
  // Returns nullptr for deferred or unsupported features.
  AnalysisWindow *openWindow(Feature feature);
  QAction *action(Feature feature) const { return actions_.at(int(feature)); }
  AnalysisWindow *existingWindow(Feature feature) const {
    return windows_.at(int(feature));
  }
  AsyncScheduler &scheduler() { return scheduler_; }
  const AnalysisContext &context() const { return context_; }

private:
  QMainWindow *owner_ = nullptr;
  // Destruction order: scheduler must drain BEFORE retired workspaces vanish.
  std::vector<std::shared_ptr<QTemporaryDir>> retiredWorkspaces_;
  QTimer retirementPoll_;
  AsyncScheduler scheduler_;
  std::array<QPointer<AnalysisWindow>, featureCount> windows_{};
  std::array<QAction *, featureCount> actions_{};
  std::array<bool, featureCount> available_{};
  AnalysisContext context_;
  IldSourceProvider ildProvider_;
  IcnSourceProvider icnProvider_;
  EyeSourceProvider eyeProvider_;
  quint64 contextGeneration_ = 0;
};
} // namespace si::gui
