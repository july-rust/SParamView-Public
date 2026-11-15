#include "app.hpp"
#include <cmath>
#include <map>
#include <set>
#include <regex>
#include "si/mapping_policy.hpp"
#include "si/direction_policy.hpp"
#include <QCryptographicHash>
#include "window_ui.cpp"
#include "window_workspace.cpp"
#include "window_mapping.cpp"
#include "window_analysis.cpp"
#include "window_results.cpp"
#include "window_plot_controller.cpp"
#include "window_project.cpp"
#include "window_export.cpp"
#include "window_validation.cpp"

// v1.2.2 keeps the proven analysis-settings geometry intact and applies only
// a compact visual grouping pass. This avoids reducing the plot area while
// making it clear that Option belongs to the TDR controls.
void applyCompactAnalysisSettings(MainWindow &window) {
  auto *panel = window.findChild<QScrollArea *>("analysisSettingsPanel");
  if (!panel || !panel->widget())
    return;
  auto *config = qobject_cast<QVBoxLayout *>(panel->widget()->layout());
  if (!config || config->count() < 1)
    return;
  auto *controls = qobject_cast<QHBoxLayout *>(config->itemAt(0)->layout());
  if (!controls)
    return;

  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(6);
  const QString segmentStyle =
      "QWidget#analysisMetricSegment {"
      "background:#f8fafc;"
      "border:1px solid #b8c7d7;"
      "border-radius:4px;"
      "}";

  for (int i = 0; i < controls->count(); ++i) {
    auto *item = controls->itemAt(i);
    auto *segment = item ? item->widget() : nullptr;
    if (!segment)
      continue;
    segment->setObjectName("analysisMetricSegment");
    segment->setStyleSheet(segmentStyle);
    if (auto *row = qobject_cast<QHBoxLayout *>(segment->layout())) {
      row->setContentsMargins(6, 0, 6, 0);
      row->setSpacing(4);
    }
  }

  if (controls->count() < 1)
    return;
  auto *last = controls->itemAt(controls->count() - 1);
  auto *tdrSegment = last ? last->widget() : nullptr;
  if (!tdrSegment)
    return;
  for (auto *tool : tdrSegment->findChildren<QToolButton *>()) {
    if (!tool->toolTip().startsWith("TDR options:"))
      continue;
    tool->setObjectName("tdrOptionButton");
    tool->setText("Option");
    tool->setMaximumWidth(68);
    break;
  }
}

void MainWindow::refreshAnalysisContext() {
  if (!analysisWindows) return;
  si::gui::AnalysisContext context;
  context.activeSource = revisionBox ? revisionBox->currentIndex() : -1;
  for (const auto &revision : revisions) {
    si::gui::SourceItem source;
    source.revision = revision.name;
    source.mappingConfirmed = revision.confirmed;
    if (revision.cache) source.sha256 = qs(revision.cache->meta().sha256);
    // Persist no Cache handle in a GUI window. Numerical adapters must capture
    // a verified immutable SourceSnapshot on their worker thread instead.
    QByteArray encoded;
    if (revision.confirmed) {
      for (const auto &channel : revision.channels) {
        source.channels.append(qs(channel.name));
        source.channelIds.append(qs(channel.id));
        encoded.append(QJsonDocument(channelJson(channel)).toJson(QJsonDocument::Compact));
        encoded.append('\n');
      }
      if (!encoded.isEmpty())
        source.mappingDigest = QString::fromLatin1(
            QCryptographicHash::hash(encoded, QCryptographicHash::Sha256).toHex());
    }
    context.sources.append(std::move(source));
  }
  analysisWindows->setContext(std::move(context));
}
