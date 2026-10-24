#include "../src/session_cache.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <iostream>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("SParamView");
  app.setOrganizationName("SParamView");
  const auto args = app.arguments();
  if (args.size() == 3 && args[1] == "--child") {
    try {
      const QString cache = sessionCachePath();
      QFile data(cache + "/sample.sicache");
      if (!data.open(QIODevice::WriteOnly) || data.write("sample") != 6)
        return 2;
      data.close();
      QFile marker(args[2]);
      if (!marker.open(QIODevice::WriteOnly))
        return 3;
      marker.write(cache.toUtf8());
      marker.close();
      return 0;
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
      return 4;
    }
  }

  QTemporaryDir fixture;
  if (!fixture.isValid())
    return 5;
  const QString markerPath = fixture.path() + "/cache_path.txt";
  const QString originalPath = fixture.path() + "/original.s4p";
  const QString projectPath = fixture.path() + "/saved.siproject";
  const QString reportPath = fixture.path() + "/report.xlsx";
  const QString settingsPath = fixture.path() + "/settings.ini";
  for (const auto &path : {originalPath, projectPath, reportPath, settingsPath}) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("preserve") != 8)
      return 6;
  }

  QProcess child;
  child.start(QCoreApplication::applicationFilePath(), {"--child", markerPath});
  if (!child.waitForStarted(10000) || !child.waitForFinished(30000) ||
      child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0) {
    std::cerr << "Cache child failed: " << child.readAllStandardError().toStdString() << '\n';
    return 7;
  }

  QFile marker(markerPath);
  if (!marker.open(QIODevice::ReadOnly))
    return 8;
  const QString cache = QString::fromUtf8(marker.readAll());
  if (cache.isEmpty() || QFileInfo::exists(cache) ||
      !cache.contains("/v3/session-")) {
    std::cerr << "Session cache remains after normal process exit\n";
    return 9;
  }
  for (const auto &path : {originalPath, projectPath, reportPath, settingsPath})
    if (!QFileInfo::exists(path))
      return 10;
  return 0;
}
