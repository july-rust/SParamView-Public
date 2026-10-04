# SParamView v1.2.0

## English

### Purpose

v1.2.0 is a maintainability-focused structural release. The two oversized implementation files, `core.cpp` and `window.cpp`, are physically partitioned by responsibility while preserving the existing C++ Reference Engine behavior and public API.

### Structural changes

- `core.cpp` is reduced to a thin implementation aggregator and the existing implementation is partitioned into `core_common.cpp`, `touchstone.cpp`, `cache.cpp`, `mapping.cpp`, `termination.cpp`, `trace.cpp`, `tdr.cpp`, `analysis.cpp`, `jobs.cpp`, and `export.cpp`.
- `window.cpp` is reduced to a thin implementation aggregator and the existing GUI/controller implementation is partitioned into `window_ui.cpp`, `window_workspace.cpp`, `window_mapping.cpp`, `window_analysis.cpp`, `window_results.cpp`, `window_plot_controller.cpp`, `window_project.cpp`, `window_export.cpp`, and `window_validation.cpp`.
- The partitions remain in the same translation units in v1.2.0. This deliberately preserves internal/static linkage and minimizes behavior changes while making source navigation and debugging substantially easier.
- CMake exposes the partitions as header-only implementation sources so IDEs can display them without compiling duplicate translation units.
- The macOS CI portability shim follows the moved implementation sections.

### Numerical scope

No intentional numerical or electrical-analysis behavior changes are included. Touchstone parsing semantics, cache format, Mixed-mode processing, RL/IL/NEXT/FEXT equations, termination feedback equations, Quick TDR transforms, limit evaluation, PASS/FAIL/N/A logic, and exported numerical results are intended to remain equivalent to v1.1.7.

### Release gate

The release is published only after the existing regression suite, golden numerical tests, ASan/UBSan, clang analyzer checks, Windows x64 packaging/validation, Windows ARM64 cross-build plus native packaged-binary gate, and macOS Apple Silicon arm64 build/GUI/self-test/package validation pass.

### Packages

- `SParamView_Windows_x64_v1.2.0.zip`
- `SParamView_Windows_ARM64_v1.2.0.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.0.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.0.dmg`

> Quick TDR remains a Preview feature and should be correlated with calibrated measurement equipment or an independent reference tool when absolute physical accuracy is required.

## 한국어

### 목적

v1.2.0은 유지보수성과 디버깅 편의성을 높이기 위한 구조 개선 릴리스입니다. 과도하게 커진 `core.cpp`와 `window.cpp`를 책임별 파일로 물리적으로 분리하되, 기존 C++ Reference Engine의 동작과 공개 API는 유지합니다.

### 구조 변경

- `core.cpp`는 얇은 구현 집계 파일로 축소하고 기존 구현을 `core_common.cpp`, `touchstone.cpp`, `cache.cpp`, `mapping.cpp`, `termination.cpp`, `trace.cpp`, `tdr.cpp`, `analysis.cpp`, `jobs.cpp`, `export.cpp`로 분리합니다.
- `window.cpp`는 얇은 구현 집계 파일로 축소하고 GUI/컨트롤러 구현을 `window_ui.cpp`, `window_workspace.cpp`, `window_mapping.cpp`, `window_analysis.cpp`, `window_results.cpp`, `window_plot_controller.cpp`, `window_project.cpp`, `window_export.cpp`, `window_validation.cpp`로 분리합니다.
- v1.2.0에서는 분리된 파일을 기존과 동일한 번역 단위 안에서 포함합니다. 내부 `static` 링크와 계산 동작을 그대로 유지하면서 소스 탐색과 디버깅을 쉽게 하기 위한 저위험 구조입니다.
- CMake에는 분리 파일을 중복 컴파일하지 않는 구현 소스로 표시하여 IDE에서 구조를 쉽게 확인할 수 있도록 합니다.
- macOS CI의 portability shim도 이동된 구현 파일을 따라가도록 조정합니다.

### 수치 계산 영향

의도적인 수치/전기 분석 알고리즘 변경은 없습니다. Touchstone 파싱 의미, 캐시 포맷, Mixed-mode, RL/IL/NEXT/FEXT, termination feedback, Quick TDR 변환, Limit 및 PASS/FAIL/N/A 판정, 수치 결과 export는 v1.1.7과 동등하게 유지하는 것이 이번 릴리스의 기준입니다.

### 릴리즈 게이트

기존 회귀시험과 golden 수치 시험, ASan/UBSan, clang analyzer, Windows x64 패키징/검증, Windows ARM64 cross-build 및 native packaged-binary gate, macOS Apple Silicon arm64 build/GUI/self-test/package 검증이 모두 통과한 경우에만 릴리스를 게시합니다.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.
