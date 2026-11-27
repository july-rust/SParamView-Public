#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QMargins>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>

namespace sparamview::preferences {
inline QSettings store() {
  return QSettings(QSettings::IniFormat, QSettings::UserScope,
                   "SParamView", QCoreApplication::applicationName());
}
inline bool automated() {
  return QCoreApplication::instance() &&
         QCoreApplication::instance()->property("sparamview.automated").toBool();
}
inline bool usableDirectory(const QString &path, bool forSave = false) {
  if (path.isEmpty() || !QDir::isAbsolutePath(path)) return false;
  const QFileInfo info(path);
  return info.isDir() && info.isReadable() && (!forSave || info.isWritable());
}
inline QString workingDirectory(bool forSave = false) {
  auto prefs = store();
  const auto saved = prefs.value("workspace/v1/lastDirectory");
  if (!automated() && saved.metaType().id() == QMetaType::QString &&
      usableDirectory(saved.toString(), forSave))
    return saved.toString();
  for (const auto &candidate : {QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                QDir::homePath(), QDir::currentPath(), QDir::tempPath()})
    if (usableDirectory(candidate, forSave)) return candidate;
  return {};
}
inline QString saveSuggestion(const QString &filename) {
  const auto dir = workingDirectory(true);
  // Preserve the existing suggested basename, not a path embedded in a project name.
  const auto leaf = QFileInfo(filename).fileName();
  return dir.isEmpty() ? leaf : QDir(dir).filePath(leaf);
}
// Call only after a user file operation has actually committed successfully,
// never on selection, cancellation or worker failure.
inline void rememberFile(const QString &path) {
  if (automated() || path.isEmpty()) return;
  const auto dir = QFileInfo(path).absolutePath();
  if (!usableDirectory(dir)) return;
  auto prefs = store();
  prefs.setValue("workspace/v1/lastDirectory", dir);
  prefs.sync();
  if (prefs.status() != QSettings::NoError)
    qWarning() << "SParamView: cannot persist last working directory";
}
struct WindowState {
  QSize normalSize;
  bool maximized = true;
};
inline WindowState readWindowState() {
  auto prefs = store();
  const auto size = prefs.value("workspace/v1/windowNormalSize");
  const auto max = prefs.value("workspace/v1/windowMaximized");
  // INI-backed QVariant bools may reload as the strings true/false.
  const bool validBool = max.metaType().id() == QMetaType::Bool ||
      (max.metaType().id() == QMetaType::QString &&
       (max.toString() == "true" || max.toString() == "false"));
  if (size.metaType().id() != QMetaType::QSize ||
      !size.toSize().isValid() || size.toSize().isEmpty() ||
      size.toSize().width() > 32768 || size.toSize().height() > 32768 ||
      !validBool)
    return {};
  return {size.toSize(), max.toBool()};
}
inline void writeWindowState(const WindowState &state) {
  if (automated() || !state.normalSize.isValid() || state.normalSize.isEmpty()) return;
  auto prefs = store();
  prefs.setValue("workspace/v1/windowNormalSize", state.normalSize);
  prefs.setValue("workspace/v1/windowMaximized", state.maximized);
  prefs.sync();
  if (prefs.status() != QSettings::NoError)
    qWarning() << "SParamView: cannot persist main window state";
}
// Use logical pixels in the target screen's available (not physical) geometry.
// Leave room for native title bars and borders before the actual frame is known.
inline QRect centeredNormalGeometry(QSize desired, const QRect &area) {
  if (!area.isValid()) return {};
  if (!desired.isValid() || desired.isEmpty()) desired = QSize(1440, 980);
  const QSize cap(qMax(1, area.width() - 32), qMax(1, area.height() - 64));
  const QSize size = desired.boundedTo(cap);
  // QRect::center() truncates signed coordinates towards zero, so moving a
  // positive-origin rect to a negative-origin screen can offset it by a pixel.
  // Center using nonnegative extent differences instead.
  return QRect(QPoint(area.x() + (area.width() - size.width()) / 2,
                      area.y() + (area.height() - size.height()) / 2), size);
}
// Fit the client area with room for native non-client margins and an
// eight-logical-pixel safety inset on every edge.
inline QSize boundedDialogClientSize(QSize desired, const QRect &area, const QMargins &frame) {
  const QSize cap(qMax(1, area.width() - qMax(0,frame.left()) - qMax(0,frame.right()) - 16),
                  qMax(1, area.height() - qMax(0,frame.top()) - qMax(0,frame.bottom()) - 16));
  return desired.expandedTo(QSize(1,1)).boundedTo(cap);
}
inline QPoint centeredDialogFrame(QSize frame, const QRect &area) {
  return QPoint(area.x() + qMax(0, (area.width()-frame.width())/2),
                area.y() + qMax(0, (area.height()-frame.height())/2));
}
} // namespace sparamview::preferences
