# SParamView v1.2.1

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

> **중요 안내**
> SParamView는 제공된 Touchstone 데이터를 계산하고 시각화하는 참고용 도구입니다. 입력 데이터의 상태, 주파수 범위, 포트/채널 매핑, 보간·근사 및 기타 분석 조건에 따라 표시된 그래프와 계산 결과가 실제 시스템 또는 계측 결과와 다를 수 있습니다. 프로그램의 결과를 100% 정확하거나 절대적인 값으로 간주하지 말고 설계·검토를 위한 참고 자료로 사용해 주십시오. 중요한 판단은 원본 데이터와 신뢰할 수 있는 계측 또는 별도의 분석 결과를 통해 다시 확인하시기 바랍니다.

Public source snapshot release. Includes the v1.1.6/v1.1.7 improvements and v1.2.0 source partitioning; platform packages are built and validated in this public repository.

## English

### Purpose

v1.2.1 is a focused correctness release for two P2 issues found after v1.2.0. It preserves the v1.2.0 source partitioning and existing C++ Reference Engine behavior outside the affected result-ranking and Quick TDR range-suitability paths.

### Fixes

- **Deterministic result ranking when margin is unavailable**
  - Margin sorting no longer switches comparison criteria on a pair-by-pair basis when finite and missing (`NaN`) margins are mixed.
  - Results with defined margins are ordered by margin first; missing-margin results form a separate class and use the existing severity ordering within that class.
  - This restores the strict weak ordering required by `std::stable_sort` and prevents cyclic comparisons such as `A < B < C < A`.

- **Quick TDR suitability is limited to the selected time range**
  - The uniform FFT TDR path no longer marks a selected interval `UNSUITABLE` because of a singular/out-of-range impedance sample that exists only outside the requested TDR time window.
  - Singularity/invalid-impedance detection is now performed while collecting the selected `tdrStartSeconds..tdrStopSeconds` samples.
  - A singularity inside the selected interval still produces the previous fail-safe `UNSUITABLE` behavior for reference termination, while terminated views keep the existing `LIMITED` behavior.

### Regression coverage

- Adds permutation coverage for mixed finite/missing-margin result ranking and verifies a single deterministic order for all input permutations.
- Adds a synthetic 10 GHz TDR fixture with a normal 0–1 ns region and a deliberate singularity at 5–6 ns. The full range remains `UNSUITABLE`, while the selected 0–1 ns range retains 21 finite samples near 50 Ω and remains suitable.
- The new v1.2.1 regression executable is included in the normal CTest release gate.

### Release gate

The release is published only after the existing regression suite, golden numerical tests, sanitizer/static-analysis gates, Windows x64 validation, Windows ARM64 native packaged-binary validation, and macOS Apple Silicon arm64 build/GUI/self-test/package validation pass.

### Packages

- `SParamView_Windows_x64_v1.2.1.zip`
- `SParamView_Windows_ARM64_v1.2.1.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.1.zip`
- `SParamView_macOS_AppleSilicon_arm64_v1.2.1.dmg`

> Quick TDR remains a Preview feature and should be correlated with calibrated measurement equipment or an independent reference tool when absolute physical accuracy is required.

## 한국어

### 목적

v1.2.1은 v1.2.0 이후 확인된 두 가지 P2 정확성 문제를 수정하는 집중 버그 수정 릴리스입니다. v1.2.0에서 정리한 소스 분할 구조는 그대로 유지하며, 영향을 받는 결과 정렬과 Quick TDR 선택 범위 판정 외의 기존 C++ Reference Engine 동작은 변경하지 않습니다.

### 수정 사항

- **Margin이 없는 결과가 섞인 경우의 결과 정렬 안정화**
  - Margin 정렬에서 유효 Margin과 `NaN` Margin이 섞였을 때 비교 대상 쌍마다 Margin/Severity 기준이 바뀌지 않도록 수정했습니다.
  - Margin이 있는 결과는 Margin 기준으로 먼저 정렬하고, Margin이 없는 결과는 별도 그룹으로 분리한 뒤 그 그룹 내부에서 기존 Severity 기준을 사용합니다.
  - 이에 따라 `std::stable_sort`가 요구하는 strict weak ordering을 만족하며 `A < B < C < A`와 같은 순환 비교 가능성을 제거했습니다.

- **Quick TDR 적합성 판정을 선택 시간 범위로 제한**
  - Uniform FFT TDR 경로에서 사용자가 선택한 범위 밖의 singular/out-of-range 임피던스가 선택 구간까지 `UNSUITABLE / N/A`로 만드는 문제를 수정했습니다.
  - 이제 singular/invalid impedance 판정은 실제 `tdrStartSeconds..tdrStopSeconds` 범위에 포함되는 샘플을 수집할 때만 수행합니다.
  - 선택 범위 내부에 실제 singularity가 존재하는 경우에는 기존 fail-safe 동작을 유지합니다. Reference termination은 `UNSUITABLE`, termination 적용 뷰는 기존과 동일하게 `LIMITED`로 처리합니다.

### 회귀 시험

- 유효 Margin/없는 Margin이 혼합된 결과를 모든 입력 순열로 정렬하여 항상 동일한 결과가 나오는지 검증하는 시험을 추가했습니다.
- 10 GHz 대역의 합성 TDR 입력에서 0–1 ns는 정상 50 Ω 구간이고 5–6 ns에 의도적인 singularity가 존재하도록 구성했습니다. 전체 범위는 계속 `UNSUITABLE`이어야 하며, 0–1 ns 선택 범위는 약 50 Ω의 유효 샘플 21개를 유지하면서 정상 판정되는지 확인합니다.
- 신규 v1.2.1 회귀 시험을 일반 CTest 릴리즈 게이트에 포함했습니다.

### 릴리즈 게이트

기존 회귀시험, golden 수치 시험, sanitizer/static-analysis, Windows x64 검증, Windows ARM64 native packaged-binary 검증, macOS Apple Silicon arm64 build/GUI/self-test/package 검증이 모두 통과한 경우에만 릴리스를 게시합니다.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.
