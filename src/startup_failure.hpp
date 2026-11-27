#pragma once

#include "ui_language.hpp"
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
    const auto language=ui_language::saved();
    ui_language::setFrameworkLanguage(language);
    QMessageBox::critical(nullptr,ui_language::menuText("SParamView - Startup failed",language),
      ui_language::menuText("Cannot start SParamView.\n\n",language)+ui_language::menuText(cause,language)+
      ui_language::menuText("\n\nCheck the cache location, folder permissions and available disk space, then restart the application.",language));
  }
  return EXIT_FAILURE;
}

} // namespace sparamview
