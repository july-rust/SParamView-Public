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
  auto text=menuText(source.toString(),value);
  for(const auto &arg:label->property("sparamview.messageArgs").toStringList()) text=text.arg(arg);
  label->setText(text);
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
