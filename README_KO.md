# SParamView 1.1.4

## 1.1.4: 요청 범위 완전성과 판정 일관성 강화

검증된 RL/IL/NEXT/FEXT, 차동 변환, 종단, Quick TDR 핵심 수식은 유지하고 판정 계층을 보강했습니다. 사용자가 유한한 주파수/시간 범위를 명시했는데 실제 데이터 또는 계산 창이 그 범위를 끝까지 덮지 못하면, 확인된 구간에서 NG가 발견된 경우는 NG를 유지하고 그렇지 않으면 전체 판정을 N/A로 표시하며 누락 범위를 Note에 남깁니다. 기본 `start=0`은 DC 실측 요구가 아니라 첫 가용 주파수부터의 자동 범위로 유지합니다. 동일한 직선 한계선을 불필요한 중간 점들로 표현해도 평가 격자와 판정이 바뀌지 않으며, 수동 마커의 비원본 주파수 값은 복소 보간에 의한 표시값이고 pass/fail을 바꾸지 않는다는 설명을 추가했습니다.

## 1.1.3: Revision별 분석 상태 격리 및 회귀 방지

Quick Analysis 이후 다른 SNP/Revision으로 전환해 RL/IL/NEXT/FEXT/TDR을 개별 선택할 때 이전 Revision 결과가 표시될 수 있던 상태 관리 문제를 수정했습니다. 결과표와 그래프는 현재 Revision 결과만 표시하며, 개별 분석의 기존 결과 재사용도 현재 Revision과 마지막 분석 설정이 모두 일치할 때만 허용합니다. Channel Mapping이 변경되면 해당 Revision의 기존 결과와 Job을 무효화합니다. 같은 사용자 조작 순서를 Workspace 자동 회귀시험에 추가했습니다.

## 1.1.0: 매핑 확인 절차와 UI 개선

`.DiffChannels` 메타데이터는 포트 인덱스와 실제 라벨 검증을 통과하면 자동 차동 매핑으로 사용합니다. Touchstone Mixed-Mode도 선언된 P/N 페어와 채널 연결이 모두 검증될 때만 자동 확정합니다. 명시적 차동 선언 없이 `_P/_N` 같은 이름만 발견된 경우에는 **차동 후보**로만 표시하며 사용자가 Channel Mapping에서 P/N과 Near/Far를 확인하기 전에는 분석에 사용하지 않습니다. 차동 근거가 없는 채널은 Single-ended로 처리합니다.

Channel Mapping에는 물리 포트/라벨 기반 수동 Differential 생성, P/N 교환, Near/Far 교환을 추가했습니다. 중복 포트와 불완전 P/N은 저장 시 거부합니다. Quick Analysis와 분석 실행 버튼은 RL/IL/NEXT/FEXT/TDR 축약형으로 정리하여 그래프 영역을 더 확보했습니다.

Advanced를 Maximize Results 오른쪽으로 이동하고 기본 UI를 영문 기술용어로 변경했습니다.
현재 검증 범위와 실행 방법은 `docs/Release_1.1.4.md`, 영문 안내는 `README_EN.md`를 확인하세요.
1.1.4 릴리스 파이프라인은 Windows x64 native 검증, ARM64 cross-build/PE audit 및 패키지의 native Windows ARM64 자동 회귀시험을 모두 통과해야 공개 릴리스를 생성합니다.

# SParamView — Windows x64

버전: **1.0.3 / UI 여백 개선 업데이트**
개발 기준: 사용자가 제공한 「SI Analyzer 요구사항 명세서 v1.0 Release」. 원문은 `docs/Requirements_v1.0.md`에 포함되어 있습니다.

Touchstone 파일에서 Return Loss, Insertion Loss, NEXT, FEXT, Quick TDR을 확인하는 C++20 / Qt 6 데스크톱 프로그램입니다. 데이터를 로컬에서 처리하며 원본 Touchstone은 읽기 전용으로 사용합니다.

## 가장 빠르게 실행하기

1. `SParamView_Windows_x64_v1.0.3.zip`을 **폴더 전체** 압축 해제합니다.
2. 폴더 안의 **SParamView.exe**를 더블클릭합니다. DLL과 `platforms`, `fonts` 폴더는 함께 두세요.
3. 첫 확인은 **샘플 열기 → QUICK ANALYSIS**를 누르면 됩니다.
4. 실제 분석은 **Touchstone 열기 → Channel Mapping 확인 → Channel 체크 → QUICK ANALYSIS** 순서입니다.
5. 결과 행을 선택하면 해당 분석의 그래프를 볼 수 있습니다. **Export XLSX**로 저장합니다.

배포 폴더에는 Qt 및 MinGW 실행 라이브러리가 포함됩니다. 사용자 PC에 Python, Qt 개발 도구 또는 Excel을 설치할 필요가 없습니다. 대상 운영체제는 Windows 11 x64입니다. 이 배포본은 코드 서명이 없는 개발용 실행파일이며, 실제 Windows 11에서의 실행·호환성 수용 시험은 미완료입니다. 프로그램 표시명과 실행 파일은 `SParamView`로 변경되었고, 창·작업표시줄·탐색기 아이콘에는 S-parameter 그래프 아이콘이 적용되었습니다.

## 1.0.3: 경계와 도구 모음 여백 개선

채널 목록과 그래프 사이에 양쪽 여백을 두고, 그래프 도구 모음·결과표 상단·Revision 비교 탭의 안쪽 여백을 정리했습니다. QUICK ANALYSIS 버튼은 채널 목록 하단에 고정되어 작은 화면에서도 접근할 수 있습니다. 낮은 그래프에서는 보조 정보 한 줄을 접고 Y축 눈금 수를 조절합니다. 작은 화면에서 분석 설정은 내부 스크롤로 확인합니다.

## 1.0.2에서 추가된 그래프와 결과표 확대

|기능|사용 방법|
|---|---|
|분할 보기|Ctrl+1, 그래프·표 경계 드래그로 높이 조절|
|그래프 크게|Ctrl+2, 채널·설정·표를 접고 그래프 확대|
|결과표 크게|Ctrl+3, 결과표를 작업 영역 전체로 확대|
|전체 화면|F11 전환, Esc 해제|
|표 글자 크기|12/14/16/18/20 px, 행 높이·열 너비도 조정|
|설정 표시|분석 설정 버튼, 작은 화면에서는 내부 스크롤|
|배치 복원|분할 비율·글자 크기·패널 표시 저장, 보기 메뉴에서 초기화|

선택 행과 현재 그래프 축 범위는 보기 전환 후에도 유지됩니다. 그래프 종류 선택창은 이미 계산한 결과를 보여 주며, 새 조건의 분석은 QUICK ANALYSIS를 실행합니다. 표에서 결과를 선택하면 그래프 종류 표시도 함께 갱신됩니다. 결과표 확대에서 Revision 비교를 실행하면 비교 탭이 보이도록 분할 보기로 돌아갑니다.

OK는 PASS, NG는 FAIL에 해당하며 기존 계산식과 판정 기준을 유지합니다. 이번 검증의 정확한 수량 및 Windows/원본 확보 제한은 `docs/Release_1.0.3.md`에 기록했습니다.

## 이전 1.0.1: 자동 매핑과 파일 열기 수정

P/N 토큰이 중간에 있는 `CLK_P_OUT_0_MP`, `DPHY_CLK_P_MP_I` 형태를 지원합니다. 나머지 이름·포트 인덱스·라벨도 검증하며, 기존 접미사와 Mapping ID를 유지합니다.

자동 매핑이 모호해도 유효한 데이터 파일은 열고 빈 매핑과 경고를 표시합니다. 수동 입력이나 CSV 가져오기 후 확인해야 분석할 수 있습니다. Label 후보 오류가 수동 입력을 지우거나 창을 종료시키지 않습니다.

계산식·캐시·프로젝트 형식·TDR 품질 기준은 RC1과 동일합니다. 이번 배포의 실제 검증 범위는 `docs/Release_1.0.3.md`를 확인하세요. 실제 Windows PC/DPI/드라이버 확인과 계측기 상관 검증은 별도입니다.

## 이전 1.0.0 RC1: 가혹 시험 및 오류 수정

다음은 RC1 당시 기록입니다. 당시 Windows 실기 실행은 검증하지 못했으며, 세부 수치는 [RC1 릴리즈 검토 보고서](docs/SIAnalyzer_Release_Review_v1.0.0_RC1.md)를 확인하세요.

v0.1.5에서 재현한 캐시 세대 혼동, 빈 TDR 입력 종료, CR 단독 줄바꿈 처리, 잘못된 2-port 잡음 인식, 잘린 캐시 읽기, 손상된 주파수 축, 프로젝트 설정의 잘못된 형 변환을 수정했습니다. 정상 TDR의 계산식과 품질 기준은 바꾸지 않았습니다. 334개 실제 조건은 이전 버전과 수치/품질이 비트 단위로 일치했습니다.

캐시 형식은 5로 갱신했습니다. **업데이트 후 기존 파일을 처음 열 때 한 번 다시 가져오며 시간이 걸릴 수 있습니다.** 이후에는 검증된 캐시를 재사용합니다. 한 주파수의 물리 행렬은 최대 512 MiB로 제한하며, 원본 크기로도 표현할 수 없는 포트 선언은 할당 전에 거절합니다. 이는 프로그램 전체 메모리 상한이 아닙니다.

포함된 `Run_Windows_Verification.cmd`는 추가 Touchstone 업로드 없이 Windows에서 코어·매핑·TDR·화면·저장 자동 검증을 실행하고 결과를 TEMP 폴더에 남깁니다. 원본 데이터와 기존 프로젝트는 수정하지 않습니다. 실제 화면/DPI와 측정기 상관 검증은 별도입니다. 아래의 이전 버전 보고서는 당시 기록으로 보존했습니다.

## v0.1.5: 대량 TDR 및 그래프 탐색 개선

TDR 부하 계산과 시간 파형을 재사용하고 비균일 적분의 삼각함수 평가를 줄였습니다. 줌은 UI를 비활성화하지 않는 전용 작업에서 실행하며, 연속 조작 시 이전 작업을 취소하고 마지막 요청만 반영합니다. 십자선 이동 시 전체 그래프를 다시 그리지 않습니다.

많은 결과는 Min/Max 미리보기를 유지하고 현재 범위의 상세 표시만 갱신합니다. 동시 Overlay는 선택 행에서도 최대 64개이며 전체 결과는 표에 그대로 남습니다. 화면 선분 단순화는 최대 0.2 논리 픽셀 오차로 제한하며 분석 수치, Worst/Limit 판정, Raw 데이터와 보고서 이미지에는 적용하지 않습니다.

부하 Trace 캐시는 64 MiB, TDR 파형 캐시는 128 MiB의 보관 예산을 사용합니다. 이는 프로그램 전체 RAM 제한이 아니며, 서로 다른 대량 데이터의 첫 계산이 모두 즉시 완료됨을 보장하지 않습니다. 자세한 성능 비교와 실제 파일 검증은 [v0.1.5 검증 보고서](docs/SIAnalyzer_Validation_v0.1.5.md)를 확인하세요. 아래 이전 버전 문서는 변경 이력으로 남겨 두었습니다.

## v0.1.4에서 추가된 TDR 종단 비교

**TDR 종단 선택…**에서 SE 50 Ω / Diff P–N 100 Ω, GND 기준 50 Ω 두 개, Open, Short, 원본 Reference 정합을 여러 개 선택할 수 있습니다. Forward는 Far, Reverse는 Near에 부하를 적용합니다. 다른 모든 물리 포트는 원본 기준 임피던스로 정합하며 각 Channel은 독립적으로 계산합니다.

**반사계수 ρ 표시**를 선택하고 TDR을 다시 실행하면 Open/Short의 반사 방향을 쉽게 비교할 수 있습니다. ρ에는 Ω 허용오차 판정을 적용하지 않습니다. 원시 CSV에는 Ω와 ρ를 함께 저장합니다. 결과 표와 Excel은 종단 이름/단위를 구별합니다.

새 설정의 기본 종단은 SE 50 Ω / Diff P–N 100 Ω이며, TDR 구간은 0–3 ns입니다. 기존 프로젝트에 종단 설정이 없으면 Reference를 유지합니다. Target Ω는 판정 기준이며 부하 저항을 변경하지 않습니다. 반대쪽 Mapping이 없는 Channel에서는 Reference 반사만 사용할 수 있습니다.

상세 기능, 독립 계산 대조와 검증 한계는 [종단 검증 보고서](docs/Termination_Validation_v0.1.4.md)를 확인하세요.

## 기본 사용법

### Channel Mapping은 처음 한 번 확인합니다

- 포트 번호는 **1부터 시작**합니다. 소스코드/프로젝트 JSON 내부의 포트 번호만 0부터 시작합니다.
- Single-ended: Near+와 Far+를 지정하고 Near−/Far−는 비웁니다.
- Differential: Near+/Near−/Far+/Far− 네 포트를 지정합니다.
- Far를 비우면 반사 및 Reference TDR 전용 Channel로 사용할 수 있습니다. 다른 종단 조건은 반대쪽 Mapping이 필요합니다. IL/FEXT 등 필요한 끝점이 없는 분석은 N/A입니다.
- `.DiffChannels` 메타데이터와 SE Net가 섞이면 사용되지 않은 포트의 일치 Net 쌍도 SE 후보로 포함하며 Differential/Single-ended Group을 나눕니다. 이 방식으로 ETC의 TEST_IN, TEST_OUT, INPUTX2 후보 누락을 보완했습니다.
- `.DiffChannels` 메타데이터 주석이 있으면 명시된 포트 인덱스와 Net 라벨을 함께 검증하여 차동 후보를 만듭니다. P/N 기재 순서가 반대인 Revision도 P/N으로 정렬합니다.
- `DIE.pin.NET`/`BGA.pin.NET`, 또는 `component-pin NET` 라벨에서 같은 Net의 두 끝점을 찾으면 SE 후보를 제안합니다.
- 포트 라벨에서 Near/Far 및 P/N 후보를 제안합니다. `DATA0_P`, `DATA0_N`만 있으면 Near 쌍 후보를 제시하며 Far는 사용자가 지정합니다.
- **앞/뒤 절반 SE 후보**는 사용자가 명시적으로 누르는 수동 보조 기능입니다. 포트 배치가 실제 연결과 일치하는지 확인해야 합니다.
- CSV는 `Channel,NearP,NearN,FarP,FarN,Group,Alias,ID` 형식을 사용합니다. 처음 다섯 열은 필수입니다. 처음 제시된 다섯 열만 있는 기존 매핑도 읽습니다.
- 같은 물리 포트를 여러 Channel에 중복 지정하면 오류를 표시합니다.
- Mapping ID / 이름 / Alias 순서로 Revision을 대응합니다. CSV 또는 프로젝트로 Mapping을 재사용하세요.

### 다섯 가지 분석과 판정

| 화면 이름 | 실제 값 | Worst | Limit 만족 조건 |
|---|---|---|---|
| Return Loss | S11/S22 또는 Sdd11/Sdd22의 dB | 최댓값 | 값 ≤ Limit |
| Insertion Loss | S21/S12 또는 Sdd21/Sdd12의 dB | 최솟값 | 값 ≥ Limit |
| NEXT | Aggressor 진행 방향의 시작점 → Victim 같은 쪽 | 최댓값 | 값 ≤ Limit |
| FEXT | Aggressor 진행 방향의 시작점 → Victim 반대쪽 | 최댓값 | 값 ≤ Limit |
| TDR | 시간에 따른 임피던스 | Target에서 가장 먼 값 | Target ± Tolerance 이내 |

**그래프 제목에는 S11 [dB]/Sdd11 [dB], S21 [dB]/Sdd21 [dB] 등 실제 parameter를 표시합니다.** SE/Differential을 함께 표시하면 두 명칭을 함께 기재하며, 역방향에서는 S22/Sdd22와 S12/Sdd12를 사용합니다.

**RL/IL 화면은 양수 손실량이 아니라 S-parameter Log Magnitude dB를 표시합니다.** 예: S11 = −15 dB, S21 = −2 dB.

Limit 체크박스는 기본적으로 꺼져 있으며 **임의 규격을 적용하지 않습니다**. Limit이 없으면 N/A입니다. 샘플 프로젝트에 저장된 −10/−3/−30 dB와 100Ω ±10%는 사용 예시이며 MIPI 공식 규격이 아닙니다.

Margin이 음수이면 NG, 양수이면 여유가 있습니다. 주파수에 따라 변하는 Limit의 경우 파형 Worst 위치와 최소 Margin 위치는 다를 수 있습니다. 결과 툴팁과 Excel에 두 위치를 구분해 기록합니다. 주파수 Limit이 분석 범위 전체를 덮지 않으면 판정은 N/A입니다.

### 그래프와 Marker

- 분석 범위는 위쪽 **Frequency [GHz] Start/Stop**으로 지정합니다. Stop = Auto max는 데이터 끝까지입니다.
- 휠은 커서 위치를 중심으로 X축 확대/축소, Ctrl+휠은 Y축, Shift+휠은 X/Y축을 함께 확대/축소합니다.
- 왼쪽 드래그로 이동합니다. Shift+드래그 또는 **영역 확대** 버튼을 켠 뒤 드래그하면 사각 영역을 확대합니다.
- **+/−**, **이전**, **축 범위**, **전체 보기** 버튼을 제공합니다. 우클릭 메뉴에서도 접근할 수 있습니다.
- 그래프에 초점을 둔 상태에서 Home=전체 보기, Backspace=이전 보기, 좌우 화살표=이동, +/−=확대/축소입니다. TDR 더블클릭은 확대이고, 주파수 그래프 더블클릭은 기존 Marker 추가입니다.
- **축 범위**에서 X 시작/끝 및 Y 최소/최대를 숫자로 지정할 수 있습니다. TDR 단위는 ns, 주파수는 GHz입니다. Y 자동 범위도 선택할 수 있습니다.
- 커서 십자선과 좌표/가장 가까운 표시 곡선 값을 보여 줍니다. 이 값은 표시 배열을 보간한 탐색값입니다. 주파수에서 정밀한 값은 Marker를 사용하세요.
- TDR 전체 보기에서는 분석 시작점 앞에 표시 구간의 1/4만큼 여백을 두어 시작점이 그래프 내부 폭의 20%에 오도록 합니다. 기본 0–3 ns 분석은 −0.75–3 ns로 표시합니다. 음의 시간 영역에 임의 파형을 생성하지 않습니다.
- 탐색은 표시 범위만 바꾸며 분석 범위와 판정은 바뀌지 않습니다. 전체 계산 샘플이 캐시된 작은 그래프는 즉시 탐색하고, 축약된 대형 그래프는 원본에서 다시 읽어 표시를 갱신합니다.
- Global Worst를 우선 검출하고 기본 Top 3 Marker를 표시합니다. Worst only / Top 3 / Top 5 / Off를 선택할 수 있습니다.
- 좁은 Peak/Notch가 사라지지 않도록 표시 버킷의 Min/Max를 보존합니다.
- Advanced의 Marker 칸에 `1, 3, 6, 9`처럼 GHz 단위로 입력하거나 그래프를 더블클릭할 수 있습니다. 샘플 사이에서는 복소수를 보간한 뒤 dB/위상을 계산합니다.
- 여러 Trace에서 Marker 글자가 겹치지 않도록 Overlay가 많을 때는 점과 Legend의 Worst 값을 중심으로 표시합니다.
- Heatmap의 Y축은 Channel 행이므로 탐색은 X축에 적용합니다. 커서는 해당 행·셀의 Worst 값 또는 Worst Margin을 표시합니다.
- Heatmap은 평균이 아닌 버킷의 Worst를 표시합니다. Margin 모드에서 빨간색은 NG이며 회색은 판정값이 없는 구간입니다.
- 기본 Overlay는 현재 분석의 상위 64개 결과를 표시합니다. Ranking 표에서 원하는 행을 선택할 수 있으며 선택 Overlay도 최대 64개입니다. 다른 결과는 선택을 바꾸어 확인하세요.

### NEXT / FEXT

선택한 Victim에 대해 **같은 Group의 나머지 Channel**을 Aggressor로 분석합니다. Advanced에서 특정 Aggressor를 고를 수 있습니다. Near/Far 방향은 Mapping 기준이며 Reverse도 지원합니다. Single-ended와 Differential 사이의 모드 변환 Crosstalk은 Quick NEXT/FEXT 대상에서 제외하여 N/A로 표시합니다.

정방향 NEXT = Aggressor Near → Victim Near, FEXT = Aggressor Near → Victim Far입니다. 역방향 NEXT = Aggressor Far → Victim Far, FEXT = Aggressor Far → Victim Near입니다. RL/TDR은 Near → Near 또는 Far → Far 반사입니다.

### Quick TDR

- Target Auto는 Single-ended 50Ω / Differential 100Ω입니다. 숫자를 입력하면 Custom Target이 됩니다.
- **Target은 판정 기준**입니다. 임피던스 변환에 쓰는 기준 임피던스는 Touchstone에서 읽습니다. 예를 들어 기준 50Ω 데이터를 Target 100Ω으로 바꿔도 파형이 두 배로 바뀌지 않습니다.
- 주파수 Stop은 변환 상한입니다. 저역통과 변환을 위해 Frequency Start와 무관하게 원본의 최저 주파수부터 사용합니다. 분석할 시간 범위는 Advanced의 TDR time [ns]로 지정합니다.
- 기본 알고리즘: 복소수 선형 재샘플링 → 실수 DC 추정 → Kaiser β=6 → Hermitian 스펙트럼 → IFFT → 누적 사다리꼴 Step → Zref·(1+ρ)/(1−ρ).
- 최고 주파수 바깥의 고주파 성분을 임의로 만들어 넣지 않습니다. 시간축은 Δt=1/(N·Δf)입니다.
- TDR 그래프 상단에 GOOD / LIMITED / UNSUITABLE 개수를 표시합니다. 품질은 회로의 OK/NG와 별개입니다.
- GOOD/LIMITED/UNSUITABLE은 입력 데이터와 변환 조건의 품질 표시입니다. DC를 추정하거나 비균일 그리드를 재샘플링하면 LIMITED가 될 수 있습니다.
- 최저 주파수가 최고 주파수의 10%를 넘거나 입력이 매우 적은 경우, 변환 용량 제한/특이 임피던스가 있는 경우에는 UNSUITABLE 또는 N/A입니다. 자세한 원인은 툴팁과 Excel Configuration에 있습니다.
- 균일 그리드 재구성 오차가 복소 S 기준 0.05를 넘으면, v0.1.3부터 원본의 비균일 주파수 구간을 직접 적분하는 Low-pass Step으로 전환합니다. 급격한 저주파 응답을 굵은 FFT 그리드로 옮기면서 잃는 문제를 줄이기 위한 경로입니다.
- 직접 적분은 DC의 실수부를 짝함수로 외삽하고 Kaiser window를 적용합니다. 원본 샘플 사이의 복소수 선형 보간을 사용하며, Gauss–Legendre 8/4점 적분 간 ρ 오차 ≤1e−5와 형상 보존 3차 보간 대비 ρ 민감도 ≤0.001을 검사합니다. 이 조건은 수치 보호 장치이며 측정 정확도나 SI 합격 기준이 아닙니다.
- 직접 적분 경로는 항상 **LIMITED**입니다. DC 불안정, 적분 미수렴, 보간 민감도 초과, 계산량 한도 또는 특이 임피던스는 계속 UNSUITABLE로 표시합니다. UNSUITABLE의 회로 판정은 N/A입니다.
- 직접 적분은 원본 구간을 사용하므로 단일 Δf가 없고 보고서에서 N/A입니다. 시간 간격은 1/(2·Fmax), 출력은 최대 16,385개 시간 샘플 및 8천만 노드×시간 평가 범위로 제한합니다. 긴 시간 범위가 거절되면 TDR Stop을 줄이세요.
- 제공한 260907/260908 파일의 기본 0–3 ns 조건에서 OUTPUT_CLK, OUTPUT_0_1_2 및 ETC 차동 TDR을 생성합니다. 검증 조건과 오차는 `docs/Upgrade_Validation_v0.1.3.md`를 확인하세요.
- 이 Preset은 합성 회로 및 독립 수치 라이브러리와 검증했지만, **원본 SI 도구 또는 계측기 결과와의 V1 수용 검증은 아직 완료되지 않았습니다**.

### Revision과 저장

- 여러 파일을 열면 Revision이 추가됩니다. 각 Revision의 Mapping을 확인하세요.
- 왼쪽 Revision은 Channel 선택/비교의 기준 Revision입니다. 분석은 등록된 모든 Revision의 대응 Channel에 수행됩니다.
- **Revision 비교**는 공통 주파수 범위와 공통 그리드에서 복소수를 보간합니다. 서로 다른 기준 임피던스는 임의로 재정규화하지 않고 오류를 표시합니다.
- Raw Δ = Candidate dB − Baseline dB입니다. Improvement는 IL에서 Raw Δ, RL/NEXT/FEXT에서 −Raw Δ입니다. Worst 간 차이와 주파수별 최소 개선량을 별도로 표시합니다.
- **파일 → 프로젝트 저장**은 Mapping, Group, Alias, Frequency Range, Limits, Marker, Quick 선택, TDR, UI 상태와 원본 위치를 `.siproject`로 저장합니다.
- 원본 파일의 SHA-256이 저장 시점과 달라지면 Mapping을 다시 확인하도록 표시합니다. 원본이 이동했으면 열 때 새 위치를 지정할 수 있습니다.
- **Limit → Preset 저장/불러오기**로 다른 프로젝트에서도 판정 기준을 재사용할 수 있습니다.

### Excel / PNG / CSV

- XLSX: Summary, Return_Loss, Insertion_Loss, NEXT, FEXT, TDR, Graphs, Configuration. 분석하지 않은 항목은 생략합니다.
- Graphs에는 현재 그래프와 분석별 그래프를 고해상도 PNG로 삽입합니다. 많은 결과는 8개 Trace 단위로 나눕니다.
- Raw Trace Data는 기본적으로 제외합니다. 선택하면 Raw_1 등으로 저장하며 시트가 커지면 분할합니다.
- 큰 Raw Data는 **결과 행 하나 선택 → 파일 → 선택 결과 Raw CSV 저장**을 권장합니다.
- XLSX는 프로그램이 직접 OOXML/ZIP으로 만듭니다. Excel COM을 사용하지 않습니다.
- XLSX는 ZIP32 한도(약 4GiB)를 초과하면 오류를 표시합니다. 이 정도로 큰 Raw Data는 별도 CSV로 저장하세요.
- 도구의 SHA-256 검증, 분석, 변환, 비교, 리포트 작업은 백그라운드에서 실행되며 하단 **취소** 버튼으로 중단합니다.

## 대용량 처리 구조

첫 Import에서 Complex128 Binary Cache를 생성합니다. 버전/크기/수정 시각/부분 지문으로 유효성을 검사하고 같은 원본이면 재사용합니다. 초기 Import 중에만 원본 전체 SHA-256을 계산하며, 사용자가 수동 검증할 수도 있습니다.

캐시는 약 8MiB를 목표로 하는 타일 안에 Trace 순서로 저장합니다. 조회할 때 필요한 Trace 구간만 메모리 매핑합니다. 원본 전체 행렬×주파수 데이터를 Heap에 유지하지 않습니다. 큰 Ranking에서는 축약 미리보기를 보관하고 상세 화면은 캐시 또는 원본에서 갱신합니다. 원본 한 주파수의 행렬조차 매우 큰 경우 그 한 프레임만큼의 메모리는 필요합니다.

기본 Cache 위치는 Windows 사용자 로컬 AppData의 SIAnalyzer Cache 디렉터리입니다. 생성 중 `.sicache.tmp`를 사용하고 정상 완료 후 확정합니다. 프로젝트를 이동할 때 Cache를 복사할 필요는 없습니다.

## 화면 대비

Windows 호스트의 어두운 팔레트가 일부 위젯에 섞이지 않도록 밝은 Fusion 테마에서 기본·선택·비활성 상태의 전경/배경을 함께 지정합니다. 메뉴, Revision 드롭다운, Advanced 버튼, 목록과 툴팁에 적용합니다.

## 실제 파일 시험에서 수정한 내용

- Windows CRLF 줄바꿈으로 누락되던 포트 라벨을 인식합니다.
- `Port Impedance` 주석을 무조건 거부하지 않고, 모든 지정 값이 고정 실수 Reference와 일치할 때 읽습니다. 실제 임피던스가 다르거나 복소수/불완전한 주석이면 계속 오류를 표시합니다.
- 원본 SI 도구 차동 Mapping 주석을 활용하며 Cache 형식은 4로 갱신했습니다. 이전 Cache는 원본에서 재생성됩니다.
- 자동 TDR 그리드가 원본 응답을 크게 바꾸는 경우 잘못된 임피던스 표시를 막습니다.
- 이전 사용자 파일 7개를 전체 행렬 기준으로 검사했습니다. 시험 조건·결과·한계는 `docs/Real_Data_Validation.md`에 있습니다.

## 이번 배포의 검증 범위

이번 수정의 시험 측정값과 남아 있는 수용 조건은 **docs/SIAnalyzer_Validation_v0.1.5.md**에 기록했습니다. Windows x64 실행파일을 교차 빌드하고 DLL 심볼 의존성을 점검했습니다. Linux는 개발 검증 환경으로만 사용했으며 지원 제품으로 배포하지 않습니다. 이전 버전 보고서의 수치는 당시 조건에 대한 기록입니다.

확인되지 않은 항목: 실제 Windows 11 실행/응답성/Working Set, Microsoft Excel에서 직접 열기, 원본 SI 도구 또는 VNA TDR 비교, Windows의 30 FPS 및 5GB/10GB 성능 수용 시험. 따라서 버전은 V1.0 정식 릴리즈 대신 Engineering Preview로 표시했습니다.

## 개발자가 다시 빌드하기

권장: Visual Studio 2022 C++ Desktop Build Tools, Qt 6.8.3 MSVC x64 kit, CMake 3.24 이상.

```powershell
.\scripts\build_windows.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64 -Compiler MSVC
```

MinGW kit에서는 해당 컴파일러와 Ninja를 PATH에 넣고 `-Compiler MinGW`를 지정합니다. 결과는 `dist\SParamView-Windows-x64`입니다. `.github/workflows/windows.yml`은 Windows CI 빌드·검증용으로 포함했으며 이번 환경에서 GitHub 실행을 수행한 것은 아닙니다.

독립 엔진 테스트는 `ctest --test-dir build --output-on-failure`입니다. `tests/cross_validate.py`는 scikit-rf/NumPy/SciPy를 이용한 교차 검사입니다. `scripts/stress_test.py`는 실제 대용량 합성 Touchstone을 만들므로 충분한 여유 디스크가 필요합니다. 이 도구들은 최종 사용자 실행에는 필요하지 않습니다.

## 출처 및 라이선스

- 개발 기준: 제공된 [요구사항 명세서](docs/Requirements_v1.0.md).
- 포맷 정의: [IBIS Touchstone 2.1 공식 규격](https://ibis.org/touchstone_ver2.1/touchstone_ver2_1.pdf).
- 시간 영역의 해석과 Window 원칙: [Keysight Time Domain](https://helpfiles.keysight.com/csg/pxivna/Time/TimeDomain.htm).
- 독립 변환 구현: [scikit-rf step_response](https://scikit-rf.readthedocs.io/en/latest/api/generated/skrf.network.Network.step_response.html).
- Windows 배포 방식: [Qt Windows Deployment](https://doc.qt.io/qt-6/windows-deployment.html).

SI Analyzer 자체 소스는 MIT 라이선스입니다. Qt는 동적 연결되며 DLL 교체를 제한하지 않습니다. Qt 라이선스와 고지는 `third_party`에 있고, 대응 Qt 6.8.3 소스 아카이브는 함께 제공되는 **SParamView_Source_v1.0.3.zip**의 `third_party`에 포함됩니다. Qt/MinGW/폰트의 개별 라이선스는 해당 고지를 따릅니다.

## v0.1.2 작업 실행 구조

GUI에서는 전용 QThreadPool 1개가 파일 처리·비교·보고서 작업을 조정하고, 별도 QThreadPool이 분석 worker를 재사용합니다. 분석 동시 실행 수는 Low 1개, Normal 최대 2개, Maximum 최대 8개이며 논리 CPU 수와 작업 개수로도 제한합니다. 대기 중인 조정 스레드와 UI 스레드는 이 분석 worker 개수와 별개입니다. CPU 점유율의 특정 퍼센트를 보장하는 설정은 아닙니다. Low는 작업 사이에 2 ms 간격도 둡니다. 취소는 실행 중인 작업이 안전한 체크 지점에서 종료하는 방식입니다.

헤드리스 CLI/core는 Qt 의존성 없이 C++ 스레드를 사용할 수 있습니다. GUI 분석 경로에서는 QtWorkerPool을 주입합니다. `si_mapping_tests`는 방향·P/N·dB 판정을 검사하며 `si_qt_tests`는 worker 재사용, 동시 실행 수, 예외/취소, GUI 이벤트 처리, 그래프 표기를 검사합니다. Windows 빌드 스크립트에서도 CTest를 실행합니다.
