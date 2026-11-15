// 012_04: event-loop and worker lifecycle tests (no numerical engine needed).
#include "gui/analysis/analysis_windows.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <stdexcept>

using namespace si::gui;
using S = si::channel::Status;
namespace {
void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
bool pumpUntil(const std::function<bool()> &predicate, int timeoutMs = 6000) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!predicate() && elapsed.elapsed() < timeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
    QThread::msleep(2);
  }
  QCoreApplication::processEvents();
  return predicate();
}
AnalysisContext context(const QString &sha = QStringLiteral("fixture-sha-256"),
                        bool confirmed = true) {
  AnalysisContext result;
  result.activeSource = 0;
  SourceItem source;
  source.revision = QStringLiteral("Fixture Revision A");
  source.sha256 = sha;
  source.mappingDigest = QStringLiteral("physical-mapping-fixture");
  source.channels = {QStringLiteral("Victim A"), QStringLiteral("Aggressor B")};
  source.mappingConfirmed = confirmed;
  result.sources.append(source);
  return result;
}
AsyncWork slowWork(int loops = 90) {
  return [loops](const std::atomic_bool &cancel, std::atomic_int &progress) -> WorkOutcome {
    for (int i = 0; i < loops; ++i) {
      if (cancel.load()) return {S::Cancelled, QStringLiteral("Cancellation observed")};
      progress.store(i * 100 / loops);
      QThread::msleep(3);
    }
    return {S::Valid, QStringLiteral("Fixture-only completion; not a real analysis")};
  };
}
void drain(AsyncScheduler &scheduler) {
  check(pumpUntil([&] { return scheduler.inFlight() == 0; }),
        "Worker did not drain before timeout");
}
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    QMainWindow main;
    AnalysisWindowManager manager(&main);
    manager.installMenu();
    check(main.menuBar()->findChild<QMenu *>(QStringLiteral("channelAnalysisMenu")),
          "Analysis menu missing");
    for (int i = 0; i < featureCount; ++i) {
      auto *action = manager.action(static_cast<Feature>(i));
      check(action && !action->isEnabled(), "Unimplemented engine is exposed as ready");
    }

    manager.setContext(context());
    AnalysisWindow *windows[featureCount]{};
    for (int i = 0; i < featureCount; ++i) {
      auto feature = static_cast<Feature>(i);
      windows[i] = manager.openWindow(feature);
      check(windows[i] && windows[i]->isWindow() && windows[i]->isVisible(),
            "Analysis must open in a non-modal top-level window");
      check(windows[i]->runButton() && !windows[i]->runButton()->isEnabled(),
            "An unfinished numerical engine must not run");
      check(manager.openWindow(feature) == windows[i], "Duplicate analysis window created");
      for (int j = 0; j < i; ++j)
        check(windows[i] != windows[j], "Distinct analysis features share a window");
    }
    // The real menu actions stay disabled until a future engine registers.
    // Only the isolated test manager enables the action to check focus/reuse.
    manager.setEngineAvailable(Feature::Ild, true);
    manager.action(Feature::Ild)->trigger();
    check(manager.existingWindow(Feature::Ild) == windows[0],
          "Repeated Analysis action did not reuse the existing window");
    manager.setEngineAvailable(Feature::Ild, false);

    // The main event loop must remain responsive during a background job.
    bool timerFired = false;
    QTimer::singleShot(20, &main, [&] { timerFired = true; });
    check(windows[0]->startWork(slowWork()), "Test job failed to start");
    check(pumpUntil([&] { return timerFired; }, 1000),
          "A worker blocked the Qt event loop");
    windows[0]->cancelButton()->click();
    drain(manager.scheduler());
    check(windows[0]->status() == S::Cancelled,
          "Cancelled job must not publish a VALID result");

    // A completed result is invalidated on context and mapping transitions.
    check(windows[1]->startWork(slowWork(5)), "Short job failed to start");
    drain(manager.scheduler());
    check(windows[1]->status() == S::Valid, "Fixture job completion missing");
    manager.setContext(context(QStringLiteral("different-source-sha")));
    check(windows[1]->status() == S::Stale,
          "Context change must invalidate previously completed results");

    // A result from the older source is never published as the current result.
    check(windows[2]->startWork(slowWork()), "Stale test job did not start");
    manager.setContext(context(QStringLiteral("newer-source-sha")));
    drain(manager.scheduler());
    check(windows[2]->status() == S::Stale, "Older source result was not discarded");

    // Exceptions from worker threads are carried to the GUI without terminate.
    check(windows[3]->startWork([](const std::atomic_bool &,
                                   std::atomic_int &) -> WorkOutcome {
      throw std::runtime_error("synthetic engine failure");
    }), "Exception test job did not start");
    drain(manager.scheduler());
    check(windows[3]->status() == S::Error, "Worker exception status not propagated");

    // Deleting a window while its worker is alive must not use a dangling QObject.
    check(windows[0]->startWork(slowWork()), "Close-while-busy job did not start");
    QPointer<AnalysisWindow> closed(windows[0]);
    windows[0]->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    check(!closed, "Closed window was not deleted");
    drain(manager.scheduler());
    auto *reopened = manager.openWindow(Feature::Ild);
    check(reopened && reopened->isVisible(), "Closed analysis window did not reopen");

    // No silently valid result from unconfirmed mappings.
    manager.setContext(context(QStringLiteral("unknown"), false));
    check(!reopened->startWork(slowWork(1)), "Unconfirmed mapping accepted");
    check(reopened->status() == S::InvalidSource, "Invalid mapping not diagnosed");

    manager.setContext(context());
    for (int i = 0; i < featureCount; ++i) {
      auto *w = manager.existingWindow(static_cast<Feature>(i));
      check(w && w->isVisible(), "One feature close affected an unrelated window");
    }
    std::cout << "012_04 QT GUI/worker lifecycle PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "012_04 QT GUI/worker lifecycle FAIL: " << error.what() << '\n';
    return 1;
  }
}
