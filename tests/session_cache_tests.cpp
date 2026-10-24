#include "cache_session.hpp"
#include "si/core.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QTemporaryDir>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void expect(bool okay, const char *message) {
  if (!okay)
    throw std::runtime_error(message);
}
static void writeFile(const QString &path, const QByteArray &data) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
    throw std::runtime_error("Test fixture write failed");
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    QTemporaryDir sandbox;
    expect(sandbox.isValid(), "Sandbox creation");
    const auto root = QDir(sandbox.path()).filePath("SParamView-cache/v3");
    const auto source = QDir(sandbox.path()).filePath("source.s2p");
    const auto project = QDir(sandbox.path()).filePath("saved.siproject");
    const auto report = QDir(sandbox.path()).filePath("saved.xlsx");
    const auto settings = QDir(sandbox.path()).filePath("settings.ini");
    writeFile(source, "# Hz S RI R 50\n1 .1 0 .8 0 .6 0 .2 0\n2 .1 0 .8 0 .6 0 .2 0\n");
    writeFile(project, "do not delete project");
    writeFile(report, "do not delete export");
    writeFile(settings, "do not delete preferences");

    QString firstPath, secondPath;
    {
      CacheSession first(root);
      CacheSession second(root);
      firstPath = first.path();
      secondPath = second.path();
      expect(firstPath != secondPath && QDir(firstPath).exists() &&
                 QDir(secondPath).exists(),
             "Independent instance cache directories");

      // Open a real Touchstone cache and release its file handles.
      {
        auto cache = si::Cache::open(
            si::pathFromUtf8(source.toUtf8().toStdString()),
            si::pathFromUtf8(firstPath.toUtf8().toStdString()));
        expect(QFileInfo::exists(QString::fromUtf8(si::utf8(cache->path()))),
               "Real imported .sicache exists in the owned directory");
      }
      expect(first.cleanup(), "First instance cleanup after handles closed");
      expect(!QDir(firstPath).exists() && QDir(secondPath).exists(),
             "Closing one instance preserves another instance's cache");
    }
    expect(!QDir(firstPath).exists() && !QDir(secondPath).exists(),
           "All normal-exit session files removed");
    expect(QFileInfo::exists(source) && QFileInfo::exists(project) &&
                 QFileInfo::exists(report) && QFileInfo::exists(settings),
           "Original, project, report and preferences preserved");

    const auto orphan = QDir(root).filePath("session-AbC123");
    expect(QDir().mkpath(orphan), "Abandoned session fixture");
    writeFile(QDir(orphan).filePath("orphan.sicache"), "old cache");
    const auto unrelated = QDir(root).filePath("custom-folder");
    expect(QDir().mkpath(unrelated), "Unrelated folder fixture");
    writeFile(QDir(unrelated).filePath("keep.txt"), "preserve");
    {
      CacheSession active(root);
      const auto livePath = active.path();
      CacheSession next(root); // Sweeps only stale session directories.
      expect(!QDir(orphan).exists(), "Abandoned cache is collected");
      expect(QDir(livePath).exists(), "Live session is preserved");
      expect(QFileInfo::exists(QDir(unrelated).filePath("keep.txt")),
             "Non-session folder is preserved");
    }

    // A symlink within the private directory must never delete its target.
    {
      CacheSession owned(root);
      const auto link = QDir(owned.path()).filePath("external-link");
      if (QFile::link(project, link))
        expect(QFileInfo::exists(project), "Symlink source exists");
      expect(owned.cleanup(), "Explicit close succeeds");
      expect(owned.cleanup(), "Cleanup is idempotent");
    }
    expect(QFileInfo::exists(project), "Symlink target preserved");

    std::cout << "Session-cache cleanup tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Session-cache cleanup: " << e.what() << '\n';
    return 1;
  }
}
