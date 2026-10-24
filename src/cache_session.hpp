#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <memory>
#include <stdexcept>

// Cache data belongs to one GUI instance, not to the user or to all processes.
// The session directory is removed only after every Cache handle is released.
class CacheSession final {
public:
  explicit CacheSession(QString root = {}) {
    if (root.isEmpty()) {
      const auto base =
          QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
      if (base.isEmpty())
        throw std::runtime_error("Cannot resolve application cache directory");
      root = QDir(base).filePath("v3");
    }
    root_ = QDir::cleanPath(root);
    if (!QDir().mkpath(root_))
      throw std::runtime_error("Cannot create application cache directory");

    // Coordinate construction/reaping across instances. No process ever
    // removes an active instance's directory.
    QLockFile registry(QDir(root_).filePath(".session-registry.lock"));
    registry.setStaleLockTime(0);
    if (!registry.tryLock(5000))
      throw std::runtime_error("Cannot lock application cache registry");
    reapAbandonedSessions(root_);

    directory_ = std::make_unique<QTemporaryDir>(
        QDir(root_).filePath("session-XXXXXX"));
    if (!directory_->isValid())
      throw std::runtime_error("Cannot create private cache session");
    // A path name is not proof of ownership. Reaping requires this exact
    // marker, which is created only inside our freshly allocated directory.
    QFile marker(QDir(directory_->path()).filePath(".sparamview-owned"));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
        marker.write(kOwnedMarker) != qsizetype(qstrlen(kOwnedMarker)))
      throw std::runtime_error("Cannot mark private cache ownership");
    marker.close();
    lease_ = std::make_unique<QLockFile>(
        QDir(directory_->path()).filePath(".owner.lock"));
    // Disabling time-based expiry prevents a long-running analysis from
    // being mistaken for an abandoned session.
    lease_->setStaleLockTime(0);
    if (!lease_->tryLock(5000))
      throw std::runtime_error("Cannot lock private cache session");
  }

  CacheSession(const CacheSession &) = delete;
  CacheSession &operator=(const CacheSession &) = delete;

  QString path() const { return directory_ ? directory_->path() : QString{}; }

  bool cleanup() noexcept {
    if (!directory_)
      return true;
    if (lease_) {
      lease_->unlock();
      lease_.reset();
    }
    if (!directory_->remove()) {
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
