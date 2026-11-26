#pragma once
#include "user_preferences.hpp"
#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <functional>
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
    {"+ Open Touchstone", "+ Touchstone 열기", "+ Touchstoneを開く"},
    {"Open Sample", "예제 열기", "サンプルを開く"},
    {"Export XLSX", "XLSX 내보내기", "XLSXをエクスポート"},
    {"Open Project", "프로젝트 열기", "プロジェクトを開く"},
    {"Open an SParamView project (.siproject)", "SParamView 프로젝트 열기 (.siproject)", "SParamViewプロジェクトを開く (.siproject)"},
    {"Save the current SParamView project", "현재 SParamView 프로젝트 저장", "現在のSParamViewプロジェクトを保存"},
    {"REVISION / CHANNEL", "리비전 / 채널", "リビジョン / チャネル"},
    {"Search Channels...", "채널 검색...", "チャネルを検索..."},
    {"Select All", "전체 선택", "すべて選択"},
    {"Clear Selection", "선택 해제", "選択解除"},
    {"Channel Mapping…", "채널 매핑…", "チャネルマッピング…"},
    {"Quick Analysis", "빠른 분석", "クイック解析"},
    {"QUICK ANALYSIS", "빠른 분석", "クイック解析"},
    {"Return Loss", "반사 손실", "反射損失"},
    {"Insertion Loss", "삽입 손실", "挿入損失"},
    {"Advanced ▸", "고급 ▸", "詳細 ▸"},
    {"Advanced ▾", "고급 ▾", "詳細 ▾"},
    {"Show advanced analysis settings", "고급 분석 설정 표시", "詳細解析設定を表示"},
    {"Freq", "주파수", "周波数"},
    {"Auto", "자동", "自動"},
    {"Auto Z₀", "자동 Z₀", "自動 Z₀"},
    {"Near → Far", "근단 → 원단", "近端 → 遠端"},
    {"Far → Near", "원단 → 근단", "遠端 → 近端"},
    {"Both", "양방향", "双方向"},
    {"Direction: Near → Far → Far → Near → Both", "방향: 근단 → 원단 / 원단 → 근단 / 양방향", "方向: 近端 → 遠端 / 遠端 → 近端 / 双方向"},
    {"Worst", "최악", "最悪"},
    {"Top 3", "상위 3개", "上位3件"},
    {"Top 5", "상위 5개", "上位5件"},
    {"Off", "끄기", "オフ"},
    {"Enable RL PASS/FAIL limit", "RL 합격/불합격 기준 활성화", "RL合否判定基準を有効化"},
    {"Enable IL PASS/FAIL limit", "IL 합격/불합격 기준 활성화", "IL合否判定基準を有効化"},
    {"Enable NEXT PASS/FAIL limit", "NEXT 합격/불합격 기준 활성화", "NEXT合否判定基準を有効化"},
    {"Enable FEXT PASS/FAIL limit", "FEXT 합격/불합격 기준 활성화", "FEXT合否判定基準を有効化"},
    {"Enable TDR impedance tolerance PASS/FAIL limit", "TDR 임피던스 허용 편차 판정 활성화", "TDRインピーダンス許容偏差の合否判定を有効化"},
    {"Return Loss plot", "반사 손실 그래프", "反射損失グラフ"},
    {"Insertion Loss plot", "삽입 손실 그래프", "挿入損失グラフ"},
    {"NEXT plot", "NEXT 그래프", "NEXTグラフ"},
    {"FEXT plot", "FEXT 그래프", "FEXTグラフ"},
    {"Marker [GHz]", "마커 [GHz]", "マーカー [GHz]"},
    {"Performance", "성능", "性能"},
    {"Low", "낮음", "低"},
    {"Normal", "보통", "標準"},
    {"Maximum", "최대", "最大"},
    {"Aggressor", "간섭 신호", "アグレッサー"},
    {"TDR time [ns]", "TDR 시간 [ns]", "TDR時間 [ns]"},
    {"Peak prominence", "피크 돌출도", "ピークプロミネンス"},
    {"Frequency Limit…", "주파수별 기준…", "周波数別判定基準…"},
    {"Select a plot from the completed analysis. Run Quick Analysis to recalculate.", "완료된 분석의 그래프를 선택합니다. 다시 계산하려면 빠른 분석을 실행하십시오.", "完了した解析のグラフを選択します。再計算するにはクイック解析を実行してください。"},
    {"TDR is shown on separate impedance scales for single-ended and differential channels. With Auto target these are nominally 50 Ω and 100 Ω.", "TDR은 단일 종단 및 차동 채널에 별도의 임피던스 스케일을 사용합니다. 자동 기준은 각각 공칭 50 Ω 및 100 Ω입니다.", "TDRはシングルエンドと差動チャネルで別々のインピーダンススケールを使用します。自動目標値は公称50 Ωおよび100 Ωです。"},
    {"Heatmap", "히트맵", "ヒートマップ"},
    {"Margin", "마진", "マージン"},
    {"Box Zoom", "영역 확대", "範囲拡大"},
    {"Previous View", "이전 보기", "前の表示"},
    {"Axis Limits", "축 범위", "軸範囲"},
    {"Fit All", "전체 맞춤", "全体表示"},
    {"Wheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom", "휠: X 확대/축소 · Ctrl+휠: Y 확대/축소 · 드래그: 이동 · Shift+드래그: 영역 확대", "ホイール: X拡大/縮小 · Ctrl+ホイール: Y拡大/縮小 · ドラッグ: 移動 · Shift+ドラッグ: 範囲拡大"},
    {"Compare Revisions", "리비전 비교", "リビジョン比較"},
    {"Table Font", "표 글꼴", "表のフォント"},
    {"Worst Margin", "최악 마진", "最悪マージン"},
    {"Worst Value", "최악값", "最悪値"},
    {"Channel", "채널", "チャネル"},
    {"Result", "결과", "結果"},
    {"Revision", "리비전", "リビジョン"},
    {"Parameter", "파라미터", "パラメータ"},
    {"Quality", "품질", "品質"},
    {"Termination / Unit", "종단 / 단위", "終端 / 単位"},
    {"Cancel", "취소", "キャンセル"},
  };
  for (const auto &entry : entries)
    if (source == QString::fromUtf8(entry.en))
      return QString::fromUtf8(value == Language::Korean ? entry.ko : entry.ja);
  return source;
}
// Read only the UI language property, never the numeric locale.
inline Language current(const QObject *object) {
  for (auto *p = object; p; p = p->parent()) {
    const auto value = p->property("sparamview.uiLanguage");
    if (value.isValid()) return fromCode(value.toString());
  }
  return Language::English;
}
inline QString textFor(const QObject *object, const QString &source) {
  return menuText(source, current(object));
}
using Refresh = std::function<void(Language)>;
using Bindings = std::vector<Refresh>;
// The binding always retains the English source. It never translates user data
// and never reconstructs a widget. Call only for stable presentation properties.
template <typename Widget, typename Setter>
inline void bind(Bindings &out, Widget *widget, const QString &source, Setter setter) {
  if (!widget || source.isEmpty() || menuText(source, Language::Korean) == source)
    return;
  out.push_back([guard = QPointer<Widget>(widget), source, setter](Language value) {
    if (guard) setter(guard.data(), menuText(source, value));
  });
}
inline void bindCombo(Bindings &out, QComboBox *box) {
  QStringList source;
  for (int i = 0; i < box->count(); ++i) source << box->itemText(i);
  out.push_back([guard = QPointer<QComboBox>(box), source](Language value) {
    if (!guard || guard->count() != source.size()) return;
    const QSignalBlocker blocker(guard.data());
    for (int i = 0; i < source.size(); ++i)
      guard->setItemText(i, menuText(source[i], value));
  });
}
inline void bindMainFixedWidgets(Bindings &out, QWidget *body) {
  // Capture only fixed labels and buttons from the main window constructor.
  // Do not walk dialog contents, data rows, revisions, channel names, or
  // status widgets that are updated by the analysis engine.
  const QStringList fixedLabels = {"REVISION / CHANNEL", "Freq", "Marker [GHz]",
      "Performance", "Aggressor", "TDR time [ns]", "Peak prominence", "Table Font"};
  const QStringList fixedButtons = {"+ Open Touchstone", "Open Sample", "Export XLSX",
      "Open Project", "Save Project", "Select All", "Clear Selection", "Channel Mapping…",
      "QUICK ANALYSIS", "Frequency Limit…", "Heatmap", "Margin", "Box Zoom",
      "Previous View", "Axis Limits", "Fit All", "Compare Revisions", "Cancel"};
  for (auto *label : body->findChildren<QLabel *>())
    if (fixedLabels.contains(label->text()))
      bind(out, label, label->text(), [](QLabel *w, const QString &s) { w->setText(s); });
  for (auto *button : body->findChildren<QAbstractButton *>()) {
    if (fixedButtons.contains(button->text()))
      bind(out, button, button->text(), [](QAbstractButton *w, const QString &s) { w->setText(s); });
    // Action-backed tools receive their labels from QAction. The TDR option
    // tooltip is still used as an English identity by the compact-layout pass.
    auto *tool = qobject_cast<QToolButton *>(button);
    if ((!tool || !tool->defaultAction()) &&
        !button->toolTip().startsWith("TDR options:") &&
        button->objectName() != "directionToggle")
      bind(out, button, button->toolTip(), [](QAbstractButton *w, const QString &s) { w->setToolTip(s); });
  }
  for (auto *box : body->findChildren<QGroupBox *>())
    if (box->title() == "Quick Analysis")
      bind(out, box, box->title(), [](QGroupBox *w, const QString &s) { w->setTitle(s); });
  for (auto *edit : body->findChildren<QLineEdit *>())
    if (edit->placeholderText() == "Search Channels...")
      bind(out, edit, edit->placeholderText(), [](QLineEdit *w, const QString &s) { w->setPlaceholderText(s); });
  for (auto *spin : body->findChildren<QDoubleSpinBox *>())
    bind(out, spin, spin->specialValueText(), [](QDoubleSpinBox *w, const QString &s) { w->setSpecialValueText(s); });
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
                      action->toolTip() == QStringLiteral("Full Screen (F11)")});
  if (auto *menu = action->menu())
    for (auto *child : menu->actions()) captureMenu(child, bindings);
}
// Run once after the main menu is constructed, before applying any translation.
inline void installMenu(QMainWindow &window, Bindings extra = {}) {
  if (window.findChild<QMenu *>(QStringLiteral("uiLanguageMenu"))) return;
  std::vector<MenuBinding> bindings;
  for (auto *action : window.menuBar()->actions()) captureMenu(action, bindings);
  auto *menu = window.menuBar()->addMenu(QStringLiteral("Language"));
  menu->setObjectName(QStringLiteral("uiLanguageMenu"));
  auto *group = new QActionGroup(menu);
  group->setExclusive(true);
  const auto initial = saved();
  auto apply = [bindings, extra, owner = QPointer<QMainWindow>(&window)](Language language) {
    if (!owner) return;
    owner->setProperty("sparamview.uiLanguage", code(language));
    for (const auto &binding : bindings) {
      if (!binding.action) continue;
      binding.action->setText(menuText(binding.text, language));
      if (binding.distinctIconText)
        binding.action->setIconText(menuText(binding.iconText, language));
      if (binding.distinctToolTip)
        binding.action->setToolTip(menuText(binding.toolTip, language));
    }
    for (const auto &refresh : extra) refresh(language);
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
