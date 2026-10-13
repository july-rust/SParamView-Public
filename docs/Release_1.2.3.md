# SParamView v1.2.3

## English

### Purpose

v1.2.3 improves differential-signal detection before Channel Mapping. It extends safe P/N-name recognition to regular names where the P/N character appears anywhere in the net name, adds an explicit Signal Type Detection review step for non-authoritative cases, and prevents explicit differential metadata from silently degrading to single-ended interpretation when polarity or physical linkage cannot be validated.

### P/N name recognition

A name-derived P/N pair is accepted only when both names:

1. have the same length after trimming,
2. are identical case-insensitively except at exactly one character, and
3. that single differing character is P versus N.

The P/N position is not restricted to an underscore-delimited token. Examples covered by regression tests include:

- `DATA_P / DATA_N`
- `DATAP0 / DATAN0`
- `CLKP1 / CLKN1`
- `DPP0 / DPN0`
- `PDATA0 / NDATA0`
- `PCLK1 / NCLK1`
- `PDP0 / NDP0`

Names with multiple differences, incomplete pairs, duplicate/ambiguous pairing, or no remaining channel identity are not guessed as differential.

### Signal Type Detection

Before Channel Mapping, non-authoritative detections are classified as:

- **Differential**
- **Single-ended**
- **Mixed**
- **Unknown**

The review shows the detected type, evidence and confidence.

- **High confidence**: explicit supported `.DiffChannels` or Touchstone `[Mixed-Mode Order]` metadata, or direction-free unambiguous reflection ports. Explicit metadata remains authoritative for the signal type; physical P/N and Near/Far may still require manual review.
- **Medium confidence**: regular/repeated names provide structural evidence without an explicit differential declaration. The user confirms the signal type before Channel Mapping.
- **Low / Unknown**: insufficient evidence. The user must choose the signal type before Channel Mapping.

Selecting **Single-ended** suppresses name-based differential inference for that revision and rebuilds the mapping candidates conservatively.

### Metadata priority and fail-safe behavior

- Validated `.DiffChannels` remains the highest-priority source for physical differential endpoints.
- Touchstone `[Mixed-Mode Order]` remains explicit differential evidence, but it does not by itself establish Near/Far direction.
- If explicit differential metadata is present but P/N polarity or physical linkage cannot be validated, SParamView keeps the signal type as differential and requires manual P/N/Near/Far review.
- In that situation, compatibility label mapping is **not** silently accepted as single-ended.
- Valid S-parameter data remains loaded even when automatic mapping cannot be confirmed.

### Project persistence

Signal type, confidence, evidence and review state are stored as optional fields in the existing schema-1 `.siproject` structure. Existing projects remain readable. If the source Touchstone hash changes, signal type and Channel Mapping review are required again.

### Regression coverage

The existing mapping and polarity suites now cover the P/N-at-any-position rule, the single-ended override path, and negative cases with multiple differences. A GUI regression test also opens a regular `DATAP0/DATAN0` fixture, confirms the Signal Type Detection dialog selects Differential, and verifies the resulting physical P/N and Near/Far candidate. The explicit-metadata fallback GUI test now verifies that invalid differential metadata does not auto-populate a single-ended compatibility mapping. Existing core, parser, mixed-mode, RL/IL/NEXT/FEXT, TDR, termination, result-ranking, project, workspace, navigation and platform validation gates remain in place.

### Numerical scope

v1.2.3 does **not** intentionally change RL, IL, NEXT, FEXT, mixed-mode conversion, Quick TDR numerical transforms, termination equations, limit evaluation or PASS/FAIL/N/A rules. The change is limited to signal-type interpretation, mapping workflow, persistence metadata and related UI.

### Packages

- `SParamView_Windows_x64_v1.2.3.zip`
- `SParamView_Windows_ARM64_v1.2.3.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.3.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.3.dmg`

> Quick TDR remains a Preview feature and should be correlated with calibrated measurement equipment or an independent reference tool when absolute physical accuracy is required.

## 한국어

### 목적

v1.2.3은 Channel Mapping 전에 수행하는 **차동 신호 자동 인식의 정확성과 안전성**을 개선한 릴리스입니다. P/N 문자가 Net 이름의 어느 위치에 있어도 규칙적인 쌍이면 인식할 수 있도록 확장하고, 명시적 메타데이터가 없는 경우에는 **Signal Type Detection** 단계에서 사용자가 Differential / Single-ended / Mixed 여부를 확인하도록 했습니다.

### P/N 이름 인식 규칙

이름 기반 P/N 쌍은 다음 조건을 모두 만족할 때만 인정합니다.

1. 두 이름의 길이가 같습니다.
2. 대소문자를 무시하면 정확히 한 글자만 다릅니다.
3. 그 한 글자가 정확히 P ↔ N 조합입니다.

따라서 P/N 위치는 문자열 앞·중간·뒤 어디든 가능합니다. 회귀시험에는 다음 패턴을 포함합니다.

- `DATA_P / DATA_N`
- `DATAP0 / DATAN0`
- `CLKP1 / CLKN1`
- `DPP0 / DPN0`
- `PDATA0 / NDATA0`
- `PCLK1 / NCLK1`
- `PDP0 / NDP0`

두 글자 이상이 다르거나, 쌍이 불완전하거나, 중복·모호한 조합이거나, P/N을 제외한 채널 식별자가 남지 않는 경우에는 차동으로 추측하지 않습니다.

### Signal Type Detection

Channel Mapping 전에 비권위적 추론 결과를 다음 네 가지로 분류합니다.

- **Differential**
- **Single-ended**
- **Mixed**
- **Unknown**

확인 화면에는 판정 결과, 근거, Confidence가 표시됩니다.

- **High**: 지원되는 `.DiffChannels`, Touchstone `[Mixed-Mode Order]` 같은 명시적 메타데이터 또는 방향 추론이 필요 없는 명확한 reflection port.
- **Medium**: 명시적 메타데이터 없이 규칙적인 P/N 이름 또는 반복되는 endpoint 이름에서 구조적 근거가 확인된 경우. Channel Mapping 전에 사용자 확인을 받습니다.
- **Low / Unknown**: 근거가 부족한 경우. 사용자가 Signal Type을 선택해야 Channel Mapping으로 진행합니다.

사용자가 **Single-ended**를 선택하면 해당 Revision에서는 이름 기반 차동 추론을 끄고 보수적으로 Single-ended 후보를 다시 구성합니다.

### 메타데이터 우선순위와 Fail-safe

- 검증된 `.DiffChannels`는 물리 차동 endpoint의 최우선 근거입니다.
- Touchstone `[Mixed-Mode Order]`는 명시적 차동 근거이지만 Near/Far 방향까지 증명하지는 않습니다.
- 명시적 차동 메타데이터가 있는데 P/N polarity 또는 물리 연결을 검증하지 못한 경우에도 Signal Type을 Single-ended로 낮추지 않습니다.
- 이 경우 compatibility label mapping을 Single-ended 확정값으로 조용히 채택하지 않고, P/N 및 Near/Far의 수동 확인을 요구합니다.
- 자동 Mapping을 확정하지 못해도 유효한 S-parameter 데이터 자체는 계속 로드합니다.

### 프로젝트 저장

Signal Type, Confidence, Evidence, 확인 상태를 기존 schema-1 `.siproject`에 선택 필드로 저장합니다. 기존 프로젝트 호환성은 유지합니다. 원본 Touchstone의 해시가 저장 당시와 달라지면 Signal Type과 Channel Mapping을 다시 확인하도록 합니다.

### 회귀 시험

기존 Mapping/Polarity 시험에 P/N 위치 무관 규칙, Single-ended 강제 선택 경로, 두 글자 이상 차이가 나는 부정 사례를 추가했습니다. GUI 회귀시험에는 `DATAP0/DATAN0` 합성 파일을 실제로 열어 Signal Type Detection에서 Differential이 제안되는지와 이후 P/N·Near/Far 후보가 정확한지 확인하는 경로를 추가했습니다. 기존 explicit metadata fallback GUI 시험도 잘못된 차동 메타데이터에서 Single-ended compatibility 후보가 자동 채택되지 않는지 확인하도록 변경했습니다. 기존 core/parser/mixed-mode/RL/IL/NEXT/FEXT/TDR/termination/result-ranking/project/workspace/navigation 및 플랫폼 검증 게이트는 그대로 유지합니다.

### 수치 계산 영향

v1.2.3에서는 RL, IL, NEXT, FEXT, Mixed-mode 변환, Quick TDR 수치 변환, termination 계산, Limit 및 PASS/FAIL/N/A 판정 알고리즘을 의도적으로 변경하지 않습니다. 변경 범위는 Signal Type 해석, Mapping workflow, 프로젝트 메타데이터 및 관련 UI입니다.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.

