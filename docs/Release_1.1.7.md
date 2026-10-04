# SParamView v1.1.7

## English

### Purpose

v1.1.7 refines the default Quick TDR viewing window requested for practical SI inspection. The change is intentionally limited to the graph viewport and does not alter the established C++ Reference Engine numerical calculations.

### Changes

- Zero-based Quick TDR results initially display approximately **-0.1 ns to 1.5 ns**.
- The pre-zero margin keeps time zero away from the left plot border.
- The 1.5 ns initial right edge prevents the long low-information tail from dominating the first view.
- If the computed/configured TDR Start is materially nonzero, the view uses approximately **0.1 ns pre-roll** from that start and preserves the same 1.6 ns review width.
- **Fit All** restores the complete computed TDR time range.
- **Reset** and newly loaded TDR views restore the initial review window.
- Manual Zoom/Pan behavior remains available.
- Regression tests separately cover the zero-based default view, nonzero TDR Start, Fit All, and Reset behavior.

### Numerical scope

The viewport change does **not** change TDR numerical samples, FFT processing, Kaiser window calculation, DC extrapolation, nonuniform transform, electrical interpolation, impedance/reflection conversion, target/tolerance evaluation, PASS/FAIL/N/A logic, termination feedback equations, or RL/IL/NEXT/FEXT/Mixed-mode equations.

The parser/input-validation and fail-safe hardening introduced in v1.1.6 remains in place.

### Release gate

The release is published only after the three-platform workflow passes the common regression suite, ASan/UBSan, clang analyzer checks, Windows x64 packaging/validation, Windows ARM64 cross-build plus native packaged-binary gate, and macOS Apple Silicon arm64 build/GUI/self-test/package validation.

### Packages

- `SParamView_Windows_x64_v1.1.7.zip`
- `SParamView_Windows_ARM64_v1.1.7.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.1.7.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.1.7.dmg`

> Quick TDR remains a Preview feature and should be correlated with calibrated measurement equipment or an independent reference tool when absolute physical accuracy is required.

## 한국어

### 목적

v1.1.7은 실제 SI 검토 시 TDR의 의미 있는 초기 구간을 더 쉽게 확인하기 위한 **초기 그래프 표시 범위 개선 릴리스**입니다. 변경 범위는 viewport에 한정하며 기존 C++ Reference Engine의 수치 계산은 변경하지 않습니다.

### 변경 사항

- 일반적인 0 ns 기반 Quick TDR 초기 X축을 약 **-0.1 ns ~ 1.5 ns**로 표시합니다.
- 0 ns 왼쪽에 약 0.1 ns 여유를 두어 그래프가 프레임에 바로 붙지 않도록 합니다.
- 초기 오른쪽 범위를 약 1.5 ns로 제한해 의미 없는 긴 후반부가 첫 화면을 과도하게 차지하지 않도록 합니다.
- 계산/설정된 TDR Start가 명확하게 0이 아니면 해당 시작점보다 약 **0.1 ns 앞에서 시작**하고 동일한 1.6 ns 관찰 폭을 유지합니다.
- **Fit All**은 전체 계산 시간 범위를 복원합니다.
- **Reset** 및 새 TDR 화면은 초기 관찰 범위로 복귀합니다.
- 수동 Zoom/Pan은 그대로 유지합니다.
- zero-based 초기 화면, nonzero Start, Fit All, Reset 동작을 각각 회귀시험으로 고정합니다.

### 수치 계산 영향

이번 변경으로 TDR 수치 배열, FFT, Kaiser window, DC extrapolation, impedance/reflection 변환, Limit, PASS/FAIL/N/A, termination 계산식과 RL/IL/NEXT/FEXT/Mixed-mode 계산식은 변경하지 않습니다. v1.1.6의 parser/input validation 및 fail-safe 개선도 유지합니다.

### 릴리즈 게이트

공통 회귀시험, ASan/UBSan, clang analyzer, Windows x64 패키징/검증, Windows ARM64 cross-build와 native packaged-binary gate, macOS Apple Silicon arm64 build/GUI/self-test/package 검증이 모두 통과한 경우에만 v1.1.7 릴리스를 게시합니다.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.
