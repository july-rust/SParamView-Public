#pragma once
#include "user_preferences.hpp"
#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QRegularExpression>
#include <QStatusBar>
#include <QDialogButtonBox>
#include <QPushButton>
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
    {"Source / Revision", "원본 / 리비전", "ソース / リビジョン"},
    {"Victim Channel", "피간섭 채널", "ビクティムチャネル"},
    {"Direction", "방향", "方向"},
    {"Forward", "정방향", "順方向"},
    {"Reverse", "역방향", "逆方向"},
    {"Fit model", "피팅 모델", "フィットモデル"},
    {"Linear", "선형", "線形"},
    {"Root + linear", "제곱근 + 선형", "平方根 + 線形"},
    {"Root + linear + quadratic", "제곱근 + 선형 + 이차", "平方根 + 線形 + 二次"},
    {"Start frequency", "시작 주파수", "開始周波数"},
    {"Stop frequency", "종료 주파수", "終了周波数"},
    {"Required: choose mode", "필수: 모드 선택", "必須: モードを選択"},
    {"Single-ended", "단일 종단", "シングルエンド"},
    {"Differential", "차동", "差動"},
    {"Required", "필수", "必須"},
    {"Alternating", "교번 패턴", "交互パターン"},
    {"Custom bits", "사용자 비트", "カスタムビット"},
    {"Binary sequence e.g. 00101110", "이진 시퀀스 예: 00101110", "ビット列の例: 00101110"},
    {"Eye overlay (2 UI)", "아이 중첩 (2 UI)", "アイ重ね合わせ (2 UI)"},
    {"TX / RX waveform", "TX / RX 파형", "TX / RX 波形"},
    {"Impulse / Unit pulse", "임펄스 / 단위 펄스", "インパルス / 単位パルス"},
    {"TX incident low (mV)", "TX 입사 저레벨 (mV)", "TX入射低レベル (mV)"},
    {"TX incident high (mV)", "TX 입사 고레벨 (mV)", "TX入射高レベル (mV)"},
    {"TX source (ohm)", "TX 소스 (Ω)", "TXソース (Ω)"},
    {"RX termination (ohm)", "RX 종단 (Ω)", "RX終端 (Ω)"},
    {"RX threshold (mV)", "RX 임계값 (mV)", "RXしきい値 (mV)"},
    {"RX input minimum (mV)", "RX 입력 최소 (mV)", "RX入力最小 (mV)"},
    {"RX input maximum (mV)", "RX 입력 최대 (mV)", "RX入力最大 (mV)"},
    {"Signal", "신호", "信号"},
    {"Pattern / FFT", "패턴 / FFT", "パターン / FFT"},
    {"PHY profile (example)", "PHY 프로파일 (예제)", "PHYプロファイル (例)"},
    {"Transfer mode", "전송 모드", "伝送モード"},
    {"NRZ bit rate", "NRZ 비트 전송률", "NRZビットレート"},
    {"Nyquist (coupled)", "나이퀴스트 (연동)", "ナイキスト (連動)"},
    {"Analysis cap", "분석 상한", "解析上限"},
    {"Pattern", "패턴", "パターン"},
    {"Custom pattern", "사용자 패턴", "カスタムパターン"},
    {"FFT samples", "FFT 샘플", "FFTサンプル"},
    {"Samples per UI", "UI당 샘플", "UIあたりのサンプル"},
    {"Required — physical reference-plane value", "필수 — 물리적 기준면 값", "必須 — 物理基準面の値"},
    {"NEXT / FEXT paths (check multiple)", "NEXT / FEXT 경로 (복수 선택)", "NEXT / FEXT経路 (複数選択)"},
    {"Independent-path RSS (assumed)", "독립 경로 RSS (가정)", "独立経路RSS (仮定)"},
    {"NRZ symbol rate", "NRZ 심볼 전송률", "NRZシンボルレート"},
    {"TX amplitude (pp)", "TX 진폭 (피크-피크)", "TX振幅 (ピーク間)"},
    {"Rise time", "상승 시간", "立ち上がり時間"},
    {"RX bandwidth", "RX 대역폭", "RX帯域幅"},
    {"Run", "실행", "実行"},
    {"Run Manual", "수동 실행", "手動実行"},
    {"Auto Preview", "자동 미리보기", "自動プレビュー"},
    {"Auto Analysis", "자동 분석", "自動解析"},
    {"Results / warnings", "결과 / 경고", "結果 / 警告"},
    {"How to read", "결과 읽는 법", "結果の読み方"},
    {"No numerical data. Analysis engine is not installed.", "수치 데이터 없음. 분석 엔진이 설치되지 않았습니다.", "数値データなし。解析エンジンがインストールされていません。"},
    {"All three graphs", "그래프 3개 모두", "3つのグラフすべて"},
    {"NEXT/FEXT power sum", "NEXT/FEXT 전력 합", "NEXT/FEXT電力和"},
    {"Weighted noise PSD", "가중 잡음 PSD", "重み付きノイズPSD"},
    {"Cumulative RMS (including total)", "누적 RMS (합계 포함)", "累積RMS (合計を含む)"},
    {"Export CSV", "CSV 내보내기", "CSVエクスポート"},
    {"Export PNG", "PNG 내보내기", "PNGエクスポート"},
    {"Channel Analysis", "채널 분석", "チャネル解析"},
    {"Reading ILD: deviation near 0 dB means smoother loss relative to its fitted trend. Large absolute peaks deserve investigation; low ILD alone does not mean low insertion loss. No standard PASS limit.", "ILD 읽는 법: 0 dB에 가까운 편차는 피팅 추세 대비 손실이 매끄러움을 의미합니다. 절댓값 피크가 크면 조사해야 합니다. 낮은 ILD가 낮은 삽입 손실을 의미하지는 않습니다. 표준 합격 기준은 없습니다.", "ILDの読み方: 0 dBに近い偏差はフィットした傾向に対して損失が滑らかであることを示します。絶対値ピークが大きい場合は要調査です。低ILDは低挿入損失を意味しません。標準合格基準はありません。"},
    {"Reading ICN: lower RMS and weighted PSD are preferable under identical TX/RX and band settings. Steep cumulative growth identifies noisy bands; compare path contributions. RSS assumes independent paths, not measured correlation. No standard PASS limit.", "ICN 읽는 법: 동일한 TX/RX 및 대역 설정에서 RMS와 가중 PSD가 낮을수록 좋습니다. 누적값이 급증하는 대역의 경로별 기여도를 비교하십시오. RSS는 독립 경로를 가정하며 측정된 상관관계가 아닙니다. 표준 합격 기준은 없습니다.", "ICNの読み方: 同一のTX/RXと帯域設定ではRMSと重み付きPSDが低い方が望ましいです。累積値が急増する帯域の経路寄与を比較してください。RSSは独立経路を仮定し、測定された相関ではありません。標準合格基準はありません。"},
    {"Reading Eye: larger eye height and width imply more simulated decision margin; closed or overlapping levels indicate risk. Threshold and sampling lines are model conditions, not a PHY mask or BER measurement.", "아이 읽는 법: 아이 높이와 폭이 클수록 시뮬레이션상 판정 마진이 큽니다. 닫히거나 겹친 레벨은 위험을 나타냅니다. 임계값 및 샘플링 선은 모델 조건이며 PHY 마스크 또는 BER 측정이 아닙니다.", "アイの読み方: アイ高さと幅が大きいほどシミュレーション上の判定マージンが大きくなります。閉じたアイや重なったレベルはリスクを示します。しきい値とサンプリング線はモデル条件であり、PHYマスクやBER測定ではありません。"},
    {"Simulated Channel Eye — Not Measured / Not PHY Compliance", "시뮬레이션 채널 아이 — 실측 아님 / PHY 적합성 검증 아님", "シミュレーションチャネルアイ — 実測ではない / PHY適合性試験ではない"},
    {"VALID", "유효", "有効"},
    {"WARNING", "경고", "警告"},
    {"STALE", "이전 결과", "旧結果"},
    {"NOT EVALUATED", "미평가", "未評価"},
    {"UNSUPPORTED", "미지원", "未対応"},
    {"INVALID SOURCE", "잘못된 원본", "無効なソース"},
    {"CANCELLED", "취소됨", "キャンセル済み"},
    {"ERROR", "오류", "エラー"},
    {"Calculation running", "계산 중", "計算中"},
    {"Engine not yet implemented; no simulated or conformant results", "엔진 미구현: 시뮬레이션 또는 적합성 결과 없음", "エンジン未実装: シミュレーション結果または適合結果なし"},
    {"Select NEXT/FEXT paths to display individual contributions.", "개별 기여도를 표시할 NEXT/FEXT 경로를 선택하십시오.", "個別の寄与を表示するNEXT/FEXT経路を選択してください。"},
    {"No current ICN result.", "현재 ICN 결과 없음.", "現在のICN結果なし。"},
    {"Run ILD to display measured/fitted IL and deviation.", "ILD를 실행하여 측정/피팅 IL 및 편차를 표시합니다.", "ILDを実行して測定/フィットILと偏差を表示します。"},
    {"Insertion loss (dB) — raw blue / fitted orange", "삽입 손실 (dB) — 원본 파랑 / 피팅 주황", "挿入損失 (dB) — 元データ青 / フィット橙"},
    {"ILD (dB) — near zero is better; dotted line: peak", "ILD (dB) — 0에 가까울수록 좋음; 점선: 피크", "ILD (dB) — 0に近いほど良好、点線: ピーク"},
    {"Power sum (dB) — zero floor −300 dB", "전력 합 (dB) — 0의 표시 하한 −300 dB", "電力和 (dB) — ゼロの表示下限 −300 dB"},
    {"Weighted PSD (mV²/GHz) — lower is better", "가중 PSD (mV²/GHz) — 낮을수록 좋음", "重み付きPSD (mV²/GHz) — 低いほど良好"},
    {"Cumulative RMS (mV) — lower is better", "누적 RMS (mV) — 낮을수록 좋음", "累積RMS (mV) — 低いほど良好"},
    {"NEXT (blue)  |  FEXT (orange)  |  Total (foreground)", "NEXT (파랑)  |  FEXT (주황)  |  합계 (전경색)", "NEXT (青)  |  FEXT (橙)  |  合計 (前景色)"},
    {"NEXT (blue)  |  FEXT (orange)", "NEXT (파랑)  |  FEXT (주황)", "NEXT (青)  |  FEXT (橙)"},
    {"Auto Preview or enter verified settings and Run Manual", "자동 미리보기 또는 검증된 설정을 입력하고 수동 실행", "自動プレビューまたは検証済み設定を入力して手動実行"},
    {"relative", "상대값", "相対値"},
    {"UI (2 symbols)", "UI (2 심볼)", "UI (2シンボル)"},
    {"Time (ps)", "시간 (ps)", "時間 (ps)"},
    {"Default exploratory model; uncheck to disallow multi-path RSS. Physical correlation is not established, including NEXT/FEXT from the same aggressor.", "기본 탐색 모델입니다. 다중 경로 RSS를 허용하지 않으려면 체크를 해제하십시오. 동일 간섭원 NEXT/FEXT를 포함한 물리적 상관관계는 확립되지 않았습니다.", "既定の探索モデルです。複数経路RSSを許可しない場合はチェックを外してください。同一アグレッサーのNEXT/FEXTを含め物理相関は確立されていません。"},
    {"Example PHY profile, not compliance", "PHY 예제 프로파일, 적합성 검증 아님", "PHY例示プロファイル、適合性試験ではない"},
    {"Assumed matched, 400 mVpp NRZ preview; uses source phase, qualified DC conditioning and bandwidth, not actual PHY settings.", "정합 및 400 mVpp NRZ를 가정한 미리보기입니다. 원본 위상, 검증된 DC 보정 및 대역폭을 사용하며 실제 PHY 설정은 아닙니다.", "整合および400 mVpp NRZを仮定したプレビューです。ソース位相、検証済みDC処理と帯域幅を使用し、実際のPHY設定ではありません。"},
    {"Derive measured band and use explicit exploratory assumptions; not a PHY or standard compliance test.", "측정 대역을 도출하고 명시적인 탐색 가정을 사용합니다. PHY 또는 표준 적합성 시험이 아닙니다.", "測定帯域を導出し、明示的な探索上の仮定を使用します。PHYまたは標準適合性試験ではありません。"},
    {"Ready", "준비 완료", "準備完了"},
    {"Ready · Local processing · Source read-only", "준비 완료 · 로컬 처리 · 원본 읽기 전용", "準備完了 · ローカル処理 · ソース読み取り専用"},
    {"Cancelling...", "취소 중...", "キャンセル中..."},
    {"Cancelled · Ready", "취소됨 · 준비 완료", "キャンセル済み · 準備完了"},
    {"Source SHA-256 verified", "원본 SHA-256 검증 완료", "ソースSHA-256検証済み"},
    {"Settings changed. Run analysis to apply them. Export uses the last completed analysis settings.", "설정이 변경되었습니다. 적용하려면 분석을 실행하십시오. 내보내기는 마지막 완료 분석 설정을 사용합니다.", "設定が変更されました。適用するには解析を実行してください。エクスポートは最後に完了した解析設定を使用します。"},
    {"Using completed results for the identical analysis request.", "동일한 분석 요청의 완료 결과를 사용합니다.", "同一解析要求の完了結果を使用します。"},
    {"Analysis complete. Frequency/time and margin values use full-resolution data. TDR starts from the lowest source frequency; check the preview quality notes.", "분석 완료. 주파수/시간 및 마진 값은 전체 해상도 데이터를 사용합니다. TDR은 원본 최저 주파수에서 시작합니다. 미리보기 품질 안내를 확인하십시오.", "解析完了。周波数/時間とマージンは全解像度データを使用します。TDRはソースの最低周波数から開始します。プレビュー品質の注記を確認してください。"},
    {"TDR options changed. Run TDR or Quick Analysis to update results.", "TDR 옵션이 변경되었습니다. TDR 또는 빠른 분석을 실행하여 결과를 갱신하십시오.", "TDRオプションが変更されました。TDRまたはクイック解析を実行して結果を更新してください。"},
    {"Mapping confirmed. Select channels and run analysis.", "매핑 확인 완료. 채널을 선택하고 분석을 실행하십시오.", "マッピング確認済み。チャネルを選択して解析を実行してください。"},
    {"New workspace ready. Previous workspace cache retired; cleanup may be deferred until worker handles are released.", "새 작업 공간이 준비되었습니다. 이전 캐시는 폐기되었으며 작업 핸들이 해제될 때까지 정리가 지연될 수 있습니다.", "新規ワークスペースの準備完了。以前のキャッシュは退役済みで、ワーカーハンドル解放まで削除が遅れる場合があります。"},
    {"Project restored. Reconfirm mapping for revisions whose source files have changed.", "프로젝트 복원 완료. 원본 파일이 변경된 리비전의 매핑을 다시 확인하십시오.", "プロジェクト復元済み。ソースファイルが変更されたリビジョンのマッピングを再確認してください。"},
    {"Frequency limits saved. Enable the corresponding limit checkbox and rerun analysis.", "주파수별 기준이 저장되었습니다. 해당 기준 체크박스를 활성화하고 분석을 다시 실행하십시오.", "周波数別判定基準を保存しました。対応するチェックボックスを有効にして再解析してください。"},
    {"Cancelling the active task. Close the window after cancellation completes.", "실행 중인 작업을 취소합니다. 취소 완료 후 창을 닫으십시오.", "実行中のタスクをキャンセルします。完了後にウィンドウを閉じてください。"},
    {"Project saved: %1", "프로젝트 저장 완료: %1", "プロジェクト保存済み: %1"},
    {"Excel workbook saved: %1", "Excel 통합 문서 저장 완료: %1", "Excelブック保存済み: %1"},
    {"TDR Options", "TDR 옵션", "TDRオプション"},
    {"Select multiple conditions to overlay TDR traces.\nForward: terminate Far / Reverse: terminate Near\nAll other physical ports are matched to their source reference impedances.", "TDR 트레이스를 중첩할 조건을 복수 선택하십시오.\n정방향: 원단 종단 / 역방향: 근단 종단\n나머지 물리 포트는 원본 기준 임피던스에 정합됩니다.", "TDRトレースを重ねる条件を複数選択してください。\n順方向: 遠端終端 / 逆方向: 近端終端\nその他の物理ポートはソース基準インピーダンスに整合します。"},
    {"Reference: matched to source reference impedance", "기준: 원본 기준 임피던스에 정합", "基準: ソース基準インピーダンスに整合"},
    {"Resistive: SE 50 Ω / Diff P–N 100 Ω (floating)", "저항: SE 50 Ω / 차동 P–N 100 Ω (부동)", "抵抗: SE 50 Ω / 差動 P–N 100 Ω (フローティング)"},
    {"Grounded: SE 50 Ω / Diff P–GND 50 Ω + N–GND 50 Ω", "접지: SE 50 Ω / 차동 P–GND 50 Ω + N–GND 50 Ω", "接地: SE 50 Ω / 差動 P–GND 50 Ω + N–GND 50 Ω"},
    {"Open: SE open / Diff P and N both open", "개방: SE 개방 / 차동 P와 N 모두 개방", "開放: SE開放 / 差動PとNの両方を開放"},
    {"Short: SE–GND short / Diff P–N short (floating)", "단락: SE–GND 단락 / 차동 P–N 단락 (부동)", "短絡: SE–GND短絡 / 差動P–N短絡 (フローティング)"},
    {"The two SE 50 Ω options are equivalent and are calculated once.\nReflection coefficient ρ is recommended for open circuits. These are ideal resistive/open/short conditions.", "두 SE 50 Ω 옵션은 동등하며 한 번만 계산합니다.\n개방 회로에는 반사 계수 ρ를 권장합니다. 이상적인 저항/개방/단락 조건입니다.", "2つのSE 50 Ωオプションは等価で、一度だけ計算します。\n開放回路には反射係数ρを推奨します。理想的な抵抗/開放/短絡条件です。"},
    {"Show Reflection Coefficient ρ", "반사 계수 ρ 표시", "反射係数ρを表示"},
    {"Useful for open/short comparisons. Impedance tolerance limits do not apply to ρ.", "개방/단락 비교에 유용합니다. 임피던스 허용 편차 기준은 ρ에 적용되지 않습니다.", "開放/短絡比較に有用です。インピーダンス許容偏差基準はρに適用されません。"},
    {"Select Terminations", "종단 선택", "終端を選択"},
    {"Select at least one termination condition.", "종단 조건을 하나 이상 선택하십시오.", "終端条件を1つ以上選択してください。"},
    {"Signal Type Detection", "신호 유형 감지", "信号タイプ検出"},
    {"Signal Type Changed", "신호 유형 변경", "信号タイプ変更"},
    {"Confirm the electrical signal type before Channel Mapping. You can still edit every physical P/N and Near/Far assignment in the next step.", "채널 매핑 전에 전기적 신호 유형을 확인하십시오. 다음 단계에서 모든 물리 P/N 및 근단/원단 할당을 편집할 수 있습니다.", "チャネルマッピングの前に電気的信号タイプを確認してください。次の段階ですべての物理P/Nと近端/遠端割り当てを編集できます。"},
    {"Select signal type…", "신호 유형 선택…", "信号タイプを選択…"},
    {"Mixed (Differential + Single-ended)", "혼합 (차동 + 단일 종단)", "混在 (差動 + シングルエンド)"},
    {"Continue to Channel Mapping", "채널 매핑 계속", "チャネルマッピングへ進む"},
    {"Group", "그룹", "グループ"},
    {"Alias", "별칭", "別名"},
    {"Mapping ID", "매핑 ID", "マッピングID"},
    {"Add Row", "행 추가", "行を追加"},
    {"Delete Selected Rows", "선택 행 삭제", "選択行を削除"},
    {"Suggest from Labels", "라벨 기반 제안", "ラベルから提案"},
    {"Suggest SE Half-Split", "SE 절반 분할 제안", "SE半分割を提案"},
    {"Manual Differential Pairing", "수동 차동 페어링", "手動差動ペアリング"},
    {"Channel name", "채널 이름", "チャネル名"},
    {"Near P", "근단 P", "近端P"},
    {"Near N", "근단 N", "近端N"},
    {"Far P", "원단 P", "遠端P"},
    {"Far N", "원단 N", "遠端N"},
    {"Create Differential Channel", "차동 채널 생성", "差動チャネルを作成"},
    {"Swap P/N", "P/N 교환", "P/N入れ替え"},
    {"Swap Near/Far", "근단/원단 교환", "近端/遠端入れ替え"},
    {"Physical Ports / Labels", "물리 포트 / 라벨", "物理ポート / ラベル"},
    {"Mapping CSV", "매핑 CSV", "マッピングCSV"},
    {"An even number of ports is required.", "짝수 개의 포트가 필요합니다.", "偶数個のポートが必要です。"},
    {"Select both Near P and Near N.", "근단 P와 N을 모두 선택하십시오.", "近端PとNの両方を選択してください。"},
    {"Select both Far P and Far N, or leave both blank.", "원단 P와 N을 모두 선택하거나 모두 비워 두십시오.", "遠端PとNの両方を選択するか、両方を空欄にしてください。"},
    {"A physical port cannot be used twice in one differential channel.", "하나의 차동 채널에 동일 물리 포트를 두 번 사용할 수 없습니다.", "1つの差動チャネルで同一物理ポートを2回使用できません。"},
    {"Select a mapping row first.", "매핑 행을 먼저 선택하십시오.", "先にマッピング行を選択してください。"},
    {"Define at least one channel.", "채널을 하나 이상 정의하십시오.", "チャネルを1つ以上定義してください。"},
    {"Excel Export", "Excel 내보내기", "Excelエクスポート"},
    {"Summary", "요약", "概要"},
    {"Detailed results", "상세 결과", "詳細結果"},
    {"Graphs", "그래프", "グラフ"},
    {"Raw trace data (large)", "원시 트레이스 데이터 (대용량)", "生トレースデータ (大容量)"},
    {"Save Excel Workbook", "Excel 통합 문서 저장", "Excelブックを保存"},
    {"Save Plot", "그래프 저장", "グラフを保存"},
    {"Raw CSV", "원시 CSV", "生CSV"},
    {"Save the modified settings and channel mapping?", "수정된 설정 및 채널 매핑을 저장하시겠습니까?", "変更した設定とチャネルマッピングを保存しますか？"},
    {"Run analysis first.", "먼저 분석을 실행하십시오.", "先に解析を実行してください。"},
    {"Save Limit Preset", "판정 기준 프리셋 저장", "判定基準プリセットを保存"},
    {"Load Limit Preset", "판정 기준 프리셋 불러오기", "判定基準プリセットを読み込む"},
    {"Frequency Limit", "주파수별 판정 기준", "周波数別判定基準"},
    {"Enter GHz, dB on each line. Clear the text to restore a constant limit.\nExample: 1, -10\nValues outside the defined range are N/A; limits are not extrapolated.", "각 줄에 GHz, dB를 입력하십시오. 텍스트를 지우면 고정 기준이 복원됩니다.\n예: 1, -10\n정의 범위 밖은 N/A이며 기준을 외삽하지 않습니다.", "各行にGHz、dBを入力してください。テキストを消去すると固定基準に戻ります。\n例: 1, -10\n定義範囲外はN/Aで、基準の外挿は行いません。"},
    {"Locate Source File", "원본 파일 찾기", "ソースファイルを探す"},
    {"Project", "프로젝트", "プロジェクト"},
    {"Select Touchstone Files", "Touchstone 파일 선택", "Touchstoneファイルを選択"},
    {"About SParamView", "SParamView 정보", "SParamViewについて"},
    {"Zoom In", "확대", "拡大"},
    {"Axis Limits...", "축 범위...", "軸範囲..."},
    {"Plot Axis Limits", "그래프 축 범위", "グラフ軸範囲"},
    {"X Start", "X 시작", "X開始"},
    {"X Stop", "X 종료", "X終了"},
    {"Y Minimum", "Y 최소", "Y最小"},
    {"Y Maximum", "Y 최대", "Y最大"},
    {"Auto Y Range", "Y 자동 범위", "Y自動範囲"},
    {"Invalid Axis Range", "잘못된 축 범위", "無効な軸範囲"},
    {"The upper limit must be greater than the lower limit.", "상한은 하한보다 커야 합니다.", "上限は下限より大きくなければなりません。"},
    {"OK", "확인", "OK"},
    {"Save", "저장", "保存"},
    {"Discard", "저장 안 함", "破棄"},
    {"Yes", "예", "はい"},
    {"No", "아니요", "いいえ"},
    {"Open a Touchstone file", "Touchstone 파일 열기", "Touchstoneファイルを開く"},
    {"Open file → Confirm mapping → Select channels → QUICK ANALYSIS. Pass/fail evaluation requires user-defined limits.", "파일 열기 → 매핑 확인 → 채널 선택 → 빠른 분석. 합격/불합격 판정에는 사용자 정의 기준이 필요합니다.", "ファイルを開く → マッピング確認 → チャネル選択 → クイック解析。合否判定にはユーザー定義基準が必要です。"},
    {"No limits configured → N/A", "판정 기준 미설정 → N/A", "判定基準未設定 → N/A"},
    {"Inputs changed; previous analysis is invalid", "입력 변경: 이전 분석 결과 무효", "入力変更: 前回の解析結果は無効"},
    {"Context updated; select channels and run the verified engine", "분석 환경 갱신: 채널 선택 후 검증된 엔진을 실행하십시오.", "解析環境更新: チャネルを選択し検証済みエンジンを実行してください。"},
    {"Confirmed source, mapping and frequency band required", "확인된 원본·매핑·주파수 대역이 필요합니다.", "確認済みソース、マッピング、周波数帯域が必要です。"},
    {"Older result discarded after context/settings change", "환경 또는 설정 변경으로 이전 결과 폐기", "環境または設定変更により以前の結果を破棄"},
    {"Eye DC extrapolation", "아이 DC 외삽", "アイDC外挿"},
    {"Continue", "계속", "続行"},
    {"DC extrapolation cancelled", "DC 외삽 취소됨", "DC外挿キャンセル済み"},
    {"Cancellation requested; pending work cannot publish results", "취소 요청됨: 진행 중인 작업은 결과를 게시할 수 없습니다.", "キャンセル要求済み: 進行中の処理は結果を公開できません。"},
    {"Run verified ILD fit", "검증된 ILD 피팅 실행", "検証済みILDフィットを実行"},
    {"ILD source provider unavailable", "ILD 원본 공급자 사용 불가", "ILDソースプロバイダー利用不可"},
    {"Choose confirmed source, channel and frequency range", "확인된 원본·채널·주파수 범위를 선택하십시오.", "確認済みソース、チャネル、周波数範囲を選択してください。"},
    {"Missing confirmed physical channel identity", "확인된 물리 채널 식별 정보 없음", "確認済み物理チャネル識別情報なし"},
    {"Source revision changed; refresh analysis context", "원본 리비전 변경: 분석 환경을 갱신하십시오.", "ソースリビジョン変更: 解析環境を更新してください。"},
    {"Cancelled", "취소됨", "キャンセル済み"},
    {"Calculation cancelled", "계산 취소됨", "計算キャンセル済み"},
    {"Unknown worker failure", "알 수 없는 작업 오류", "不明なワーカーエラー"},
    {"Cannot open CSV output", "CSV 출력 파일을 열 수 없습니다.", "CSV出力を開けません。"},
    {"ILD CSV write failed", "ILD CSV 쓰기 실패", "ILD CSV書き込み失敗"},
    {"ILD PNG write failed", "ILD PNG 쓰기 실패", "ILD PNG書き込み失敗"},
    {"Run generic band-limited NRZ ICN", "일반 대역 제한 NRZ ICN 실행", "一般帯域制限NRZ ICNを実行"},
    {"ICN source provider unavailable", "ICN 원본 공급자 사용 불가", "ICNソースプロバイダー利用不可"},
    {"Select confirmed source and frequency band", "확인된 원본 및 주파수 대역을 선택하십시오.", "確認済みソースと周波数帯域を選択してください。"},
    {"Missing confirmed physical victim identity", "확인된 물리 피간섭 채널 식별 정보 없음", "確認済み物理ビクティム識別情報なし"},
    {"Invalid aggressor mapping", "잘못된 간섭원 매핑", "無効なアグレッサーマッピング"},
    {"Select 1–32 NEXT/FEXT aggressor paths", "NEXT/FEXT 간섭 경로 1~32개를 선택하십시오.", "NEXT/FEXTアグレッサー経路を1～32個選択してください。"},
    {"Multiple paths require the independent-path RSS model. Enable the model or select one path.", "복수 경로에는 독립 경로 RSS 모델이 필요합니다. 모델을 활성화하거나 경로 하나를 선택하십시오.", "複数経路には独立経路RSSモデルが必要です。モデルを有効化するか、1経路を選択してください。"},
    {"Source revision changed; refresh analysis", "원본 리비전 변경: 분석을 갱신하십시오.", "ソースリビジョン変更: 解析を更新してください。"},
    {"ICN CSV write failed", "ICN CSV 쓰기 실패", "ICN CSV書き込み失敗"},
    {"ICN PNG write failed", "ICN PNG 쓰기 실패", "ICN PNG書き込み失敗"},
    {"Run phase-preserving simulated NRZ Eye with explicit electrical conditions", "명시적인 전기적 조건으로 위상 보존 NRZ 아이 시뮬레이션 실행", "明示的電気条件で位相を保持するNRZアイシミュレーションを実行"},
    {"Verified Eye source provider unavailable", "검증된 아이 원본 공급자 사용 불가", "検証済みアイソースプロバイダー利用不可"},
    {"Explicit signal mode, bit rate, reference frequency and analysis cap required", "명시적인 신호 모드·비트율·기준 주파수·분석 상한 필요", "明示的な信号モード、ビットレート、基準周波数、解析上限が必要"},
    {"Invalid finite TX/RX electrical input", "유효하지 않은 TX/RX 전기 입력값", "無効なTX/RX電気入力値"},
    {"Select a verified Touchstone revision and channel", "검증된 Touchstone 리비전과 채널을 선택하십시오.", "検証済みTouchstoneリビジョンとチャネルを選択してください。"},
    {"Confirmed physical port mapping required", "확인된 물리 포트 매핑 필요", "確認済み物理ポートマッピングが必要"},
    {"Source SHA/revision changed; refresh analysis", "원본 SHA/리비전 변경: 분석을 갱신하십시오.", "ソースSHA/リビジョン変更: 解析を更新してください。"},
    {"Eye CSV write/commit failed", "아이 CSV 쓰기/저장 실패", "アイCSV書き込み/保存失敗"},
    {"Eye PNG write/commit failed", "아이 PNG 쓰기/저장 실패", "アイPNG書き込み/保存失敗"},
    {"Cannot open Eye CSV", "아이 CSV 파일을 열 수 없습니다.", "アイCSVを開けません。"},
    {"Cannot open ICN CSV", "ICN CSV 파일을 열 수 없습니다.", "ICN CSVを開けません。"},
    {"Eye graph cannot be rendered", "아이 그래프를 그릴 수 없습니다.", "アイグラフを描画できません。"},
    {"Custom pattern must contain only 0 and 1", "사용자 패턴에는 0과 1만 입력할 수 있습니다.", "カスタムパターンには0と1のみ入力できます。"},
    {"Export ILD data", "ILD 데이터 내보내기", "ILDデータをエクスポート"},
    {"Export ILD plot", "ILD 그래프 내보내기", "ILDグラフをエクスポート"},
    {"Export ICN numerical data", "ICN 수치 데이터 내보내기", "ICN数値データをエクスポート"},
    {"Export ICN plot", "ICN 그래프 내보내기", "ICNグラフをエクスポート"},
    {"Export simulated Eye numerical data", "시뮬레이션 아이 수치 데이터 내보내기", "シミュレーションアイ数値データをエクスポート"},
    {"Export simulated Eye graph", "시뮬레이션 아이 그래프 내보내기", "シミュレーションアイグラフをエクスポート"},
    {"Band RMS %1 dB | peak abs %2 dB @ %3 GHz | %4 samples | %5–%6 GHz. Bandwidth-weighted fit; no standard PASS", "대역 RMS %1 dB | 절대 피크 %2 dB @ %3 GHz | 샘플 %4개 | %5–%6 GHz. 대역폭 가중 피팅; 표준 합격 판정 아님", "帯域RMS %1 dB | 絶対ピーク %2 dB @ %3 GHz | %4サンプル | %5–%6 GHz。帯域幅重み付きフィット、標準合格判定ではない"},
    {"Path %1 %2 %3: %4 mV RMS (%5% power)", "경로 %1 %2 %3: %4 mV RMS (전력 %5%)", "経路 %1 %2 %3: %4 mV RMS (電力%5%)"},
    {"Largest contribution: %1 %2 (%3% of noise power).\n", "최대 기여 경로: %1 %2 (잡음 전력의 %3%).\n", "最大寄与経路: %1 %2 (ノイズ電力の%3%)。\n"},
    {"NEXT %1 mV RMS | FEXT %2 mV RMS | Total %3 mV RMS | %4 paths%5 | %6–%7 GHz — Generic independent-path NRZ RSS, no standard PASS", "NEXT %1 mV RMS | FEXT %2 mV RMS | 합계 %3 mV RMS | %4개 경로%5 | %6–%7 GHz — 일반 독립 경로 NRZ RSS, 표준 합격 판정 아님", "NEXT %1 mV RMS | FEXT %2 mV RMS | 合計 %3 mV RMS | %4経路%5 | %6–%7 GHz — 一般独立経路NRZ RSS、標準合格判定ではない"},
    {" | BAND LIMITED", " | 대역 제한", " | 帯域制限"},
    {"AUTO PREVIEW — assumed TX ±200 mV, matched Zref %1 / %2 ohm, threshold 0 mV, RX ±1000 mV, PRBS7. Not actual PHY settings.\n", "자동 미리보기 — TX ±200 mV, 정합 Zref %1 / %2 Ω, 임계값 0 mV, RX ±1000 mV, PRBS7 가정. 실제 PHY 설정 아님.\n", "自動プレビュー — TX ±200 mV、整合Zref %1 / %2 Ω、しきい値0 mV、RX ±1000 mV、PRBS7を仮定。実際のPHY設定ではない。\n"},
    {"MANUAL SETTINGS\n", "수동 설정\n", "手動設定\n"},
    {"DC: %1 | Original %2 Hz | Effective %3 Hz | First positive %4 Hz | %5\n%6\n", "DC: %1 | 원본 %2 Hz | 유효 %3 Hz | 첫 양의 주파수 %4 Hz | %5\n%6\n", "DC: %1 | 元 %2 Hz | 有効 %3 Hz | 最初の正周波数 %4 Hz | %5\n%6\n"},
    {"LIMITED", "제한됨", "制限あり"},
    {"LOCAL CHECKS PASSED", "국소 검사 통과", "局所検査合格"},
    {"AC-like pole %1 Hz | Fit residual %2 | RC time constant %3 us | Analytic frame droop %4% | Full baseline wander NOT QUALIFIED\n", "AC 유사 극점 %1 Hz | 피팅 잔차 %2 | RC 시정수 %3 us | 해석적 프레임 드룹 %4% | 전체 기준선 변동 미검증\n", "AC状極 %1 Hz | フィット残差 %2 | RC時定数 %3 us | 解析的フレームドループ %4% | 全ベースライン変動未検証\n"},
    {"Ideal RC only: longest periodic run %1 bits, droop %2%; at %3 bits (>=5 tau), remaining %4%.\n", "이상적 RC만: 최장 주기 연속 %1비트, 드룹 %2%; %3비트 (>=5 tau)에서 잔류 %4%.\n", "理想RCのみ: 最長周期連続 %1ビット、ドループ %2%; %3ビット (>=5 tau)で残留 %4%。\n"},
    {"Isolated modeled step [1/3/5 tau] = %1 / %2 / %3 V/V | grid delta %4 | %5 (not physical qualification)\n", "독립 모델 계단 응답 [1/3/5 tau] = %1 / %2 / %3 V/V | 격자 차이 %4 | %5 (물리 검증 아님)\n", "独立モデルステップ [1/3/5 tau] = %1 / %2 / %3 V/V | グリッド差 %4 | %5 (物理検証ではない)\n"},
    {"GRID AGREEMENT", "격자 일치", "グリッド一致"},
    {"GRID LIMITED", "격자 제한", "グリッド制限"},
    {"Isolated modeled long step: NOT EVALUATED\n", "독립 모델 장시간 계단 응답: 미평가\n", "独立モデル長時間ステップ: 未評価\n"},
    {"Coupling model: %1 (not physical topology proof)\n", "커플링 모델: %1 (물리 토폴로지 입증 아님)\n", "結合モデル: %1 (物理トポロジーの証明ではない)\n"},
    {"Simulated Channel Eye — Not Measured / Not PHY Compliance\nHeight %1 mV | Width %2 ps (%3 UI) | Max-height phase %4 UI | Delay %5 ps\nLow maximum %6 mV | High minimum %7 mV | Received [%8, %9] mV\nBit rate %10 Gbps | Reference %11 GHz | Analysis cap %12 GHz", "시뮬레이션 채널 아이 — 실측 아님 / PHY 적합성 검증 아님\n높이 %1 mV | 폭 %2 ps (%3 UI) | 최대 높이 위상 %4 UI | 지연 %5 ps\n저레벨 최대 %6 mV | 고레벨 최소 %7 mV | 수신 [%8, %9] mV\n비트율 %10 Gbps | 기준 %11 GHz | 분석 상한 %12 GHz", "シミュレーションチャネルアイ — 実測ではない / PHY適合性試験ではない\n高さ %1 mV | 幅 %2 ps (%3 UI) | 最大高さ位相 %4 UI | 遅延 %5 ps\n低レベル最大 %6 mV | 高レベル最小 %7 mV | 受信 [%8, %9] mV\nビットレート %10 Gbps | 基準 %11 GHz | 解析上限 %12 GHz"},
    {"\n%1 | %2 | Frame %3 bits | Overlay %4 traces | Width grid 1/%5 UI | Phase threshold: %6", "\n%1 | %2 | 프레임 %3비트 | 중첩 %4 트레이스 | 폭 격자 1/%5 UI | 위상 임계값: %6", "\n%1 | %2 | フレーム %3ビット | 重ね合わせ %4トレース | 幅グリッド 1/%5 UI | 位相しきい値: %6"},
    {"OPEN", "열림", "開口"},
    {"NOT OPEN", "열리지 않음", "非開口"},
    {"\nDecision margin exists at selected phase under this model (not a mask PASS).", "\n이 모델의 선택 위상에 판정 마진이 있습니다 (마스크 합격 아님).", "\nこのモデルの選択位相に判定マージンがあります (マスク合格ではない)。"},
    {"\nAttention: threshold not open at selected phase under this model.", "\n주의: 이 모델의 선택 위상에서 임계값 개구가 없습니다.", "\n注意: このモデルの選択位相でしきい値の開口がありません。"},
    {"Overall", "종합", "総合"},
    {"Metric", "측정 항목", "測定項目"},
    {"Worst value", "최악값", "最悪値"},
    {"Worst Frequency Hz / Time s", "최악값 주파수 Hz / 시간 s", "最悪値周波数 Hz / 時間 s"},
    {"Limit at worst margin", "최악 마진의 기준", "最悪マージンの基準"},
    {"Worst margin", "최악 마진", "最悪マージン"},
    {"Margin Frequency Hz / Time s", "마진 주파수 Hz / 시간 s", "マージン周波数 Hz / 時間 s"},
    {"Minimum", "최소", "最小"},
    {"Target ohm", "목표 Ω", "目標 Ω"},
    {"Reference ohm", "기준 Ω", "基準 Ω"},
    {"TDR quality", "TDR 품질", "TDR品質"},
    {"Note", "참고", "注記"},
    {"Min error %", "최소 오차 %", "最小誤差 %"},
    {"Max error %", "최대 오차 %", "最大誤差 %"},
    {"Max directional delta dB", "최대 방향 차이 dB", "最大方向差 dB"},
    {"Directional delta frequency Hz", "방향 차이 주파수 Hz", "方向差周波数 Hz"},
    {"Termination", "종단", "終端"},
    {"Value unit", "값 단위", "値の単位"},
    {"Current view followed by result graphs (up to 8 traces per image)", "현재 보기 이후 결과 그래프 (이미지당 최대 8개 트레이스)", "現在の表示に続いて結果グラフ (画像ごとに最大8トレース)"},
    {"All groups", "전체 그룹", "全グループ"},
    {"AUTO (same group)", "자동 (같은 그룹)", "自動 (同じグループ)"},
    {"%1 ports · %2 points\n%3 · %4\nSignal: %5 (%6)\nSource: %7\nMapping: %8", "포트 %1개 · 지점 %2개\n%3 · %4\n신호: %5 (%6)\n원본: %7\n매핑: %8", "ポート%1個 · 点数%2\n%3 · %4\n信号: %5 (%6)\nソース: %7\nマッピング: %8"},
    {"Touchstone data loaded. Mapping validated from %1. Select channels and run analysis.", "Touchstone 데이터 로드 완료. %1에서 매핑 검증 완료. 채널을 선택하고 분석을 실행하십시오.", "Touchstoneデータ読み込み済み。%1のマッピングを検証済み。チャネルを選択して解析してください。"},
    {"Touchstone data loaded. Mapping review is required before analysis.", "Touchstone 데이터 로드 완료. 분석 전에 매핑 확인이 필요합니다.", "Touchstoneデータ読み込み済み。解析前にマッピング確認が必要です。"},
    {"Skipped %1: mapping not confirmed.", "%1 건너뜀: 매핑 미확인.", "%1をスキップ: マッピング未確認。"},
    {"Overall %1   ·   OK %2   NG %3   N/A %4", "종합 %1   ·   OK %2   NG %3   N/A %4", "総合 %1   ·   OK %2   NG %3   N/A %4"},
    {"<b>Detected:</b> %1 &nbsp;&nbsp; <b>Confidence:</b> %2", "<b>감지:</b> %1 &nbsp;&nbsp; <b>신뢰도:</b> %2", "<b>検出:</b> %1 &nbsp;&nbsp; <b>信頼度:</b> %2"},
    {"<b>Evidence:</b> ", "<b>근거:</b> ", "<b>根拠:</b> "},
    {"<br><br>SParamView recognizes a name-based P/N pair only when two net names have the same length and differ at exactly one character, where that character is P versus N. The P/N position may be anywhere in the name. Irregular or ambiguous names are not guessed.", "<br><br>SParamView는 두 네트 이름 길이가 같고 한 문자만 다르며 해당 문자가 P와 N일 때만 이름 기반 P/N 쌍을 인식합니다. P/N 위치는 이름의 어느 곳이든 가능합니다. 불규칙하거나 모호한 이름은 추정하지 않습니다.", "<br><br>SParamViewは2つのネット名の長さが同じで1文字のみがPとNで異なる場合に限り、名前に基づくP/Nペアを認識します。P/Nの位置は名前の任意の場所です。不規則または曖昧な名前は推測しません。"},
    {"Signal Type: %1 (%2)\nEvidence: %3\nSource: %4\nPort numbering starts at 1. Leave Near−/Far− blank for single-ended channels.\nVerify Near/Far and P/N against the physical connections. A blank Far end supports reflection/TDR only.", "신호 유형: %1 (%2)\n근거: %3\n원본: %4\n포트 번호는 1부터 시작합니다. 단일 종단 채널은 Near−/Far−를 비워 두십시오.\n물리 연결과 근단/원단 및 P/N을 대조하십시오. 원단이 비어 있으면 반사/TDR만 지원됩니다.", "信号タイプ: %1 (%2)\n根拠: %3\nソース: %4\nポート番号は1から始まります。シングルエンドチャネルはNear−/Far−を空欄にしてください。\n物理接続と近端/遠端、P/Nを照合してください。遠端が空欄の場合は反射/TDRのみ対応します。"},
    {"\nUnmapped physical ports: ", "\n미매핑 물리 포트: ", "\n未マッピング物理ポート: "},
    {"File data loaded, but automatic mapping could not be confirmed. Check port labels and enter the mapping manually or import a Mapping CSV.\n", "파일 데이터 로드 완료. 자동 매핑을 확인하지 못했습니다. 포트 라벨을 확인하고 직접 매핑하거나 매핑 CSV를 가져오십시오.\n", "ファイルデータを読み込みましたが、自動マッピングを確認できませんでした。ポートラベルを確認し、手動入力またはマッピングCSVをインポートしてください。\n"},
    {"Reflection coefficient ρ [1]", "반사 계수 ρ [1]", "反射係数ρ [1]"},
    {"Impedance [Ω]", "임피던스 [Ω]", "インピーダンス [Ω]"},
    {"Sdd[response, stimulus] [dB]", "Sdd[응답, 자극] [dB]", "Sdd[応答, 刺激] [dB]"},
    {"S[response, stimulus] [dB]", "S[응답, 자극] [dB]", "S[応答, 刺激] [dB]"},
    {"S / Sdd[response, stimulus] [dB]", "S / Sdd[응답, 자극] [dB]", "S / Sdd[応答, 刺激] [dB]"},
    {"TDR quality: GOOD %1  /  LIMITED %2  /  UNSUITABLE %3", "TDR 품질: 양호 %1  /  제한 %2  /  부적합 %3", "TDR品質: 良好 %1  /  制限 %2  /  不適切 %3"},
    {"Both directions", "양방향", "双方向"},
    {"%1 traces", "트레이스 %1개", "%1トレース"},
    {"Open Touchstone → Select channels → QUICK ANALYSIS", "Touchstone 열기 → 채널 선택 → 빠른 분석", "Touchstoneを開く → チャネル選択 → クイック解析"},
    {"Quick TDR unavailable · UNSUITABLE\n\nThe current input data and transform settings cannot produce a Quick TDR waveform.\nDo not use this result for impedance evaluation.\n\n", "빠른 TDR 사용 불가 · 부적합\n\n현재 입력 데이터 및 변환 설정으로 빠른 TDR 파형을 생성할 수 없습니다.\n이 결과를 임피던스 평가에 사용하지 마십시오.\n\n", "クイックTDR利用不可 · 不適切\n\n現在の入力データと変換設定ではクイックTDR波形を生成できません。\nこの結果をインピーダンス評価に使用しないでください。\n\n"},
    {"Time [ns]", "시간 [ns]", "時間 [ns]"},
    {"Frequency [GHz]", "주파수 [GHz]", "周波数 [GHz]"},
    {"Analysis: %1 – %2 %3  |  Limit: %4", "분석: %1 – %2 %3  |  기준: %4", "解析: %1 – %2 %3  |  判定基準: %4"},
    {"Target ± ", "목표 ± ", "目標 ± "},
    {"Off / N/A", "끄기 / N/A", "オフ / N/A"},
    {"value ≥ ", "값 ≥ ", "値 ≥ "},
    {"value ≤ ", "값 ≤ ", "値 ≤ "},
    {"Frequency mask", "주파수 마스크", "周波数マスク"},
    {"Worst margin per cell • Red = NG • Gray = N/A", "셀별 최악 마진 • 빨강 = NG • 회색 = N/A", "セルごとの最悪マージン • 赤 = NG • 灰色 = N/A"},
    {"Worst value per cell • Teal = better • Red = worse", "셀별 최악값 • 청록 = 양호 • 빨강 = 불량", "セルごとの最悪値 • 青緑 = 良好 • 赤 = 悪化"},
    {"+ %1 traces; select result rows", "+ 트레이스 %1개; 결과 행 선택", "+ %1トレース、結果行を選択"},
    {" • wheel: X zoom | Ctrl: Y | drag: pan | Shift+drag: box", " • 휠: X 확대/축소 | Ctrl: Y | 드래그: 이동 | Shift+드래그: 영역 확대", " • ホイール: X拡大/縮小 | Ctrl: Y | ドラッグ: 移動 | Shift+ドラッグ: 範囲拡大"},
    {"Option", "옵션", "オプション"},
    {"TDR options: reference impedance, terminations and reflection coefficient", "TDR 옵션: 기준 임피던스, 종단 및 반사 계수", "TDRオプション: 基準インピーダンス、終端、反射係数"},
    {"Apply the termination at the opposite end of the selected channel. All other physical ports are matched to their source reference impedances.", "선택한 채널의 반대쪽 끝에 종단을 적용합니다. 나머지 물리 포트는 원본 기준 임피던스에 정합됩니다.", "選択チャネルの反対端に終端を適用します。他の物理ポートはソース基準インピーダンスに整合します。"},
    {"Enter marker frequencies in GHz, separated by commas.", "마커 주파수를 GHz 단위로 쉼표로 구분하여 입력하십시오.", "マーカー周波数をGHz単位でカンマ区切りで入力してください。"},
    {"Port numbers must be integers greater than or equal to 1.", "포트 번호는 1 이상의 정수여야 합니다.", "ポート番号は1以上の整数でなければなりません。"},
    {"Cannot verify current workspace cache ownership.", "현재 작업 공간 캐시의 소유권을 확인할 수 없습니다.", "現在のワークスペースキャッシュの所有権を確認できません。"},
    {"Cannot create new private workspace cache.", "새 전용 작업 공간 캐시를 생성할 수 없습니다.", "新規の専用ワークスペースキャッシュを作成できません。"},
    {"Project save failed", "프로젝트 저장 실패", "プロジェクトの保存に失敗しました"},
    {"Cannot open project", "프로젝트를 열 수 없습니다.", "プロジェクトを開けません。"},
    {"Project file is too large", "프로젝트 파일이 너무 큽니다.", "プロジェクトファイルが大きすぎます。"},
    {"Invalid/unsupported project format", "잘못되었거나 지원되지 않는 프로젝트 형식", "無効または未対応のプロジェクト形式"},
    {"Cannot create staged project cache", "프로젝트 준비 캐시를 생성할 수 없습니다.", "プロジェクト準備用キャッシュを作成できません。"},
    {"Preset save failed", "프리셋 저장 실패", "プリセットの保存に失敗しました"},
    {"Cannot open preset", "프리셋을 열 수 없습니다.", "プリセットを開けません。"},
    {"Not an SParamView limit preset", "SParamView 판정 기준 프리셋이 아닙니다.", "SParamView判定基準プリセットではありません。"},
    {"TDR uses Target ± Tolerance.", "TDR은 목표값 ± 허용 편차를 사용합니다.", "TDRは目標値 ± 許容偏差を使用します。"},
    {"Expected format: GHz, dB", "필요한 형식: GHz, dB", "必要な形式: GHz, dB"},
    {"Invalid limit number", "잘못된 판정 기준 숫자", "無効な判定基準数値"},
    {"Select channels to analyze.", "분석할 채널을 선택하십시오.", "解析するチャネルを選択してください。"},
    {"No selected channels have valid mapped endpoints for the requested analysis.", "선택한 채널에 요청된 분석을 위한 유효한 매핑 끝점이 없습니다.", "選択したチャネルに要求された解析の有効なマッピング端点がありません。"},
    {"Select at least one analysis type.", "분석 유형을 하나 이상 선택하십시오.", "解析タイプを1つ以上選択してください。"},
    {"Select one result row to export as raw CSV.", "원시 CSV로 내보낼 결과 행을 하나 선택하십시오.", "生CSVへエクスポートする結果行を1つ選択してください。"},
    {"Add at least two revisions.", "리비전을 두 개 이상 추가하십시오.", "リビジョンを2つ以上追加してください。"},
    {"Compare TDR using plot overlays. Use dB deltas for RL/IL/NEXT/FEXT.", "TDR은 그래프 중첩으로 비교하십시오. RL/IL/NEXT/FEXT는 dB 차이를 사용하십시오.", "TDRはグラフの重ね合わせで比較してください。RL/IL/NEXT/FEXTにはdB差を使用してください。"},
    {"Cannot find examples/demo.siproject.", "examples/demo.siproject 파일을 찾을 수 없습니다.", "examples/demo.siprojectが見つかりません。"},
    {"Source hash changed. Reopen the Touchstone file.", "원본 해시 변경: Touchstone 파일을 다시 여십시오.", "ソースハッシュ変更: Touchstoneファイルを開き直してください。"},
    {"Cannot create private workspace cache", "전용 작업 공간 캐시를 생성할 수 없습니다.", "専用ワークスペースキャッシュを作成できません。"},
    {"Unknown startup failure", "알 수 없는 시작 오류", "不明な起動エラー"},
    {"SParamView - Startup failed", "SParamView - 시작 실패", "SParamView - 起動失敗"},
    {"Cannot start SParamView.\n\n", "SParamView를 시작할 수 없습니다.\n\n", "SParamViewを起動できません。\n\n"},
    {"\n\nCheck the cache location, folder permissions and available disk space, then restart the application.", "\n\n캐시 위치, 폴더 권한 및 디스크 여유 공간을 확인한 다음 프로그램을 다시 시작하십시오.", "\n\nキャッシュの場所、フォルダー権限、ディスク空き容量を確認し、アプリを再起動してください。"},
    {"Unexpected startup file-open failure", "시작 시 예기치 않은 파일 열기 실패", "起動時の予期しないファイルオープン失敗"},
    {"Unexpected non-standard startup exception", "시작 시 예기치 않은 비표준 예외", "起動時の予期しない非標準例外"},
    {"Context updated; analysis engine not installed", "분석 환경 갱신: 분석 엔진 미설치", "解析環境更新: 解析エンジン未インストール"},
    {"Revision SHA mismatch", "리비전 SHA 불일치", "リビジョンSHA不一致"},
    {"Exploratory ILD; no IEEE/MIPI PASS or FAIL", "탐색용 ILD: IEEE/MIPI 합격 또는 불합격 판정 아님", "探索用ILD: IEEE/MIPI合否判定ではない"},
    {"ICN source SHA mismatch", "ICN 원본 SHA 불일치", "ICNソースSHA不一致"},
    {"Auto ICN needs 1..32 compatible paths; use Manual to select a subset", "자동 ICN에는 호환 경로 1~32개가 필요합니다. 수동으로 일부 경로를 선택하십시오.", "自動ICNには互換経路1～32個が必要です。手動で一部の経路を選択してください。"},
    {"Auto ICN multiple paths require explicitly enabled independent-path RSS", "자동 ICN 복수 경로는 독립 경로 RSS의 명시적 활성화가 필요합니다.", "自動ICNの複数経路には独立経路RSSの明示的な有効化が必要です。"},
    {"ICN aggressor must be a different, compatible channel in the victim group", "ICN 간섭원은 피간섭 그룹 내의 다른 호환 채널이어야 합니다.", "ICNアグレッサーはビクティムグループ内の別の互換チャネルでなければなりません。"},
    {"Generic NRZ band-limited ICN; assumed independent paths, not IEEE/SNIA compliance", "일반 대역 제한 NRZ ICN: 독립 경로 가정, IEEE/SNIA 적합성 검증 아님", "一般帯域制限NRZ ICN: 独立経路を仮定、IEEE/SNIA適合性試験ではない"},
    {"Eye source SHA changed since selection", "선택 이후 아이 원본 SHA 변경", "選択後にアイソースSHAが変更されました"},
    {"Eye single-ended/differential mode disagrees with mapped channel", "아이 단일 종단/차동 모드가 매핑 채널과 불일치", "アイのシングルエンド/差動モードがマッピングチャネルと不一致"},
    {"Analysis worker capacity exceeded", "분석 작업 용량 초과", "解析ワーカー容量超過"},
    {"Cursor: %1 %2; simulated only", "커서: %1 %2; 시뮬레이션 전용", "カーソル: %1 %2; シミュレーションのみ"},
    {"SParamView\nVersion %1\nBuild: Windows %2\n\nCopyright © 2026 july\nsparamview@gmail.com\nLicensed under the MIT License\n\nC++20 / Qt 6\nProcessing is local. Original Touchstone files are not modified.", "SParamView\n버전 %1\n빌드: Windows %2\n\nCopyright © 2026 july\nsparamview@gmail.com\nMIT 라이선스\n\nC++20 / Qt 6\n로컬에서 처리하며 원본 Touchstone 파일은 수정하지 않습니다.", "SParamView\nバージョン %1\nビルド: Windows %2\n\nCopyright © 2026 july\nsparamview@gmail.com\nMITライセンス\n\nC++20 / Qt 6\nローカルで処理し、元のTouchstoneファイルは変更しません。"},
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
// Store the source template separately from its display rendering. Arguments are
// user data, never translation lookup keys.
inline void refreshMessage(QLabel *label, Language value) {
  const auto source=label->property("sparamview.messageSource");
  if(!source.isValid()) return;
  const auto pattern=menuText(source.toString(),value);
  const auto args=label->property("sparamview.messageArgs").toStringList();
  QString rendered;
  qsizetype offset=0;
  static const QRegularExpression placeholder(QStringLiteral("%([1-9][0-9]?)"));
  auto matches=placeholder.globalMatch(pattern);
  while(matches.hasNext()) {
    const auto match=matches.next();
    rendered+=pattern.mid(offset,match.capturedStart()-offset);
    const int index=match.captured(1).toInt()-1;
    rendered+=(index>=0 && index<args.size())?args[index]:match.captured(0);
    offset=match.capturedEnd();
  }
  rendered+=pattern.mid(offset);
  label->setText(rendered);
}
inline void message(QLabel *label,const QString &source,const QStringList &args={}) {
  label->setProperty("sparamview.messageSource",source);
  label->setProperty("sparamview.messageArgs",args);
  refreshMessage(label,current(label));
}
inline void refreshStatus(QStatusBar *bar, Language value) {
  const auto source=bar->property("sparamview.messageSource");
  if(source.isValid()) bar->showMessage(menuText(source.toString(),value));
}
inline void status(QStatusBar *bar,const QString &source) {
  bar->setProperty("sparamview.messageSource",source);
  refreshStatus(bar,current(bar));
}
inline void localizeButtons(QDialogButtonBox *box,const QObject *context) {
  for(auto *button:box->buttons()) {
    QString source;
    switch(box->standardButton(button)) {
      case QDialogButtonBox::Ok: source="OK";break;
      case QDialogButtonBox::Cancel: source="Cancel";break;
      case QDialogButtonBox::Save: source="Save";break;
      case QDialogButtonBox::Discard: source="Discard";break;
      case QDialogButtonBox::Yes: source="Yes";break;
      case QDialogButtonBox::No: source="No";break;
      default: break;
    }
    if(!source.isEmpty() && current(context)!=Language::English) button->setText(textFor(context,source));
  }
}
using Refresh = std::function<void(Language)>;
using Bindings = std::vector<Refresh>;
// The binding always retains the English source. It never translates user data
// and never reconstructs a widget. Call only for stable presentation properties.
template <typename Widget, typename Setter>
inline void bind(Bindings &out, Widget *widget, const QString &source, Setter setter) {
  if (!widget || source.isEmpty() || (menuText(source, Language::Korean) == source && menuText(source, Language::Japanese) == source))
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
// Observe the main window language property without changing data or ownership.
class LanguageObserver final : public QObject {
  QPointer<QObject> owner_;
  Bindings bindings_;
public:
  LanguageObserver(QWidget *window, QObject *owner, Bindings bindings)
      : QObject(window), owner_(owner), bindings_(std::move(bindings)) {
    if (owner_) owner_->installEventFilter(this);
    apply(current(window));
  }
  void apply(Language value) {
    for (const auto &refresh : bindings_) refresh(value);
  }
protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == owner_.data() && event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "sparamview.uiLanguage")
      apply(current(watched));
    return false;
  }
};
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
