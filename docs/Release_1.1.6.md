# SParamView v1.1.6

## English

### Key Changes

- Strengthened input validation and fail-safe handling while preserving the established numerical behavior of the C++ Reference Engine for RL/IL/NEXT/FEXT/Mixed-mode/TDR.
- Touchstone option lines now interpret valid ordering such as `R 50 RI` and `RI R 50` identically, while preserving Touchstone 1.1 per-port reference values. Duplicate `#` option lines are no longer silently ignored.
- TDR now safely rejects invalid Kaiser beta values and invalid Start/Stop time values including NaN, Inf, and out-of-range combinations. TDR analysis is also no longer affected by unrelated RL limit settings.
- Added fail-safe handling for unknown `overall()` states, null Job cache entries, invalid performance values, empty/all-invalid compare inputs, Mapping CSV integer overflow, and fingerprint/POSIX mmap I/O boundary conditions.
- Improved the default TDR graph view with **Review Fit**. The complete calculated dataset is preserved, while discontinuities and violation regions that are meaningful for review are shown more prominently in the initial view.
- **Fit All** explicitly restores the complete calculated time range, including termination extremes. A user-adjusted Zoom/Pan viewport is preserved during repaint and detail refresh for the same SNP/channel, and Review Fit is reinitialized only when switching to a new SNP/channel.
- Clearly identifiable synthetic singularities caused by Open/Short and other termination conditions no longer dominate Review Fit autoscaling, but they are not removed from the actual data or PASS/FAIL calculations.

### Validation

- All 25 regression tests passed in the pre-release Linux/Qt validation.
- The TDR improvements change only viewport/range and rendering behavior. They do not change the FFT, Kaiser window, DC extrapolation, nonuniform transform, interpolation, impedance conversion, or termination feedback equations.
- Added dedicated v1.1.6 regression coverage for parser, input-validation, and fail-safe behavior.
- Existing core/algorithm/mapping/polarity/termination/performance/Qt/navigation/workspace/hardening tests, sanitizer checks, and clang analyzer checks remain in place.
- The release pipeline validates Windows x64, Windows ARM64 with a native packaged-binary gate, and macOS Apple Silicon arm64 before publishing the three platform builds.

### Note

Quick TDR remains a Preview feature. This release improves viewing convenience and input safety, but it does not replace correlation against measurement equipment or a commercial reference tool.

---

## 한국어

### 주요 변경 사항

- C++ Reference Engine의 정상 RL/IL/NEXT/FEXT/Mixed-mode/TDR 수치 알고리즘은 유지하면서 입력 검증과 fail-safe 처리를 강화했습니다.
- Touchstone option line에서 `R 50 RI`와 `RI R 50`처럼 합법적인 옵션 순서가 모두 동일하게 해석되며, Touchstone 1.1 per-port reference 값도 유지합니다. 중복 `#` option line은 더 이상 조용히 무시하지 않습니다.
- TDR Kaiser beta 및 Start/Stop time의 NaN/Inf/범위 오류를 안전하게 거부하고, TDR 분석이 관계없는 RL limit 설정 때문에 실패하는 의존성을 제거했습니다.
- `overall()`의 알 수 없는 상태, null Job cache, 잘못된 performance 값, compare의 empty/all-invalid 입력, Mapping CSV integer overflow, fingerprint/POSIX mmap I/O 경계 조건을 fail-safe로 보강했습니다.
- TDR 그래프 기본 표시를 **Review Fit**으로 개선했습니다. 계산된 전체 데이터는 그대로 유지하면서 실제 검토에 의미 있는 불연속/위반 구간을 초기 화면에서 더 크게 보여줍니다.
- **Fit All**은 전체 계산 시간 범위와 termination 극단값까지 명시적으로 복원합니다. 사용자가 직접 Zoom/Pan한 viewport는 같은 SNP/채널의 repaint·detail refresh에서 유지되고, 새로운 SNP/채널로 전환할 때만 Review Fit으로 초기화됩니다.
- Open/Short 등 termination의 명확한 synthetic singularity는 Review Fit autoscale을 지배하지 않지만 실제 데이터와 PASS/FAIL 계산에서는 제거되지 않습니다.

### 검증

- 릴리즈 전 Linux/Qt 사전 검증에서 전체 25개 회귀 테스트가 모두 통과했습니다.
- TDR 개선은 표시 범위/렌더링 정책만 변경하며 FFT, Kaiser window, DC extrapolation, nonuniform transform, interpolation, impedance 변환 및 termination feedback 계산식은 변경하지 않습니다.
- v1.1.6 전용 parser/input/fail-safe 회귀 테스트를 추가했습니다.
- 기존 core/algorithm/mapping/polarity/termination/performance/Qt/navigation/workspace/hardening 테스트와 sanitizer/clang analyzer 검증을 그대로 유지합니다.
- 릴리즈 파이프라인에서 Windows x64, Windows ARM64(네이티브 packaged-binary gate), macOS Apple Silicon arm64를 검증한 뒤 세 플랫폼 산출물을 게시합니다.

### 주의

Quick TDR은 여전히 Preview 기능입니다. 이번 릴리즈는 표시 편의성과 입력 안전성 개선이며, 계측기 또는 상용 reference tool과의 correlation을 대체하지 않습니다.
