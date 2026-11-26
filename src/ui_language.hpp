#pragma once
#include "user_preferences.hpp"
#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QString>
#include <vector>

namespace sparamview::ui_language {
// UI-only language choice. Never set QLocale or translate serialized values.
enum class Language { English, Korean, Japanese };
inline Language fromCode(const QString &code) {
  if (code == QStringLiteral("ko")) return Language::Korean;
  if (code == QStringLiteral("ja")) return Language::Japanese;
  return Language::English;
}
inline QString code(Language value) {
  switch (value) {
  case Language::Korean: return QStringLiteral("ko");
  case Language::Japanese: return QStringLiteral("ja");
  default: return QStringLiteral("en");
  }
}
inline Language saved() {
  if (preferences::automated()) return Language::English;
  return fromCode(preferences::store().value("ui/v1/language").toString());
}
inline void remember(Language value) {
  if (preferences::automated()) return;
  auto prefs = preferences::store();
  prefs.setValue("ui/v1/language", code(value));
  prefs.sync();
  if (prefs.status() != QSettings::NoError)
    qWarning() << "SParamView: cannot persist UI language";
}
// Initial catalog: main menu and its explicit action icon labels/tooltips.
// Untranslated strings are not evidence of full application localization.
inline QString menuText(const QString &source, Language value) {
  if (value == Language::English) return source;
  struct Entry { const char *en; const char *ko; const char *ja; };
  static constexpr Entry entries[] = {
    {"File", "파일", "ファイル"},
    {"New", "새로 만들기", "新規作成"},
    {"Open Touchstone...", "Touchstone 열기...", "Touchstoneを開く..."},
    {"Open Project...", "프로젝트 열기...", "プロジェクトを開く..."},
    {"Save Project", "프로젝트 저장", "プロジェクトを保存"},
    {"Save Project As...", "다른 이름으로 프로젝트 저장...", "名前を付けてプロジェクトを保存..."},
    {"Export Excel Workbook...", "Excel 통합 문서 내보내기...", "Excelブックをエクスポート..."},
    {"Export Plot as PNG...", "그래프를 PNG로 내보내기...", "グラフをPNGでエクスポート..."},
    {"Export Selected Trace as CSV...", "선택한 트레이스를 CSV로 내보내기...", "選択したトレースをCSVでエクスポート..."},
    {"Channel Mapping", "채널 매핑", "チャネルマッピング"},
    {"Edit / Confirm Mapping...", "매핑 편집 / 확인...", "マッピングの編集 / 確認..."},
    {"Import Mapping CSV...", "매핑 CSV 가져오기...", "マッピングCSVをインポート..."},
    {"Export Mapping CSV...", "매핑 CSV 내보내기...", "マッピングCSVをエクスポート..."},
    {"View", "보기", "表示"},
    {"Split View", "분할 보기", "分割表示"},
    {"Maximize Plot", "그래프 최대화", "グラフを最大化"},
    {"Maximize Results", "결과 최대화", "結果を最大化"},
    {"Channel List", "채널 목록", "チャネル一覧"},
    {"Analysis Settings", "분석 설정", "解析設定"},
    {"Full Screen", "전체 화면", "全画面"},
    {"Reset Layout", "레이아웃 초기화", "レイアウトをリセット"},
    {"Analysis", "분석", "解析"},
    {"Eye Diagram", "아이 다이어그램", "アイダイアグラム"},
    {"Limit", "판정 기준", "判定基準"},
    {"Save Preset...", "프리셋 저장...", "プリセットを保存..."},
    {"Load Preset...", "프리셋 불러오기...", "プリセットを読み込む..."},
    {"Edit Frequency Limits...", "주파수별 판정 기준 편집...", "周波数別判定基準を編集..."},
    {"Tools", "도구", "ツール"},
    {"Open Sample Project", "예제 프로젝트 열기", "サンプルプロジェクトを開く"},
    {"Verify Source SHA-256", "원본 SHA-256 검증", "元ファイルのSHA-256を検証"},
    {"About", "프로그램 정보", "このアプリについて"},
    {"Split", "분할", "分割"},
    {"Plot", "그래프", "グラフ"},
    {"Results", "결과", "結果"},
    {"Channels", "채널", "チャネル"},
    {"Settings", "설정", "設定"},
    {"Full Screen (F11)", "전체 화면 (F11)", "全画面 (F11)"},
    {"Available after the numerical engine is verified", "수치 엔진 검증 후 사용 가능", "数値エンジンの検証後に使用可能"},
  };
  for (const auto &entry : entries)
    if (source == QString::fromUtf8(entry.en))
      return QString::fromUtf8(value == Language::Korean ? entry.ko : entry.ja);
  return source;
}
struct MenuBinding {
  QPointer<QAction> action;
  QString text;
  QString iconText;
  QString toolTip;
  bool distinctIconText;
  bool distinctToolTip;
};
inline void captureMenu(QAction *action, std::vector<MenuBinding> &bindings) {
  if (!action || action->isSeparator()) return;
  bindings.push_back({action, action->text(), action->iconText(), action->toolTip(),
                      action->iconText() != action->text(),
                      action->toolTip() != action->text()});
  if (auto *menu = action->menu())
    for (auto *child : menu->actions()) captureMenu(child, bindings);
}
// Run once after the main menu is constructed, before applying any translation.
inline void installMenu(QMainWindow &window) {
  if (window.findChild<QMenu *>(QStringLiteral("uiLanguageMenu"))) return;
  std::vector<MenuBinding> bindings;
  for (auto *action : window.menuBar()->actions()) captureMenu(action, bindings);
  auto *menu = window.menuBar()->addMenu(QStringLiteral("Language"));
  menu->setObjectName(QStringLiteral("uiLanguageMenu"));
  auto *group = new QActionGroup(menu);
  group->setExclusive(true);
  const auto initial = saved();
  auto apply = [bindings](Language language) {
    for (const auto &binding : bindings) {
      if (!binding.action) continue;
      binding.action->setText(menuText(binding.text, language));
      if (binding.distinctIconText)
        binding.action->setIconText(menuText(binding.iconText, language));
      if (binding.distinctToolTip)
        binding.action->setToolTip(menuText(binding.toolTip, language));
    }
  };
  struct Choice { Language value; const char *label; };
  const Choice choices[] = {{Language::English, "English"},
                            {Language::Korean, "Korean"},
                            {Language::Japanese, "Japanese"}};
  for (const auto &choice : choices) {
    auto *action = menu->addAction(QString::fromLatin1(choice.label));
    action->setObjectName(QStringLiteral("uiLanguage_") + code(choice.value));
    action->setCheckable(true);
    group->addAction(action);
    action->setChecked(choice.value == initial);
    QObject::connect(action, &QAction::triggered, menu,
                     [apply, value = choice.value] {
      apply(value);
      remember(value);
    });
  }
  apply(initial);
}
} // namespace sparamview::ui_language
