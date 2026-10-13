# SParamView v1.2.2

## English

### Purpose

v1.2.2 is a focused UI and macOS packaging release. It keeps the analysis algorithms and the compact v1.2.1 workspace geometry unchanged while improving the clarity of the top analysis controls and restoring a proper macOS application-bundle icon before launch.

### UI changes

- The TDR gear-only control is presented as **Option** so its purpose is readable without interpreting an icon.
- RL, IL, NEXT, FEXT, and TDR controls are visually grouped in compact bordered sections.
- The grouping is applied without increasing the analysis-settings panel height; the normal compact panel remains capped at 74 px so the graph area is not reduced.
- The TDR **Option** control remains inside the TDR group and opens the existing reference-impedance / termination / reflection settings.

### macOS icon packaging

- The packaged `SParamView.app` now contains `Contents/Resources/SParamView.icns` generated from the existing SParamView icon asset during the macOS release build.
- `Info.plist` now declares `CFBundleIconFile` as `SParamView.icns`, allowing Finder to resolve the application icon from the bundle before SParamView is launched.
- The release job verifies that the ICNS file exists and that the bundle metadata points to it before ad-hoc signing and packaging.

### Regression coverage

- The existing header/UI regression test now checks that the compact settings panel retains its 74 px maximum height.
- It verifies five grouped analysis sections and the TDR **Option** label, and rejects the legacy gear-only label.
- Existing core, algorithm, parser, mapping, TDR, direction, termination, performance, workspace, navigation, and platform validation remain in the release gate.

### Analysis scope

No RL, IL, NEXT, FEXT, TDR numerical algorithm, PASS/FAIL rule, channel mapping rule, termination equation, or result-ranking behavior is intentionally changed in v1.2.2.

### Packages

- `SParamView_Windows_x64_v1.2.2.zip`
- `SParamView_Windows_ARM64_v1.2.2.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.2.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.2.dmg`

> Quick TDR remains a Preview feature and should be correlated with calibrated measurement equipment or an independent reference tool when absolute physical accuracy is required.

## 한국어

### 목적

v1.2.2는 UI와 macOS 패키징에 집중한 릴리스입니다. 분석 알고리즘과 v1.2.1의 컴팩트 작업영역 구조는 유지하면서, 상단 분석 설정의 구분을 명확하게 하고 macOS에서 실행 전에도 앱 아이콘을 정상적으로 인식할 수 있도록 번들 구성을 보강했습니다.

### UI 변경

- TDR의 톱니바퀴 전용 버튼을 **Option** 텍스트 버튼으로 변경해 기능을 바로 알 수 있도록 했습니다.
- RL, IL, NEXT, FEXT, TDR 설정을 각각 컴팩트한 박스로 구분했습니다.
- 일반 분석 설정 패널의 최대 높이 74 px는 그대로 유지하여 그래프 영역이 줄어들지 않도록 했습니다.
- TDR **Option** 버튼은 TDR 박스 내부에 유지되며 기존의 기준 임피던스 / 터미네이션 / 반사계수 설정 기능을 그대로 호출합니다.

### macOS 아이콘 패키징

- macOS 릴리스 빌드 중 기존 SParamView 아이콘 자산으로 `Contents/Resources/SParamView.icns`를 생성해 `SParamView.app`에 포함합니다.
- `Info.plist`의 `CFBundleIconFile`을 `SParamView.icns`로 지정해 앱을 실행하기 전에도 Finder가 번들 아이콘을 확인할 수 있도록 수정했습니다.
- ad-hoc 서명과 ZIP/DMG 패키징 전에 ICNS 파일 존재 여부와 `Info.plist` 연결 상태를 자동 검증합니다.

### 회귀 시험

- 기존 Header/UI 자동 시험에 분석 설정 패널 최대 높이 74 px 유지 여부를 추가했습니다.
- RL/IL/NEXT/FEXT/TDR 5개 구분 박스와 TDR **Option** 표시를 확인하고, 기존 톱니바퀴 전용 표시가 남아 있으면 실패하도록 했습니다.
- 기존 core, 알고리즘, parser, mapping, TDR, direction, termination, performance, workspace, navigation 및 플랫폼별 검증은 그대로 릴리스 게이트에 포함됩니다.

### 분석 기능 영향

v1.2.2에서는 RL, IL, NEXT, FEXT, TDR 수치 알고리즘, PASS/FAIL 규칙, 채널 매핑 규칙, 터미네이션 계산식, 결과 정렬 동작을 의도적으로 변경하지 않습니다.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.

