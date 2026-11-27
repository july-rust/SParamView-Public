# SParamView v1.4.1

Maintenance update to [v1.4.0](Release_1.4.0.md) for Windows x64, Windows ARM64 and macOS Apple Silicon.

## Changes

- **v1.4.1:** File > New closes ILD, ICN and Eye Diagram only after confirmation; Cancel or a failed save preserves the workspace. File > Exit uses the normal save/cleanup path. Window geometry is fitted to the active monitor's logical work area, with scrollable custom dialog bodies and fixed confirmation buttons. Qt file dialogs are used consistently across platforms.

- **v1.4.1:** 파일 > 새로 만들기는 저장 확인 후 ILD·ICN·Eye Diagram 창을 닫으며 취소 또는 저장 실패 시 기존 작업을 유지합니다. 파일 > 종료는 기존 저장·캐시 정리 절차를 사용합니다. 창을 현재 모니터의 논리 작업 영역에 맞추며 사용자 정의 대화상자는 본문 스크롤과 하단 고정 버튼을 사용합니다. 파일 선택은 공통 Qt 대화상자를 사용합니다.

- Signal Type Detection and Channel Mapping: wrap explanatory text, constrain long port-label controls, fit the native frame to the owner monitor and scroll content without moving confirmation buttons.
- TDR Options, Plot Axis Limits, Excel Export and Eye DC extrapolation confirmation use the common scroll-body/fixed-footer policy. A shared top-level QWidget geometry filter covers main/analysis windows and standard Qt dialogs, including file selection, warnings, input and save confirmations.
- Preserve explicit maximize/fullscreen/minimize behavior and current valid position on screen changes. Work-area fitting uses logical Qt coordinates and native frame margins, rather than physical panel pixels.
- No intended change to numerical analysis, project format, COM deferral, or existing v1.4.0 release assets.

## Qualification scope

Automated regression cases cover the workspace lifecycle, localized Exit, worker/cache retirement, import window bounds, long filenames, fixed footer visibility and synthetic offset/small logical work areas. Exact candidate validation is required by the existing three-platform CI and immutable publication workflow. This note does not assert physical LCD/DPI coverage or successful test execution before the corresponding CI evidence exists. Extremely small logical work areas may still be constrained by the platform/widget minimum dimensions. The Windows ARM64 physical reproduction reported by the user must be checked against the actual v1.4.1 build.

## Downloads

The release workflow publishes validated Windows x64/ARM64 user and optional QA ZIPs, macOS Apple Silicon ZIP/DMG, guides and SHA-256 checksums only after required validation. Existing v1.4.0 binaries remain immutable.

[English guide](USER_GUIDE_EN.md) · [한국어 사용자 설명서](USER_GUIDE_KO.md) · [Revision history](Revision_History.md)
