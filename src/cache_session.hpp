#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <memory>
#include <stdexcept>

// Cache data belongs to one GUI instance, not to the user or to all processes.
// The session directory is removed only after every Cache handle is released.
class CacheSession final {
public:
#ifdef SPARAMVIEW_CACHE_TEST_FAULTS
  // Fault injection exists ONLY in the separately compiled test executable.
  enum class Fault {
    None, AfterPrivateDirectory, AfterOwnershipMarker, BeforeOwnerLock,
    AfterOwnerLock
  };
  explicit CacheSession(QString root = {}, Fault fault = Fault::None) {
#else
  explicit CacheSession(QString root = {}) {
#endif
    if (root.isEmpty()) {
      const auto base =
          QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
      if (base.isEmpty())
        throw std::runtime_error("Cannot resolve application cache directory");
      root = QDir(base).filePath("v3");
    }
    root_ = QDir::cleanPath(root);
    // Multiple application processes can create the same cache root
    // simultaneously. QDir::mkpath() may report failure when another
    // process creates an intermediate directory at the same instant.
    // Only fail if the root is still not a usable directory.
    bool ready = false;
    for (int attempt = 0; attempt < 5 && !ready; ++attempt) {
      ready = QDir(root_).exists() || QDir().mkpath(root_) ||
              QFileInfo(root_).isDir();
      if (!ready && attempt < 4)
        QThread::msleep(10);
    }
    if (!ready || !QFileInfo(root_).isDir())
      throw std::runtime_error(
          ("Cannot create application cache directory: " +
           root_.toStdString()));

    // Coordinate construction/reaping across instances. No process ever
    // removes an active instance's directory.
    QLockFile registry(QDir(root_).filePath(".session-registry.lock"));
    registry.setStaleLockTime(0);
    if (!registry.tryLock(5000))
      throw std::runtime_error("Cannot lock application cache registry");
    reapAbandonedSessions(root_);

    // Keep the in-progress resources local until construction succeeds.
    // C++ destroys these locals (owner lock first, directory second) BEFORE
    // releasing registry, including when a constructor step throws.
    // This avoids publishing a half-initialized session or racing the reaper.
    auto pendingDirectory = std::make_unique<QTemporaryDir>(
        QDir(root_).filePath("session-XXXXXX"));
    if (!pendingDirectory->isValid())
      throw std::runtime_error("Cannot create private cache session");
#ifdef SPARAMVIEW_CACHE_TEST_FAULTS
    if (fault == Fault::AfterPrivateDirectory)
      throw std::runtime_error("Injected failure after private cache creation");
#endif
    QFile marker(QDir(pendingDirectory->path()).filePath(".sparamview-owned"));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
        marker.write(kOwnedMarker) != qsizetype(qstrlen(kOwnedMarker)))
      throw std::runtime_error("Cannot mark private cache ownership");
    marker.close(); // Close before a Windows QTemporaryDir cleanup.
#ifdef SPARAMVIEW_CACHE_TEST_FAULTS
    if (fault == Fault::AfterOwnershipMarker)
      throw std::runtime_error("Injected failure after ownership marker");
    if (fault == Fault::BeforeOwnerLock)
      throw std::runtime_error("Injected failure before owner lock");
#endif
    auto pendingLease = std::make_unique<QLockFile>(
        QDir(pendingDirectory->path()).filePath(".owner.lock"));
    pendingLease->setStaleLockTime(0);
    if (!pendingLease->tryLock(5000))
      throw std::runtime_error("Cannot lock private cache session");
#ifdef SPARAMVIEW_CACHE_TEST_FAULTS
    if (fault == Fault::AfterOwnerLock)
      throw std::runtime_error("Injected failure after owner lock");
#endif
    directory_ = std::move(pendingDirectory);
    lease_ = std::move(pendingLease);
  }

  CacheSession(const CacheSession &) = delete;
  CacheSession &operator=(const CacheSession &) = delete;

  QString path() const { return directory_ ? directory_->path() : QString{}; }

  bool cleanup() noexcept {
    if (!directory_)
      return true;

    // Serialize lease release + directory removal with the startup reaper.
    // Without this lock a second process can mistake a session being removed
    // for an abandoned one and race its owner to delete the same files.
    QLockFile registry(QDir(root_).filePath(".session-registry.lock"));
    registry.setStaleLockTime(0);
    if (!registry.tryLock(5000)) {
      // Keep the live lease until destruction; never attempt an unlocked
      // removal. A later launch can safely reclaim the abandoned directory.
      directory_->setAutoRemove(false);
      qWarning() << "SParamView: cache registry busy during shutdown:"
                 << directory_->path();
      return false;
    }
    if (lease_) {
      lease_->unlock();
      lease_.reset();
    }
    if (!directory_->remove()) {
      // A still-open Windows cache handle can prevent removal. Do not race
      // the destructor's implicit QTemporaryDir deletion against other
      // processes; the next launch will retry with ownership checks.
      directory_->setAutoRemove(false);
      qWarning() << "SParamView: cache-session removal failed:"
                 << directory_->path();
      return false;
    }
    directory_.reset();
    return true;
  }

  ~CacheSession() { cleanup(); }

private:
  static constexpr const char *kOwnedMarker = "SParamView-owned-cache-session-v1\n";

  static void reapAbandonedSessions(const QString &root) {
    QDir parent(root);
    const QRegularExpression ownedName("^session-[A-Za-z0-9]{6}$");
    const auto entries = parent.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &entry : entries) {
      if (!ownedName.match(entry.fileName()).hasMatch())
        continue;
      const auto folder = entry.absoluteFilePath();
      const auto markerPath = QDir(folder).filePath(".sparamview-owned");
      const QFileInfo markerInfo(markerPath);
      if (!markerInfo.isFile() || markerInfo.isSymLink())
        continue; // Never delete an unmarked or impersonated folder.
      QFile marker(markerPath);
      if (!marker.open(QIODevice::ReadOnly))
        continue;
      const bool owned = marker.readAll() == QByteArray(kOwnedMarker);
      marker.close(); // Windows cannot unlink a still-open QFile.
      if (!owned)
        continue;
      QLockFile owner(QDir(folder).filePath(".owner.lock"));
      owner.setStaleLockTime(0);
      if (!owner.tryLock(0))
        continue; // An active process owns this folder.
      owner.unlock();
      if (!QDir(folder).removeRecursively())
        qWarning() << "SParamView: abandoned cache cleanup failed:"
                   << folder;
    }
  }

  QString root_;
  std::unique_ptr<QTemporaryDir> directory_;
  std::unique_ptr<QLockFile> lease_;
};
