# 012_01 — v1.3.5 기준선·검증 증거

**기준일:** 2026-11-15 (KST) · **범위:** 설계 계약/기준선 고정 (새 분석 엔진 구현 없음)
**판정 원칙:** 기존 CI 결과의 **재확인**과 012_01 커밋의 **신규 실행**을 구별한다.

## 1. 변경 불가 기준선

| 항목 | 고정 값/증거 |
| --- | --- |
| Repository | `july-rust/SParamView-Public` |
| 공식 태그 | [`v1.3.5`](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.5) |
| 태그 SHA | `8eb449aa2259bdf007a3a709ea719b9740540f2a` |
| 012_01 분기 시 main HEAD | `b802ee55236164069154013fbf7deab7555763c0` |
| 태그→main | 3 commits ahead, 0 behind; README와 채널 분석 계획 문서만 변경 ([diff](https://github.com/july-rust/SParamView-Public/compare/v1.3.5...b802ee55236164069154013fbf7deab7555763c0)) |
| 프로젝트/C++ API 버전 | `CMakeLists.txt`의 1.3.5 / `include/si/core.hpp`의 1.3.5 |
| 빌드 | CMake ≥3.24, C++20, Qt6 ≥6.4 (CI Qt 6.8.3); `SI_BUILD_GUI=OFF` 지원 |
| 릴리스 정책 | v1.3.5 태그·아티팩트 불변, 012_14까지 정식 v1.4.0 미배포 |

**기존 릴리스 CI 증거:** [GitHub Actions run 43678704012](https://github.com/july-rust/SParamView-Public/actions/runs/43678704012), head SHA `8eb449aa...`, `2026-11-15T04:53:28Z`, conclusion `success`.

| 기존 CI 작업 (동일 태그 SHA) | API 확인 상태 | 해석 |
| --- | --- | --- |
| Version/release consistency | SUCCESS | 문서·버전 정합 |
| Core quality gates | SUCCESS | Release core CTest, Clang ASan/UBSan CTest, clang analyzer 단계 성공 |
| Windows x64 | SUCCESS | CI Windows x64 빌드/검증 |
| Windows ARM64 | SUCCESS | Windows ARM64 cross-build 및 패키지 |
| Native Windows ARM64 packaged-binary gate | SUCCESS | 별도 네이티브 ARM64 검증 잡 통과 |
| macOS Apple Silicon arm64 | SUCCESS | macOS 빌드/GUI 통합/패키지 확인 |
| Publish three-platform release | SUCCESS | v1.3.5 정식 배포 잡 성공 |

**제한:** 위 표는 **기존 v1.3.5 태그에서 이미 실행된 CI의 기록**이다. 012_01 신규 코드 검증 또는 수동 물리 PC GUI 조작의 증거로 대체하지 않는다. 새 브랜치 CI는 해당 워크플로 실행 URL과 결론으로 별도 기록한다.

**현 실행 환경:** GitHub 연결을 통한 코드/CI/브랜치 조회 및 문서 변경은 가능했다. 실행 컨테이너에는 Qt6 개발 라이브러리가 없으며 GitHub 네트워크 이름 해석이 실패하므로 저장소 전체 checkout과 **로컬 CTest 재실행은 UNVERIFIED**. 이 제한을 없었던 것처럼 PASS로 표시하지 않는다.

## 2. 실제 코드 조사와 호환성 경계

| 현재 코드 | 확인된 계약/후속 영향 |
| --- | --- |
| [`include/si/core.hpp`](../include/si/core.hpp) | `si::Cache::trace(response,stimulus)`는 **0-based physical port** 가중치를 사용. `Metadata.sha256`, `Channel.nearP/nearN/farP/farN`, `Control.cancelled` 활용 가능 |
| [`src/trace.cpp`](../src/trace.cpp) | `loadTrace`의 IL은 near→far에서 S21/Sdd21, 역방향에서 S12/Sdd12; NEXT/FEXT 방향과 SE/차동 호환 검사 존재 |
| [`src/tdr.cpp`](../src/tdr.cpp) | FFT/주파수 외삽 및 TDR 캐시가 있음. 새로운 공통 FFT로 대체하기 전 독립 회귀 필요 |
| [`src/app.hpp`](../src/app.hpp) | `Revision`은 `shared_ptr<Cache>`, 포트 매핑 및 confirmed 상태를 보유. `MainWindow`에 비동기 task/view 및 cache-session 소유권 존재 |
| [`src/window_analysis.cpp`](../src/window_analysis.cpp) | `requestedJobs()`에서 모든 revision에 Mapping confirmation 및 `validateMapping` 필요 |
| [`src/workers.cpp`](../src/workers.cpp) | `QtWorkerPool::run`은 submitted 작업의 완료를 기다리는 **blocking 호출**이므로 GUI 스레드 직접 호출 금지 |
| [`src/window.cpp`](../src/window.cpp) | `window_*.cpp`를 포함하는 단일 메인 컴파일 단위. Qt 신규 창은 별도 번역 단위/타깃과 헤더를 이용하여 기존 구조 파괴 최소화 |
| [`.github/workflows/release-3platform.yml`](../.github/workflows/release-3platform.yml) | 원본은 PR/push `main`에 한정. 통합 브랜치 검증 트리거 필요. `release` 잡은 `push && main`에서만 발행 가능 |

**반드시 보존:** RL/IL/NEXT/FEXT/TDR 결과 및 비교 방향, Touchstone 1.x/2.x, `.siproject`, Mapping CSV v1/v2/v3 로드, CSV spreadsheet-safe v3 저장, 프로젝트/리비전 교체, session-cache/다중 프로세스 격리/시작 오류 복구, Windows 최상위 EXE 배치.

## 3. 검증 기준선 및 새 테스트 계획

기존 회귀 타깃 예시는 `golden_touchstone_numerics`, `bidirectional_direction_delta`, `algorithm_boundary_regressions`, `v133_csv_roundtrip_security`, `multi_process_cache_isolation`, `session_cache_lifecycle`, `qt_workers_and_labels`이다. 실제 CTest 전체 목록은 빌드별 `ctest -N`으로 수집한다.

```sh
# 기존 CI 정의와 동일한 Linux core / sanitizer 확인 절차
cmake -S . -B build-core -DSI_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
cmake -S . -B build-san -DSI_BUILD_GUI=OFF -DSI_SANITIZE=ON -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-san --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-san --output-on-failure
```

**012_01 신규 계약 테스트:** 입력 포트/방향/단위, SourceSnapshot 불변성·SHA/세대 일치, 프로젝트 교체 중 worker 완료 시 STALE, Cancel/창 종료, CSV/PNG 출처 기록, 제한된 메모리/시간; 실제 구현과 검증은 012_03~012_12에서 진행. 위 명령은 **재현 지침**이며 이 문서 작성으로 실행됐다는 의미가 아니다.

### Golden fixture 예비 등록 및 출처

| ID | 대상 | 구성/기대 성질 | 생성·출처/라이선스 | 상태 |
| --- | --- | --- | --- | --- |
| LEGACY-GOLD-S2P | RL/IL 방향 | `tests/golden_tests.cpp`가 생성하는 1/2/3 GHz 2-port 표준 fixture; S21=0.5, S12=0.4 | 저장소 자체 생성 테스트 | 기존 CI 재확인 |
| CH-ILD-FLAT | ILD | 일정 복소 THRU 이득, 잔차 거의 0 | 신규 결정론적 합성; 자체 생성 예정 | NOT GENERATED |
| CH-ILD-RIPPLE | ILD | 설정된 손실·리플, signed residual 및 band RMS | 신규 결정론적 합성; 자체 생성 예정 | NOT GENERATED |
| CH-ICN-ZERO | ICN | victim, crosstalk=0 → RMS 0 | 신규 합성 S-matrix; 자체 생성 예정 | NOT GENERATED |
| CH-ICN-MULTI | ICN | 1/2 aggressor, known 1-sided PSD, coherent/uncorrelated 구분 | 신규 합성 데이터 및 독립 적분; 자체 생성 예정 | NOT GENERATED |
| CH-EYE-IDEAL | Eye | 복소 H=1, NRZ pattern + 정의된 시간격자 | 자체 생성 및 독립 시간영역 오라클 예정 | NOT GENERATED |
| CH-EYE-BAND | Eye | 1차 저역통과/지연 채널의 위상과 Eye 변화 | 분석적 임펄스 응답 기준 예정 | NOT GENERATED |
| CH-INVALID | 전체 | DC 누락/비등간격/포트 오류/주파수 역전/비유한 입력 | 자체 생성 예정 | NOT GENERATED |
| CH-COM-IEEE | COM | 특정 IEEE 802.3 edition/clause/profile/fixture | **012_02에서 공식 출처, revision, SHA-256, 이용/재배포 조건 확정** | BLOCKED: 012_02 |

**해시 원칙:** 아직 만들어지지 않은 fixture에는 SHA-256을 기입하지 않는다. 실제 fixture 생성 후 `sha256sum`/플랫폼 동등 수단으로 입력 SHA-256과 oracle 버전·라이선스를 동결한다. 과거 Git blob SHA-1과 입력 데이터 SHA-256은 다르다.

## 4. 허용 오차 결정 절차 (수치값 사전 날조 금지)

1. **단위 및 정의 고정:** dB 손실 부호, Hz·s·V·V²/Hz, one-sided PSD, complex phase convention.
2. **독립 oracle:** 생산 코드의 수식/FFT/피팅 코드를 그대로 재호출하지 않는 해석적 해·독립 계산 라이브러리·공식 COM 레퍼런스로 확인.
3. **오차 종류 분리:** 절대/상대 오차, dB 오차, 시간 보간/주파수 이산화, peak/grid 민감도, floating-point rounding.
4. **수렴 시험:** 독립적으로 선택한 `df`/sample rate/bit pattern/FFT 길이/피팅 구간을 변경해 불확도 추정. 실패 조건 및 안전한 범위 명시.
5. **고정 승인:** 실제 값과 수렴 결과를 기록한 뒤 테스트별 허용치를 결정하고 테스트 코드·입력 SHA·oracle 버전을 묶어서 잠금. 모든 메트릭에 임의로 동일한 ±1% 적용 금지.

## 5. 012_01 종료/인계

- **요구사항 및 타입/GUI/SourceSnapshot 계약:** [Channel_Analysis_Requirements_v1.4.0.md](Channel_Analysis_Requirements_v1.4.0.md)
- **012_02 인계:** 공식 COM 조항/edition, ref 소스 커밋, 재현 fixture, reference output, 알고리즘·종속성 라이선스/재배포 가능성부터 잠금. 불충족이면 COM 미구현·미평가로 유지.
- **릴리스 게이트:** 네 모듈+독립 oracle+세 대상 OS GUI/패키지+구형 회귀+문서+SHA, 하나라도 미충족이면 v1.4.0 릴리스 금지.
