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

    // A regular file where the cache root is expected must fail closed.
    const auto blockedRoot = QDir(sandbox.path()).filePath("occupied-cache-root");
    writeFile(blockedRoot, "user file must survive");
    bool rootFailed = false;
    try {
      CacheSession blocked(blockedRoot);
    } catch (const std::exception &) {
      rootFailed = true;
    }
    expect(rootFailed, "File-occupied cache root was accepted");
    QFile preserved(blockedRoot);
    expect(preserved.open(QIODevice::ReadOnly) &&
               preserved.readAll() == "user file must survive",
           "Cache startup changed a user-owned file");

    // Simulate failures at each private-construction phase. A constructor
    // failure must unwind the owned directory and any acquired owner lock
    // while the registry is still locked, before another instance can reap.
    auto sessionCount = [&] {
      return QDir(root).entryInfoList(
          QStringList{"session-??????"},
          QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks).size();
    };
    for (auto fault : {CacheSession::Fault::AfterPrivateDirectory,
                       CacheSession::Fault::AfterOwnershipMarker,
                       CacheSession::Fault::BeforeOwnerLock,
                       CacheSession::Fault::AfterOwnerLock}) {
      const auto before = sessionCount();
      bool failed = false;
      try {
        CacheSession interrupted(root, fault);
      } catch (const std::exception &) {
        failed = true;
      }
      expect(failed, "Injected constructor failure was ignored");
      expect(sessionCount() == before,
             "Constructor failure left a partial cache session");
      CacheSession recovered(root);
      expect(recovered.cleanup(), "Restart after constructor failure");
    }

    // Real lock contention must time out rather than silently sharing a root.
    {
      QLockFile held(QDir(root).filePath(".session-registry.lock"));
      held.setStaleLockTime(0);
      expect(held.tryLock(100), "Cannot prepare registry contention fixture");
      bool lockedOut = false;
      try {
        CacheSession blocked(root);
      } catch (const std::exception &) {
        lockedOut = true;
      }
      expect(lockedOut, "Concurrent registry lock was ignored");
      held.unlock();
      CacheSession recovered(root);
      expect(recovered.cleanup(), "Registry recovered after lock contention");
    }

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
        expect(QFileInfo::exists(QString::fromStdString(si::utf8(cache->path()))),
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
    writeFile(QDir(orphan).filePath(".sparamview-owned"),
              "SParamView-owned-cache-session-v1\n");
    writeFile(QDir(orphan).filePath("orphan.sicache"), "old cache");
    const auto impostor = QDir(root).filePath("session-FaKe88");
    expect(QDir().mkpath(impostor), "Impostor session fixture");
    writeFile(QDir(impostor).filePath("saved.siproject"),
              "Must not remove unmarked folder");
    const auto unrelated = QDir(root).filePath("custom-folder");
    expect(QDir().mkpath(unrelated), "Unrelated folder fixture");
    writeFile(QDir(unrelated).filePath("keep.txt"), "preserve");
    {
      CacheSession active(root);
      const auto livePath = active.path();
      CacheSession next(root); // Sweeps only stale session directories.
      expect(!QDir(orphan).exists(), "Abandoned cache is collected");
      expect(QFileInfo::exists(QDir(impostor).filePath("saved.siproject")),
             "Unmarked session-name collision must be preserved");
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
