#pragma once

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QString>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace sparamview {

// Test and project-validation runs must never wait on a modal startup dialog.
inline bool isAutomatedLaunch(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (argv[i] &&
        (!std::strcmp(argv[i], "--selftest") ||
         !std::strcmp(argv[i], "--validate-project")))
      return true;
  return false;
}

inline bool canShowStartupDialog(bool automated) {
  if (automated ||
      !qobject_cast<QApplication *>(QCoreApplication::instance()))
    return false;
  const auto platform = QGuiApplication::platformName().toLower();
  // CI/offscreen platforms have no user to acknowledge a modal dialog.
  return platform != "offscreen" && platform != "minimal";
}

inline int reportStartupFailure(const QString &detail, bool automated) {
  const auto cause = detail.isEmpty()
                         ? QStringLiteral("Unknown startup failure")
                         : detail;
  std::cerr << "SParamView startup failed: "
            << cause.toUtf8().toStdString() << '\n';
  if (canShowStartupDialog(automated)) {
    QMessageBox::critical(
        nullptr, QStringLiteral("SParamView - Startup failed"),
        QStringLiteral("Cannot start SParamView.\n\n") + cause +
            QStringLiteral(
                "\n\nCheck the cache location, folder permissions and "
                "available disk space, then restart the application."));
  }
  return EXIT_FAILURE;
}

} // namespace sparamview
