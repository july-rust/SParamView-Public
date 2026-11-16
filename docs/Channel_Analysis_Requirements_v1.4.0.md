# SParamView v1.4.0 — Channel Analysis 요구사항·인터페이스 계약 (012_01)

**상태:** REQUIREMENTS LOCKED (설계 기준). **구현:** 012_03 공통 Core·012_04 독립 창 틀·012_05 ILD·012_06 ICN·**012_07 Eye 수치 엔진 COMPLETE**; 012_08 Eye GUI COMPLETE (PR #47 및 병합 후 CI #43812686401 SUCCESS)·012_09 이후 COM/통합/릴리스는 미완료. **출시 기준:** immutable v1.3.5
**원본 계획:** [Development_Plan_012_v1.4.0.md](Development_Plan_012_v1.4.0.md) · **실제 기준선:** [Channel_Analysis_Baseline_012_01.md](Channel_Analysis_Baseline_012_01.md)

> 이 문서는 요구사항 계약이며 규격 적합성 선언이 아니다. ILD/ICN/Eye 수치 구현과 ILD/ICN GUI 자동 검증은 각각 완료됐지만, Eye GUI는 PR #47로 검증·병합되었고 병합 후 통합 CI #43812686401이 SUCCESS로 완료됐다(012_08 COMPLETE). COM 수치/GUI·실측 PHY 적합성·v1.4.0 배포는 아직 완료되지 않았다. [Eye 수치 검증](Channel_Analysis_Eye_012_07.md) 참조.

## 1. 기능/출력 계약

| 모듈 | 필수 입력 | 주요 출력(단위) | 범위 제한 |
| --- | --- | --- | --- |
| ILD | 확정 THRU, 방향, 양의 f 범위, fit 설정 | IL/fit/ILD(f) (dB), band RMS/peak (dB), 피팅 잔차·조건 | **Exploratory generic ILD**; 특정 규격 weighted FOM/PASS 별도 프로파일 없이는 금지 |
| ICN | 확정 victim·aggressor/물리 방향, source PSD/진폭, RX filter, 적분 대역, correlation 가정 | 기여 PSD (V²/Hz), 개별/총 RMS (V 또는 mV), 가정/대역 | 전압·정합·PSD 단위 조건 미확정 시 **mV 금지** |
| Eye | 복소 THRU S21/Sdd21, NRZ bit rate/bit pattern, TX level, sampling/conditioning | simulated waveform (V, s), eye height (V), width (s 또는 UI), 진단 | BER, 실측 eye, MIPI 규격 적합 판정, PAM4/IBIS-AMI 미포함 |
| COM | **고정 IEEE 802.3 profile**, THRU/NEXT/FEXT, package, TX/RX EQ, noise | COM (dB), profile-specific components/diagnostics/criterion | 공식 프로파일 잠금 전에는 **NOT EVALUATED**; 범용 3 dB 합격 금지 |

### 1.1 단위·부호·수치 정의

- 내부 주파수: **Hz**, 시간: **s**, 임피던스: **Ω**, 전압: **V**, one-sided PSD: **V²/Hz**, 로그 전달: **dB**. GUI에서 GHz, ps, mV 등의 축약 단위를 사용하더라도 원본 단위를 메타데이터에 기록한다.
- 기존 `logMagnitude(H)=20log10(|H|)`의 **signed transmission dB**와 generic ILD의 **positive loss** `L(f)=-20log10(|H(f)|)`를 구별한다. `ILD(f)=L(f)-Lfit(f)` (signed residual). `H=0`과 비유한 수치는 무한대 로그를 정상 finite sample로 포함하지 않는다.
- ILD 기본 탐색용 피팅 계약: 양의 주파수 `x=f/10^9 Hz`에서 `Lfit=a0+a1 sqrt(x)+a2 x`, 측정 주파수 구간의 bandwidth 기반 가중 최소제곱(주파수 밀집도가 결과를 좌우하지 않도록 trapezoid cell weight), QR/SVD 등 수치 안정법. 3개 이상 독립 sample, 유효 rank 및 finite 값 필수; 추정 구간 외 자동 외삽 금지. 다른 규격 피팅과 혼동되지 않도록 method/version, 계수, 범위를 출력.
- ILD `peak=max |ILD(f_i)|`은 **샘플 그리드 peak**; `RMS=sqrt(∫[f0,f1] ILD(f)^2 df/(f1-f0))`는 piecewise-linear 잔차/검증된 수치 적분 기반 **대역 RMS**. 주파수 그리드/수렴 경고와 계산방식을 결과에 기록. 이는 임의 표준의 weighted ILD와 같지 않다.
- ICN: matched reference 및 올바른 transfer normalization이 검증된 경우, **one-sided** source voltage PSD `P_i(f)`에 대해 비상관 가정하에 `sigma_i²=∫|H_i(f) F_rx(f)|² P_i(f) df`, `sigma_total=sqrt(sum_i sigma_i²)`. 이 계산 계약은 분석 모델의 예시이며 참조 임피던스·입력 정의가 다른 경우 power-wave S를 무조건 voltage ratio로 오용하면 안 된다. 상관된 소스는 cross-spectral matrix 검증이 필요하고, 구현 전이면 UNSUPPORTED. 누락된 진폭에 임의 숫자를 넣지 않는다.
- Eye: 복소 크기·**위상**을 보존하고 주파수·시간 샘플링, DC 추정/외삽, Hermitian symmetry/인과성, bandwidth/aliasing, waveform delay를 진단한다. TX bit sequence/초기 조건과 sample phase/threshold/eye width 정의를 기록. 매핑·입력 부족시 수치 출력 거부.
- COM: 계산 알고리즘·참조 프로파일·EQ 검색 공간·패키지·ISI/잡음/누화 모델·임계값은 **012_02에서 지정한 단일 edition/clause/version**에 엄격히 묶는다. 다른 IEEE 조항 또는 MIPI로 일반화하지 않는다.

## 2. 포트 방향 및 입력 유효성 계약

**현재 실코드 기반:** `si::Channel`은 `nearP,nearN,farP,farN`의 **0-based physical single-ended indices**를 갖는다. `si::Cache::trace(response,stimulus)`는 물리 포트 가중치를 받아 **복소** 값을 반환한다. `si::loadTrace()`가 현재 quick metric 방향을 정한다. 새 모듈은 동일한 **확정 매핑**과 포트 순서를 사용한다.

| 종류 | Forward (기준) | Reverse | 검증 포인트 |
| --- | --- | --- | --- |
| THRU | near 입력 → far 출력 (S21/Sdd21) | far 입력 → near 출력 (S12/Sdd12) | 분모/분자 방향과 reference impedance |
| NEXT | aggressor near 입력 → victim near 출력 | aggressor far 입력 → victim far 출력 | victim/aggressor 다름, 동일 grouping과 모드 일치 |
| FEXT | aggressor near 입력 → victim far 출력 | aggressor far 입력 → victim near 출력 | 개별 aggressor 소스/수신 포트 |
| Differential | P/N 극성·포트 가중치를 mapping으로 명시 | 포트 교체 시 극성 영향 기록 | 혼합모드 단정 금지, 교차모드 요청은 별도 지원 판단 |

**필수 Fail Closed:** 확인되지 않은 mapping; 누락/중복 포트; 단일 종단/차동 모드 불일치; stale SHA; 길이 불일치; 비단조/non-finite 주파수; 유효 source band 밖 계산; 적분 구간 0; TX 레벨 미정인데 절대 ICN/Eye 전압 요청; 지원 밖 COM profile. 비수동 또는 noncausal 입력은 경고/미평가 정책을 명시하고 결과 품질을 숨기지 않는다.

**Data provenance:** 각 출력/CSV/PNG는 source SHA, revision, channel IDs+port endpoints, forward/reverse, source reference impedance, original band/selected band, configuration, algorithm/version, assumptions, quality, UTC/KST 생성 시각을 복원 가능하게 기록한다. PNG는 그림 내 캡션 및 동일 basename의 메타데이터 sidecar 등 플랫폼 안전한 방법으로 provenance를 보존한다. CSV는 기존 spreadsheet-safe v3 원칙을 유지한다.

## 3. 공통 SourceSnapshot·결과 타입 (012_03 구현 계약)

기존 `si::Metric`/`si::Result`는 RL/IL/NEXT/FEXT/TDR용이며 **수정·오버로딩 금지**. 채널 분석은 분리된 API와 결과를 도입한다.

| 논리 모델 | 변경할 수 없는 필드/소유권 |
| --- | --- |
| `SourceSnapshot` | `shared_ptr<const Cache>`에 준하는 읽기 전용 세션 핸들; source SHA-256 및 verified flag; revision identity; immutable mapping 사본; monotonically increasing mapping/workspace generation; 물리 포트/방향; input reference impedance/geometry metadata; selected frequency span |
| `ChannelRequest` | snapshot ID, module type, channel/victim/aggressor selection, direction, typed settings, TX/RX inputs, algorithm/profile version |
| `ChannelResult<T>` | typed `ILDResult`/`ICNResult`/`EyeResult`/`COMResult`; quality/status, diagnostics, uncertainty, input/config digest, numerical arrays/metrics, attribution |
| `RunHandle` | per-window independent cancel, progress, generation token, bounded resource/callback ownership |

**상태 집합:** VALID (수치 가능, 규격 합격과 다름) / WARNING (한계 기록) / STALE / NOT_EVALUATED / UNSUPPORTED / INVALID_SOURCE / CANCELLED / ERROR. 별도의 `conformance PASS/FAIL`은 **프로파일이 규정한 공식 평가**로 입증된 경우에만 사용한다. 미평가 값을 0, NaN-to-zero, 또는 PASS로 표시하지 않는다.

**Revision/mapping 전환:** source import/reload, project replace, mapping edit/reconfirm, source fingerprint/SHA mismatch → generation 증가, 진행 작업 cancel, 기존 결과 STALE/hidden, 재사용 cache key 무효화. 워커가 늦게 완료되어도 GUI 스레드의 `generation + source SHA + mapping hash + configuration digest` 비교에서 탈락하면 결과 반영 금지.

**Cache key** = source SHA-256 + revision + exact physical port/polarity/direction + mapping generation + selected frequency range/resampling policy + module algorithm version + serialized settings + TX/RX/profile version. 각 창의 조작 상태와 mutable plot objects는 절대로 공유하지 않는다. 기초 캐시 파일의 수명은 모든 읽기 작업 완료까지 보장하며 닫을 때 기존 세션 정리 규칙을 유지한다.

## 4. C++ / Qt 구조·스케줄

| 계층 | 현재 존재 | 새로 도입 예정 |
| --- | --- | --- |
| Core | `include/si/core.hpp`, `src/trace.cpp`, `src/tdr.cpp` | `include/si/channel_analysis/`, `src/channel_analysis/common_*.cpp`, ILD/ICN/Eye/COM 각각 자체 cpp |
| GUI | `src/app.hpp`, `src/window.cpp`, `src/window_ui.cpp` | `src/gui/analysis/window_manager.cpp`, `ild_window.cpp`, `icn_window.cpp`, `eye_window.cpp`, `com_window.cpp` |
| Async | `Control`, `MainWindow::task`, `QtWorkerPool` | 창별 generation, cancel, owning worker completion, safe `QPointer` callback |
| Tests | `tests/golden_tests.cpp`, `qt_tests.cpp`, `v133_csv_tests.cpp` 등 | `tests/channel_analysis/` 및 `tests/fixtures/channel/` + 기능별 독립 oracle |

**구현상 주의:** `QtWorkerPool::run`은 작업 완료까지 동기 대기하므로 **GUI 메인 스레드에서 직접 실행하면 freeze 위험**이 있다. 비차단 제출/완료는 기존 `MainWindow::task` 패턴을 검토하여 안전한 큐/전용 worker thread로 분리. 계산 코드의 예외와 cancelled 상태를 구별해 GUI로 전달한다. `QPointer` + generation 검증으로 파괴된 창에 완료 callback을 적용하지 않는다.

**메뉴/창 계약:** 상단 `Analysis` 메뉴 → ILD / ICN / Eye Diagram / COM의 **별도 비모달 최상위 창**, 모두 동시 존재 가능. 같은 종류 메뉴 재클릭 시 기존 창을 raise/activate; 닫으면 해당 창만 해제; 메인 파일/결과/탭 기능과 격리. 각 창은 입력/진행/취소/결과/그래프/내보내기 소유. **실제 엔진 완성 전에는 메뉴 비활성화**, v1.4.0 정식 배포에서는 네 기능이 모두 사용 가능해야 한다.

## COM 012_02 인수 범위 정정 (2026-11-15)

**012_02 = COMPLETE / REFERENCE_READY**는 **공식 COM 소스와 승인 PHY 정체성, 독립 계산 알고리즘의 2조건 재현, QA 전용 라이선스 경로**가 확보되었음을 뜻한다. 참고 [012_02 프로파일](COM_Profile_Lock_012_02.md), [실제 계산](COM_Reference_Qualification_012_02.md), [실제 CI](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018).

**802.3ck-2022 C2C 표준 적합성, 규격의 Table 120F-8 전 수치와 Test1/Test2 패키지의 실제 COM 및 3건 이상 동규격 외부 oracle 비교는 012_09와 012_11의 절대 인수 조건**이다. 이번에 재현한 IEEE P802.3dj 채널 예제 결과는 **COM v4.17 알고리즘 교차검증**만 입증하며 802.3ck COM 합격/불합격을 뜻하지 않는다. 새 C++ 엔진 미구현 상태이므로 COM 메뉴의 규격 PASS 출력은 금지한다.

`manifest.json`의 `REFERENCE_READY`와 `LOCKED`는 서로 다르다. `reference-ready` 검사는 단계별 readiness 증거, `qualify` 검사는 후속 실제 규격의 전체 검증이며 후자는 현재 fail-closed 유지. 정식 v1.4.0 출시 게이트는 **전혀 완화하지 않는다.**

## 5. 단계별 인수 테스트 표

| 개발 단계 | 필수 인수 관찰 | 통과 불가 사례 |
| --- | --- | --- |
| 012_03 공통 core | immutable snapshot, port/direction/SHA, nonuniform frequency/DC/bandwidth 진단, cancellation·cache ID | 공통화로 TDR/RL/IL 값 변화 |
| 012_04 GUI 프레임 | 메뉴별 창 독립, 재호출 포커스, close/reopen, event loop 응답, project change STALE | worker 지연 callback 결과 오염 |
| 012_05 ILD | flat/ripple 독립 oracle, positive loss 부호/fit/peak/RMS 및 CSV/PNG 일치 | 규격 없는 PASS |
| 012_06 ICN | zero/one/multi-aggressor, source PSD amplitude scaling, receiver filter, polarity/direction | PSD 미정의 절대 mV |
| 012_07/08 Eye | known analytic impulse/step/NRZ, complex phase, causality/DC/bandwidth, open/reopen/Cancel | magnitude-only eye 또는 BER 추정 |
| 012_09/10 COM | 012_02에 고정한 공식 oracle 여러 fixture, EQ/ISI/noise 및 profile reject, GUI/export | 프로파일 미정인데 dB/PASS 출력 |
| 012_11 회귀 | 기존 v1.3.5 CTest, 프로젝트/CSV 캐시·오류 처리, 독립 numeric/ASan/UBSan | legacy 결과/호환성 회귀 |
| 012_12 플랫폼 | Windows x64 + **native** Windows ARM64 + macOS arm64 설치/패키지 네 창 실동작 | Linux offscreen 결과로 macOS/Windows GUI 판정 |
| 012_13/14 배포 | QA·라이선스·가이드 EN/KO·SHA-256/CI, 1 main PR·공식 release | 필수 게이트 미완 |

Golden fixture IDs·출처/현재 상태 및 오차 결정 절차는 [012_01 baseline](Channel_Analysis_Baseline_012_01.md)을 사용한다. **실제 수치 허용치는 fixture/oracle 데이터를 검증 후 잠금**한다. 012_02는 참조 준비 단계를 완결했으며 COM의 정식 IEEE 수치 프로파일·라이선스 배포 확인 및 C++ 엔진 적합성은 012_09/11/13 게이트에서 따로 완료해야 한다.

## 6. PR/통합·출시 계약

- 통합 브랜치: `integration/v1.4.0`; 기능 작업 PR은 여기로 squash-merge. 최종 보호 `main` PR **1건**, v1.4.0 배포 **1회**.
- 통합 브랜치에서 CI를 구동할 수 있도록 `.github/workflows/release-3platform.yml`의 branch trigger를 확장한다. **릴리스 발행 잡은 기존 `push && main` 가드를 유지**한다. 현재 v1.3.5 태그/릴리스 건드리지 않는다.
- Windows/macOS 실제 런타임, 공식 COM 참조 라이선스, 네 엔진 독립 수치 oracle가 모두 PASS 하기 전에는 v1.4.0을 태그/발행하지 않는다.
- 이 문서는 012_01 설계 계약 고정의 증거다. 위 기능들의 구현/테스트 완료를 의미하지 않는다.
