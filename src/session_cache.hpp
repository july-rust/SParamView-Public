#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QString>
#include <stdexcept>

// Each GUI process owns a distinct temporary cache directory. QTemporaryDir
// removes it at normal process shutdown, after MainWindow has released every
// CacheFile and worker thread. Never delete the shared cache root: another
// process (or an older SParamView release) may still be using its files.
inline QString sessionCachePath() {
  static QTemporaryDir session([] {
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/v3";
    if (root.isEmpty() || !QDir().mkpath(root))
      throw std::runtime_error("Cannot create application cache directory");
    return root + "/session-XXXXXX";
  }());
  if (!session.isValid())
    throw std::runtime_error("Cannot create temporary session cache");
  return session.path();
}
