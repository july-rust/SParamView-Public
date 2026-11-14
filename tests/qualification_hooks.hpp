#pragma once
// Included ONLY by si_failure_probe, never by the shipped application target.
#include <QAbstractButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QTimer>
#include <QMessageBox>
#include <QDir>
#include <new>
#include <stdexcept>
namespace qualification {
inline QString mode() { return qEnvironmentVariable("SPARAMVIEW_QA_SCENARIO"); }
inline QString root() { return qEnvironmentVariable("SPARAMVIEW_QA_ROOT"); }
inline void beforeApp() {
#ifdef Q_OS_WIN
  // Isolated crash probes must not wait for Windows Error Reporting UI.
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
  if (mode() == "pre-app") throw std::runtime_error("QA pre-QApplication exception");
}
inline void afterApp(QApplication &app) {
  // Isolate cache/settings from every normal SParamView installation.
  app.setApplicationName("SParamViewQualification-" + QFileInfo(root()).fileName());
  app.setOrganizationName("SParamViewQualification");
  QFile metadata(QDir(root()).filePath("runtime.json"));
  if (metadata.open(QIODevice::WriteOnly))
    metadata.write(QJsonDocument(QJsonObject{{"qt", qVersion()},
      {"platform", QGuiApplication::platformName()},
      {"cache", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)}}).toJson());
  if (qEnvironmentVariableIsSet("SPARAMVIEW_QA_CLICK")) {
    auto *watch = new QTimer(&app);
    QObject::connect(watch, &QTimer::timeout, &app, [watch] {
      for (auto *widget : QApplication::topLevelWidgets()) {
        auto *box = qobject_cast<QMessageBox *>(widget);
        if (!box || !box->isVisible()) continue;
        auto *ok = box->button(QMessageBox::Ok);
        if (!ok || !ok->isVisible()) continue;
        const bool title = box->windowTitle() == "SParamView - Startup failed";
        const bool text = box->text().contains("Cannot start SParamView.") &&
                          box->text().contains("restart the application.");
        const bool screenshot = box->grab().save(QDir(root()).filePath("dialog.png"));
        QFile report(QDir(root()).filePath("dialog.json"));
        if (report.open(QIODevice::WriteOnly))
          report.write(QJsonDocument(QJsonObject{{"title", box->windowTitle()},
            {"text", box->text()}, {"okVisible", ok->isVisible()},
            {"valid", title && text && screenshot}, {"screenshot", screenshot},
            {"interaction", "Qt mouse press/release events on visible OK button"}}).toJson());
        watch->stop();
        const QPointF p = ok->rect().center();
        QMouseEvent press(QEvent::MouseButtonPress, p, ok->mapToGlobal(p.toPoint()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, p, ok->mapToGlobal(p.toPoint()),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(ok, &press);
        QApplication::sendEvent(ok, &release);
        return;
      }
    });
    watch->start(100);
  }
  if (mode() == "initialization") throw std::runtime_error("QA initialization failure");
  if (mode() == "bad-alloc") throw std::bad_alloc();
  if (mode() == "cache-block") {
    const auto base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(base);
    QFile blocker(QDir(base).filePath("v3"));
    if (!blocker.open(QIODevice::WriteOnly) || blocker.write("QA SENTINEL") != 11)
      throw std::runtime_error("QA could not prepare cache blocker");
  }
}
inline void afterWindow(QApplication &app) {
  const auto m = mode();
  if (m == "normal") QTimer::singleShot(200, &app, &QApplication::quit);
  if (m == "fatal") QTimer::singleShot(100, [] { qFatal("QA deliberate Qt fatal"); });
  if (m == "timer-unhandled") QTimer::singleShot(100, [] {
    throw std::runtime_error("QA deliberate unguarded timer exception");
  });
  if (m == "hold") {
    QFile ready(QDir(root()).filePath("ready"));
    if (ready.open(QIODevice::WriteOnly)) ready.write("ready");
  }
}
inline void fileOpen() {
  if (mode() == "file-open") throw std::runtime_error("QA startup file-open exception");
}
}
