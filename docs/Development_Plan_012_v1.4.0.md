# SParamView v1.4.0 — 012 시리즈 전체 개발·검증·릴리스 계획

> **2026-11-17 번호 체계 변경 — `012_10` 재계획 우선:** 아래의 `012_10~012_14` 세부 내용은 2026-11-16 이전 **기존 계획의 역사적 기록**이다. 사용자가 새 `012_10`을 계획 수립 전용으로 확정했으며, 새 `012_11`은 **IEEE 802.3ck-2022 Annex 120F 규격 검증**이다. 해당 작업의 [최종 원문 감사](COM_8023ck_Normative_Audit_012_11.md)는 **012_11 100% COMPLETE** (2026-11-17, 정식 IEEE Xplore 발행 원본 PDF SHA-256 검증, 120F-8 코드 수치 68/68 MATCH, Test1/2 및 관련 93A 규칙 확인)이다. 아래 기존 `012_11` 회귀 테스트 계획과는 번호가 다르다. **COM 수치 엔진·독립 Golden·정식 PASS/FAIL은 여전히 미완료이므로 별도 후속 단계에서만 진행하며, 원본 규격값 확인 완료를 COM 합격으로 해석하지 않는다.**

> **2026-11-17 새 번호 기준 최신 상태 — 012_13 참조 수치 검증:** 기존 2026-11-16 이전 번호 체계의 012_13(릴리스 준비)과 무관하게, 새 [012_13 IEEE 802.3ck-2022 COM Reference Numerical Qualification](COM_120F8_Reference_Numerical_012_13.md)은 **정식 Table 120F-8의 68개 검증 수치를 적용한 실제 3개 IEEE 역사적 채널 × Test1/Test2 = 6개 COM/FOM 참조 후보 계산 및 SiCoPR Python/Octave 독립 수치 비교 성공** ([수치 CI #44070019965](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965), [PR #71](https://github.com/july-rust/SParamView-Public/pull/71)). NEXT/FEXT 패키지는 2020년 P802.3ck 공개 엔지니어링 모델로 별도 명시했으며 **최종 PHY 정식 적합성·C++ COM 구현·정식 PASS/FAIL은 여전히 미완료**다. 과거 번호 계획의 본문은 역사적 기록으로 보존한다.

> **2026-11-17 새 `012_14` 작업:** [COM NEXT/FEXT 개별 간섭 펄스 엔진](COM_Interference_012_14.md)은 검증된 복소 Aggressor 입력에서만 시간영역 간섭을 계산한다. Test1/Test2 및 출처 식별을 유지하며 최종 IEEE COM/PHY PASS/FAIL, 통계 누화 결합 및 GUI는 제외한다. 이 신규 명령은 아래 역사적 계획의 기존 `012_14`(릴리스)와 다르며 실제 COMPLETE 판정은 PR·CI·병합 후 증거로 확정한다.

> **2026-11-17 새 `012_15` 범위:** [수신기 Gaussian 잡음 독립 수치 엔진](COM_Receiver_Noise_012_15.md). 유한 대역 단측 PSD 적분, RMS, PDF/CDF, 기존 Gaussian-only Quantile을 재사용한다. 지터·통계 누화 결합·COM dB/PHY PASS/FAIL·GUI·릴리스는 제외한다. 실제 완료 판정은 해당 PR 최신 HEAD의 필수 CI 및 병합 증거를 확인한 후 기록한다. 이 신규 번호는 아래 2026-11-16 이전 역사적 012_15 번호 계획과 무관하다.

> **2026-11-18 새 `012_16` 범위:** [COM RJ/Dual-Dirac 국소 선형 지터 수치 엔진](COM_Jitter_012_16.md). 독립 시간/UI 변환, 두 원자 이동 Gaussian 혼합의 PDF/CDF/단측 Quantile, `012_15`의 검증된 Gaussian RMS 입력만 재사용한다. 전압 캘리브레이션 및 SourceSnapshot/잡음 Provenance가 일치해야 하며, 비정규(normative 아님) 근사 출력만 제공한다. CTest 단일 타깃으로 독립 고정밀 oracle를 검증한다. 최종 Annex 93A 지터/ISI/XTALK 통계 PDF, PHY PASS/FAIL, GUI, main 및 릴리스는 여전히 제외한다. 실제 완료는 최신 PR HEAD CI·리뷰·병합 증거로만 판정한다.

> **2026-11-18 신규 012_17:** [COM 고정 6-Tap DFE 잔류 ISI 엔진](COM_DFE_012_17.md)은 이전 명령의 임펄스/심볼 혼용 문제를 수정하여 기존 band-limited rectangular-SYMBOL 응답만 입력한다. 최종 120F-8 Tap Min/Max, 명시적인 -3..+9 UI 결정 시점, Test1/Test2 출처 격리, 고정된 독립 수치 Golden 및 실제 심볼 컨볼루션 회귀만 수행한다. DFE 최적화 탐색, Annex 93A 전체 통계, COM dB, PHY PASS/FAIL, GUI, main/릴리스는 제외한다. 완료 여부는 최신 PR HEAD 필수 CI와 보호 병합 증거로만 판정한다.

**기준일:** 2026-11-16 (KST)
**공식 기준:** v1.3.5 (tag/release 불변)
**목표:** ILD·ICN·Eye Diagram·COM을 각각 독립적인 C++20 수치 엔진, Qt6 창, 테스트로 구현하고 **v1.4.0 한 번에 릴리스**
**상태:** 012_01 COMPLETE / 012_02 COMPLETE (REFERENCE_READY) / 012_03 CORE VERIFIED (PR #37; 최종 병합 상태는 PR 참조) / 012_04 GUI/워커 COMPLETE (PR #38; CI 43708927772) / 012_05 COMPLETE (ILD 엔진/GUI PR #40; CI 43766947020) / 012_06 COMPLETE (Generic ICN PR #42; CI 43775680490) / 012_07 COMPLETE (Eye 수치 엔진 PR #45; CI 43798693800) / 012_08 COMPLETE (PR #47 및 통합 CI #43812686401 검증 성공) / 012_09 PARTIAL / BLOCKED (PR #49~#61 병합, #62 Test1/2 물리-RX 통합 CI 검증; PR #59에서 IEEE 2019 C2C 실제 3채널×Test1/Test2의 Python/Octave 4.17 독립 참조 수치 일치 확인, CI #43972559021; 정식 IEEE 최종 표 및 C++ COM 엔진은 미완성), 012_10~14 미착수 / 규격 COM 미구현 / v1.4.0 미출시
**작업 번호:** 012_01 ~ 012_14. 단계별 PR, CI, 증거와 완료 조건을 연결한다.

> 이 문서는 향후 실행할 개발 명령의 기준이며, 문서 작성만으로 기능이나 공식 표준 적합성을 검증했다고 주장하지 않는다.

## 1. 개발 원칙과 UI 계약

1. 공식 v1.3.5 태그·배포 파일은 수정하지 않는다. RL/IL/NEXT/FEXT/TDR 수치와 기존 GUI, Touchstone import, 프로젝트 파일, Mapping CSV v3, 임시 캐시·다중 프로세스 격리·시작 오류 복구를 보존한다.
2. 상단 **Analysis** 메뉴의 4개 항목에서 각 기능의 **별도 비모달 최상위 창**을 연다. 메인 화면 내부 탭·패널 형태로 끼워 넣지 않는다.
3. ILD/ICN/Eye/COM은 동시에 열 수 있고, 창마다 입력 설정·그래프·계산·취소·내보내기가 독립적이다. 같은 종류를 다시 선택하면 이미 열린 창을 활성화한다. 하나를 닫아도 다른 창이나 메인 창을 닫지 않는다.
4. 검증된 Touchstone 복소 S-파라미터, 확정된 포트 매핑과 공통 수치 유틸리티만 공유한다. 각 엔진·자료형·테스트는 분리한다.
5. 프로젝트/리비전 교체, 포트 매핑 수정, 원본 해시 변경 시 작업 취소 또는 세대 무효화하고 결과를 **STALE** 처리한다. 원본이 다른 결과를 현재 값처럼 표시하지 않는다.
6. Qt 이벤트 루프를 차단하지 않는다. 워커 취소·메모리 한계·안전한 객체 수명 관리(QPointer 등)·프로세스별 캐시 격리를 유지한다.
7. 계산 입력·규격 프로파일 또는 독립 검증이 부족하면 **NOT EVALUATED / UNSUPPORTED / INVALID SOURCE**. 실제 계산 없는 더미 그래프나 임의 PASS는 금지한다.
8. 작은 기능 PR은 전용 **v1.4.0 통합 브랜치**에 반영한다. 마지막에 보호 규칙을 준수해 **main으로 최종 PR 1건**을 병합하고, 모든 기능과 3개 플랫폼 검증 통과 뒤 v1.4.0을 단일 공식 배포한다.
9. 단계별 문제는 해당 단계에서 수정·검증한다. 증거 없는 PASS, 단순 컴파일만으로 완료, 표준 전체에 대한 근거 없는 정확도 약속 금지.

## 2. v1.4.0 확정 기능 범위

| 기능 | 필수 계산·GUI 산출물 | 금지되는 주장 |
| --- | --- | --- |
| **ILD** | THRU IL, fitted IL, ILD(f), max/RMS 편차, 피팅 구간·방법, 그래프·CSV/PNG | 일반 RMS를 규격별 weighted FOM과 혼동, 조건 없는 PASS |
| **ICN** | victim + 다중 NEXT/FEXT aggressor, TX PSD/진폭·RX 필터, 개별/합산 누화 및 품질 | TX 조건 없는데 절대 mV 생성, 무조건 비상관 가정 |
| **Eye Diagram** | 위상 포함 복소 전달함수, NRZ bit rate/pattern, impulse/pulse, eye height/width, 그래프·CSV/PNG | 실측 Eye/BER, MIPI 적합성, PAM4·IBIS-AMI (후속) |
| **COM** | **명시한 IEEE 802.3 프로파일 최소 1개**의 THRU/NEXT/FEXT, 패키지·TX/RX EQ·잡음 입력, COM dB/구성 요소·진단 | 범용 3 dB 합격선, 모든 IEEE·MIPI 규격 자동 적합 판정 |

**COM 기준:** 012_02에서는 **승인된 규격의 프로파일 정체성**, 공식 COM v4.17.0 소스/라이선스 고정, 별도 구현으로 실제 COM 수치 재현(THRU/THRU+NEXT/FEXT 2조건), 데이터 외부 다운로드 기반의 재배포 금지 QA 전략을 확정했다. v1.4.0에서 IEEE 802.3ck-2022의 실제 Test1/Test2, Table 120F-8 **모든 값·3건 이상 동규격 독립 수치 참조**, C++ COM 적합성은 **012_09/11에 필수 인수 게이트**로 남긴다. 기준값을 얻은 IEEE P802.3dj 샘플은 단순 알고리즘 상관성 검증이므로 802.3ck 규격 PASS나 MIPI 적합성에 사용 금지. 이 범위 변경은 [012_02 독립 검증](COM_Reference_Qualification_012_02.md)과 함께 명시적으로 문서화했다.

## 3. 코드·브랜치 아키텍처 (제안)

| 계층 | 제안 파일/구성 | 책임 |
| --- | --- | --- |
| 공통 | include/si/channel_analysis/, src/channel_analysis/common_* | 읽기전용 SourceSnapshot, 해시·Revision·Mapping 세대, 복소 trace/재표본화/시간변환, 상태 |
| ILD | src/channel_analysis/ild.cpp, src/gui/analysis/ild_window.cpp | ILD 엔진, 피팅, GUI/내보내기 |
| ICN | src/channel_analysis/icn.cpp, src/gui/analysis/icn_window.cpp | 잡음 적분, Aggressor 모델, GUI |
| Eye | src/channel_analysis/eye.cpp, src/gui/analysis/eye_window.cpp | NRZ 파형, Eye 통계, GUI |
| COM | src/channel_analysis/com.cpp, src/gui/analysis/com_window.cpp | 참조 프로파일, 계산, GUI |
| 창 관리 | src/gui/analysis/window_manager.cpp + 기존 src/window_ui.cpp | Analysis 메뉴, 창 생성/활성화/종료, UI 안전성 |
| 검증 | tests/channel_analysis/, tests/fixtures/channel/ | 독립 oracle·golden fixtures·Qt GUI·기존 회귀 |

이름은 설계안이며 실제 존재하는 파일이 아니다. 현재 v1.3.5의 Cache::trace, src/trace.cpp, src/tdr.cpp, src/workers.cpp와 CMake 빌드 구조를 점검해 재사용한다. FFT 등 중복 구현을 피하되 공통화로 기존 TDR 결과가 변하지 않는지 회귀 검증한다.

## 4. 개별 개발 작업 (012_01 ~ 012_14)

### 012_01 — 기준선 고정·요구사항 및 테스트 기준
- **실행:** 공식 v1.3.5 태그와 main commit, 빌드 타깃·CI·기존 수치/GUI 결과, 문서 및 배포 구조를 기록한다. 전용 통합 브랜치를 만들고 네 기능의 입력·출력·단위·상태·포트 방향 계약을 정한다.
- **산출:** 요구사항·API 초안·기능별 인수표, golden fixture 목록과 출처/라이선스, 실제 수행한 기준선 회귀 로그.
- **완료:** 범위·고정 포트 매핑 정책·독립 오라클 확보 방안·수치 허용 오차 결정 절차 확정. 미실행 baseline은 UNVERIFIED 표기.

### 012_02 — COM 승인 PHY 프로파일 지정·공식 레퍼런스·검증 준비 **COMPLETE (REFERENCE_READY)**
- **2026-11-15 명시적 인수 조건 정정:** 이 단계는 C++ COM **구현 이전의 참조 계산 가능성·출처·QA 라이선스 경로**를 완결한다. 012_09의 구현·규격 수치 적합성 인증을 미리 012_02에 요구한 기존 계획의 중복을 수정한다. COM 개발/릴리스 전체의 정확도 요구를 낮추지 않는다.
- **고정:** 승인 IEEE Std 802.3ck-2022, 100GAUI-1 C2C, 120F.4.1/Table 120F-8 및 93A.1 COM 방법. 공식 COM 4.17.0 Git tag·소스 SHA, BSD3 및 제3자 재배포 없이 외부 QA만 수행하는 방식을 확인한다.
- **실행 증거:** IEEE 공개 채널 6개 SHA-256 확인 후, 독립 Python 구현에서 공식 COM 4.17 공개 Octave 기준값을 **누화 제외/포함 두 조건**에서 실제 재현, COM·FOM·ERL·VEC·VEO·ICN·itick 검증 PASS. [COM CI 43696202018](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018).
- **산출:** [프로파일 정체성](COM_Profile_Lock_012_02.md), [독립 재현 로그](COM_Reference_Qualification_012_02.md), [배포 전략](COM_License_Review_012_02.md), `reference-ready` 실패 차단 테스트. 실제 계산 없는 빈 golden 또는 미완의 표준 수치 테이블을 PASS로 변경하지 않는다.
- **완료:** `REFERENCE_READY` 확인과 참조 숫자 재현, PR 필수 CI 및 squash-merge 성공. **실제 802.3ck 규격 적합성은 여기서 인정하지 않는다.**

### 012_03 — 공통 Channel Analysis Core
- **구현·1차 수치 검증 완료:** Qt-free C++20 `si_channel_analysis`, SHA 재확인 SourceSnapshot, 확정 매핑/방향, 진단 상태, 복소 재표본화, radix-2 FFT/IFFT, 캐시 키/취소, fail-closed. [012_03 구현·제약](Channel_Analysis_Core_012_03.md). 기존 TDR 계산 파일 불변.
- **실제 검증 증거:** [PR #37](https://github.com/july-rust/SParamView-Public/pull/37) · [CI 43702830271](https://github.com/july-rust/SParamView-Public/actions/runs/43702830271): Release Core CTest **32/32**, ASan/UBSan Core CTest **32/32**, Clang 정적 분석, Windows x64/ARM64, macOS arm64 및 **네이티브 Windows ARM64 패키지 게이트 전부 PASS**. PR 최종 커밋과 병합 후 통합 CI를 따로 확인한다.
- **실행:** 불변 SourceSnapshot(파일 SHA·Revision·포트·매핑 세대), typed result, 진단/quality enum, 공통 주파수축·복소 trace·FFT/재표본화/시간영역 변환·데이터 검증 API 구현.
- **산출:** UI 독립 C++20 공통 라이브러리, 계산 캐시 키/메타데이터, 단위 테스트.
- **완료:** DC 부족·불충분 대역·포트 오류·비등간격 데이터 처리 및 구형 TDR/RL/IL/NEXT/FEXT 수치 회귀 통과.

### 012_04 — Analysis 메뉴·독립 Qt 창·워커 프레임워크
- **인수 완료:** [PR #38](https://github.com/july-rust/SParamView-Public/pull/38) squash-merge `c21898657e5bb1fa13a446198eeb0468c6c04f79`; [필수 CI](https://github.com/july-rust/SParamView-Public/actions/runs/43708927772) Core, Windows x64 51/51 CTest, macOS ARM64 47/47 CTest, Windows ARM64 빌드 및 native ARM64 packaged gate 전부 PASS. [GUI/워커 계약](Channel_Analysis_GUI_012_04.md). 최종 통합 브랜치 CI는 [병합 후 실행](https://github.com/july-rust/SParamView-Public/actions/runs/43709948534)에서 별도로 확인한다.
- **실행:** ILD/ICN/Eye/COM을 별도의 최상위 비모달 창으로 연결한다. 진행·취소·상태·내보내기 공통 컨트롤 및 비동기 안전성 구현. 수치 미완 항목은 disabled 처리.
- **산출:** 독립 Window Manager, 4개 전용 창 기반, GUI/lifecycle 테스트.
- **완료:** 서로 다른 창의 동시 실행·재클릭 포커스·단독 종료·프로젝트 변경 시 결과 무효화 및 안전한 워커 종료가 실제 GUI 테스트에서 확인됨.

### 012_05 — ILD 수치 엔진·GUI 완성
- **완료 증거 (2026-11-16 KST):** [PR #40](https://github.com/july-rust/SParamView-Public/pull/40) squash-merge `90a59ada15a9c17aa787ffb317fcafefb4285c75` → `integration/v1.4.0`; [전체 PR CI 43766947020](https://github.com/july-rust/SParamView-Public/actions/runs/43766947020): Core Release 및 ASan/UBSan **33/33 + 33/33**, Windows x64 **52/52**, macOS arm64 **48/48**, Windows ARM64 크로스 빌드/패키지 + **네이티브 ARM64 패키지 게이트** 모두 PASS. [수치/GUI 계약](Channel_Analysis_ILD_012_05.md). 실제 물리적 사용자 GUI 직접 조작은 별도 012_12 최종 릴리스 게이트이며, 표준 가중 FOM·PHY 인증은 주장하지 않는다.
- **실행:** IL, Fitted IL, ILD(f), peak/RMS 구현. 선택한 피팅 함수·가중치·주파수 구간을 출력에 명시하고 안정적 계산법 적용. 공식 규격 프로파일 없이 범용 ILD를 PASS/FAIL로 표기하지 않는다.
- **산출:** 독립 `ild.cpp` 및 `ild.hpp`, 기존 독립 `AnalysisWindow`의 ILD 모드, 원본/피팅/편차 그래프, CSV/PNG, `ild_tests`/GUI 실파일 내보내기 테스트.
- **완료:** 이상 무리플 채널/의도적 리플 채널의 독립 골든 값 일치, 빈/손상 입력 fail-closed, 채널 방향·주파수 범위 검증.

### 012_06 — ICN 수치 엔진·GUI 완성
- **구현·인수 완료(2026-11-16 KST):** [PR #42](https://github.com/july-rust/SParamView-Public/pull/42)가 `integration/v1.4.0`에 squash-merge (1bcf54008d3031c5b8ef634e4d2bef1479eee96a). [CI 43775680490](https://github.com/july-rust/SParamView-Public/actions/runs/43775680490) 전체 PASS: Core Release 및 ASan/UBSan 각각 34/34, Windows x64 53/53, macOS arm64 49/49, Windows ARM64 cross-build 및 네이티브 패키지 게이트 PASS. 독립 Generic ICN 엔진, 비상관 NRZ TX/RX, 다중 NEXT/FEXT, 개별 기여도, Qt 독립 창, CSV/PNG, 실제 s4p GUI·STALE 검증. [수치/GUI 계약](Channel_Analysis_ICN_012_06.md). IEEE/SNIA/MIPI 표준 적합성과 012_12 물리 GUI 및 v1.4.0 릴리스는 **미검증/미출시**.
- **실행:** Victim 및 여러 Aggressor의 NEXT/FEXT 방향·극성을 검증하여 S-파라미터 추출, TX 스펙트럼·진폭 및 RX 필터를 이용해 주파수 적분·전력 합산. 누화 성분별 기여 표시.
- **산출:** icn.cpp, IcnWindow, 개별/전체 기여 그래프, 조건·단위·CSV/PNG, icn_tests.
- **완료:** 0누화/1개·다수 Aggressor/진폭 배율/극성·방향에 대한 독립 oracle 일치. TX 진폭 미상이라면 절대 mV 출력 금지.

### 012_07 — Eye Diagram 수치 엔진
- **실행:** 위상을 보존한 S21/Sdd21의 impulse/pulse response 계산, NRZ pattern 및 bit rate와 컨볼루션, 수신 파형·UI·eye height/width 생성. DC 외삽·재표본화·최고 주파수·인과성/대역 한계와 정량 오차 관리.
- **산출:** eye.cpp, 수치 API, 테스트 파형 및 eye_tests. [012_07 수치·전압 조건 계약](Channel_Analysis_Eye_012_07.md).
- **구현 범위:** MIPI D-PHY HS 4.5 Gb/s·NRZ 기준 주파수 2.25 GHz(변경 시 Bit Rate/UI 연동)·독립 분석 상한 10 GHz 예시, DDR2/3/4/5·LVDS·SLVS·SLVS-EC·Custom 프로파일, 비확정 TX/RX 조건 fail-closed. Eye Qt 통합은 012_08.
- **검증 완료 (2026-11-16 KST):** [PR #45](https://github.com/july-rust/SParamView-Public/pull/45) squash-merge `ae417b64d631da232ee26eaff2291e1eb0b88e5b` → `integration/v1.4.0`. [전체 PR CI #43798693800](https://github.com/july-rust/SParamView-Public/actions/runs/43798693800): version/release consistency, Linux Core(+ASan/UBSan), Windows x64, Windows ARM64 cross-build/package, Windows ARM64 native packaged-binary gate, macOS arm64 **모두 SUCCESS**. 처음 실패한 위상 지연 Golden fixture는 FFT 격자 불일치를 수정하고 최신 HEAD로 전체 재검증했다. **012_07 COMPLETE는 Qt-free 수치 엔진 범위에 한정**; Eye 독립 GUI/SourceSnapshot 연동·Export는 `012_08`, 물리 GUI 최종 검증은 `012_12`. 실제 BER/PHY 적합성·v1.4.0 배포는 주장하지 않는다.
- **완료:** 이상 채널/대역제한/반사 채널의 독립 참조 파형 일치. 실제 BER과 PHY 적합성은 계산되지 않았다고 명시.

### 012_08 — Eye Diagram 독립 창 완성
- **검증·병합 기록:** [PR #47](https://github.com/july-rust/SParamView-Public/pull/47) squash-merged `17bcec471aef0061102293c9ea738b961475c3de`; full head CI [#43811255898](https://github.com/july-rust/SParamView-Public/actions/runs/43811255898) **SUCCESS** across Linux Core/ASan/UBSan/static analysis, Windows x64 CTest+packaging, Windows ARM64 build and native ARM64 packaged-binary qualification, and macOS Apple Silicon arm64 CTest+GUI. All Codex PR review threads resolved. Post-merge integration CI: [#43812686401](https://github.com/july-rust/SParamView-Public/actions/runs/43812686401) **SUCCESS**, verified on merged SHA `17bcec47`. **012_08 is 100% COMPLETE within Eye GUI/SourceSnapshot scope.** [GUI 계약](Channel_Analysis_Eye_GUI_012_08.md). 기능 PR은 병합 완료; 별도 최종 실물 GUI 테스트는 012_12.

- **실행:** EyeWindow의 bit rate, pattern, TX 조건, overlay graph, sampling point, Eye Height/Width, 줌/커서, CSV/PNG 및 비동기 Cancel 구현.
- **산출:** 실제 작동하는 Simulated Channel Eye GUI와 Qt 자동화 검증.
- **완료:** 그래프·숫자 동일 데이터 일치, 큰 데이터 처리/창 종료 안정성, 측정된 Eye와 혼동되지 않는 라벨 확인.

### 012_09 — COM 수치 엔진 및 **최종 승인 PHY 수치 프로파일 자격 검증**

- **2026-11-16 실제 구현 추가:** [PR #49](https://github.com/july-rust/SParamView-Public/pull/49) COM 전처리와 normative fail-closed gate, [PR #50](https://github.com/july-rust/SParamView-Public/pull/50) 복소 2포트/ABCD, [PR #51](https://github.com/july-rust/SParamView-Public/pull/51) S4P 차동 추출·Redheffer 다중 반사·TX/RX 패키지 물리 모델, [PR #52](https://github.com/july-rust/SParamView-Public/pull/52) 2022 IEEE C2C 원본 SHA 및 안전한 XLSX 자료 감사, [PR #53](https://github.com/july-rust/SParamView-Public/pull/53) 독립 PAM4/Gaussian 통계 잡음 정량 모델. **#49~#53 모두 `integration/v1.4.0`에 squash-merge 완료.**
- **기록:** PR #53의 최신 헤드 [CI #43837614094](https://github.com/july-rust/SParamView-Public/actions/runs/43837614094) 전체 PASS, Linux Release/Sanitizer 각 **41/41**; 병합 SHA `cb6448d8672a3c391ef07c14f15290c91e63176b`; 병합 후 [CI #43845851063](https://github.com/july-rust/SParamView-Public/actions/runs/43845851063) 별도 확인 필요. 독립 2022 IEEE 자료 감사 [CI #43835564179](https://github.com/july-rust/SParamView-Public/actions/runs/43835564179) 성공. 자세한 수치 계약은 [COM 단계 증거 문서](Channel_Analysis_COM_012_09.md).
- **2026-11-16 추적 증거 갱신:** [PR #54](https://github.com/july-rust/SParamView-Public/pull/54) 문서화 완료, [병합 후 CI #43849332236](https://github.com/july-rust/SParamView-Public/actions/runs/43849332236) 전체 SUCCESS. [PR #55](https://github.com/july-rust/SParamView-Public/pull/55) SHA 검증된 실제 Touchstone S4P의 16개 측정 포트 응답을 Test1/Test2 TX-Board-RX 패키지 물리 회로로 연결; Linux Core Release 및 Sanitizer 각각 **42/42**, Windows x64/ARM64/native ARM64, macOS arm64 [필수 CI #43854149149](https://github.com/july-rust/SParamView-Public/actions/runs/43854149149) SUCCESS 후 `16288759ae453d45b3da01ec1741a5d9f3f5ea83`로 보호 squash 병합. [병합 후 CI #43857020290](https://github.com/july-rust/SParamView-Public/actions/runs/43857020290)은 별도로 확인. [실제 회로 데이터 경로 문서](COM_Physical_Test_Cases_012_09.md).
- **2026-11-17 독립 참조 계산 검증:** [PR #59](https://github.com/july-rust/SParamView-Public/pull/59)에서 2019년 IEEE 공식 S4P의 실제 차동 물리 포트 순서 [1,2,3,4]를 SHA/10 MHz/26.56 GHz 검증으로 확정하여, 잘못된 기본 포트 매핑에서 발생한 음수 커서·복소수 FOM 오류를 수정했다. 역사적 2022년 COM 설정을 Strict OOXML에서 정확히 변환하고 엑셀 수식 8개를 재검산했다. [6개 실제 독립 COM 결과의 Python/Octave 일치 CI #43972559021](https://github.com/july-rust/SParamView-Public/actions/runs/43972559021)에서 10/16/18 dB 각각 Test1·Test2의 최대 오차 1.2e-13 dB, 원본 SHA, 4 FEXT/0 NEXT, 포트 매핑, 유한 실수 FOM을 확인했다. **이는 정식 802.3ck-2022 Table120F-8 및 C++ 계산이 아니라 역사적 참조 후보 자격검증에 국한된다.** [상세 수치/근거](COM_8023ck_Real_Channel_Reference_012_09.md).\n- **남은 차단:** 2022년 historical sponsor-ballot workbook은 **최종 승인 IEEE 802.3ck-2022의 Table 120F-8와 동치임이 입증되지 않았다**. 전체 IEEE 93A/120F 수치 엔진 및 Test1·Test2 정식 결과, 3건 이상 **같은 정식 PHY** 독립 Golden 비교는 여전히 미완성. `COM dB`, 정식 PASS/FAIL, `012_09=100% COMPLETE`, `v1.4.0` 릴리스 금지.
- **진행 중, 100% COMPLETE 아님:** [012_09 실제 구현·차단 범위](Channel_Analysis_COM_012_09.md). 복소 THRU/FFE/RX/NEXT/FEXT 전처리 및 수치/Fail-Closed 테스트를 추가했으나, 최종 승인 120F-8 수치·패키지 Test1/Test2·동규격 Golden 3건 및 공식 COM 엔진은 미완이다. 수치 COM 출력은 NOT_EVALUATED. 이 범위의 CI·병합 상태는 기능 PR에서 확인한다.
- **실행:** 012_02가 고정한 승인 PHY의 **최종 IEEE 802.3ck-2022 Annex 120F.4.1/Table 120F-8 수치와 예외 규칙 전체**를 정식 자료에서 항목별 검증하고, 지정한 Test1/Test2 패키지 설정 각각 COM을 재현한다. 실제 C++ 엔진의 채널·패키지·TX/RX EQ, ISI/누화/잡음 및 최적화를 구현한다.
- **산출:** com.cpp, 파라미터 출처/버전·테이블 대조, 최소 **3개 이상 같은 정식 802.3ck 프로파일의 독립 COM golden 수치 사례**, 단위/오차/수렴 근거 및 원본 SHA; 독립 참조 차이 보고서.
- **완료:** 전체 Table 120F-8 값 및 Test1/Test2 실제 재현, 3개 동규격 oracle 각각 허용 오차 통과, profile 불일치·미지원은 NOT_EVALUATED. 단순 P802.3dj 예제의 수치 PASS를 802.3ck PHY 적합성으로 전용하지 않는다. **이 게이트가 실패하면 012_09 미완료·v1.4.0 미출시**.

- **2026-11-17 `012_09_03` 부분 구현:** [PR #61](https://github.com/july-rust/SParamView-Public/pull/61)에서 단일 C++ band-limited 원형 임펄스 및 -3~+9 UI tap 진단, analytic delay oracle와 fail-closed 검증을 추가했다. [커밋 기준 CI #43992134634](https://github.com/july-rust/SParamView-Public/actions/runs/43992134634): Release/Sanitizer 각 **45/45**, Windows x64/ARM64 + native ARM64, macOS arm64 **성공**. 이는 **정식 IEEE 93A 펄스/ISI/잡음/DFE/최적화/COM 계산이 아님**. 최종 규격 원문 독립 감사, C++ 대 정식 PHY Golden 3채널×Test1/Test2, COM PASS/FAIL 등 필수 게이트는 **미완료**. 문서 정리는 플랫폼별 중복 없이 1회 배치. `012_09`의 완료 상태를 변경하지 않는다.

- **2026-11-17 `012_09_03` 보완·통합:** [PR #62](https://github.com/july-rust/SParamView-Public/pull/62)에서 기존 TX/Board/RX 물리 S4P 패키지 Test 1·2와 FFE/CTLE/RX 사이 실제 C++ 수치 경로 및 93A-46 Gaussian TX 전이 필터(7.5 ps)를 연결했다. 두 패키지 경로·임피던스·위상·수동성 검증과 독립 10 GHz complex oracle, 실제 합성 4포트 회귀를 추가했다. 최종 코드 커밋 `bcfb8b98` [CI #43998681208](https://github.com/july-rust/SParamView-Public/actions/runs/43998681208): Linux Release/Sanitizer 각각 **45/45**, Windows x64/ARM64, native ARM64, macOS arm64 **전부 SUCCESS**. 이는 **완전한 93A pulse/ISI/jitter/noise/DFE/XTALK 최적화·COM dB나 정식 PHY Golden 3채널 일치가 아니다**. `calculateCom()`은 계속 `NotEvaluated`; `012_09` 전체는 **PARTIAL/BLOCKED**. 문서는 플랫폼별 중복 없이 최종 1회 일괄 갱신했다.

### 012_10 — COM 독립 GUI 및 결과 보고
- **실행:** ComWindow 프로파일/채널/Aggressor/패키지·EQ·잡음 설정, 결과/원인·진단 표시, CSV/PNG 및 재현성 정보 제공.
- **산출:** 실제 COM 엔진 연결된 독립 창, COM UI/test.
- **완료:** 입력 변경·취소·재실행·결과 무효화·내보내기 일치 확인. 필수 입력 부족 시 임의 PASS 금지.

### 012_11 — 전체 수치 교차 검증 및 구버전 회귀
- **실행:** ILD/ICN/Eye/COM의 외부 독립 oracle와 golden 차이 검증. 기존 RL/IL/NEXT/FEXT/TDR, Mapping CSV v1/v2/v3, .siproject, 캐시 및 ASan/UBSan 회귀 테스트.
- **산출:** 기능별 입력 SHA, 기준값/계산값/허용치, PASS/FAIL/UNVERIFIED와 로그·원인·수정 기록.
- **완료:** 네 수치 엔진 실제 PASS, 특히 COM 승인 802.3ck 프로파일의 Test1/Test2 및 012_09 동규격 golden 3건 이상에 대한 독립 수치 차이 PASS, 기존 기능 회귀 없음. 미실행·미검증은 PASS 금지.

### 012_12 — Windows/macOS 네이티브 GUI·안정성·성능
- **실행:** Windows x64, **native Windows ARM64**, macOS arm64 패키지를 실제 실행해 4창 동시 조작, 취소/종료/반복, 프로젝트·매핑 변경, 비정상 종료 캐시 복구, 장시간/대용량 처리 및 메모리 사용량 점검.
- **산출:** 플랫폼별 스크린샷·명령·실행 파일 SHA·로그·종료코드·결과, 성능 기준선/측정값.
- **완료:** 필수 3개 플랫폼 실기반 GUI 테스트·패키징 PASS. Qt offscreen과 Linux 결과만으로 Windows/macOS GUI PASS 주장하지 않음.

### 012_13 — v1.4.0 배포 준비·문서·릴리스 감사
- **실행:** 4개 EN/KO 기능 설명, 예제·설정·제약, COM 프로파일/IEEE 출처, 출력 파일·CSV 보호, 제한 사항, 릴리스 노트·변경 이력 업데이트. 버전·패키지·해시·테스트 정확한 커밋 확인.
- **산출:** 모든 기능 문서·Release Gate 표, 3개 플랫폼 릴리스 후보 자산과 SHA-256, 라이선스 인벤토리.
- **완료:** 문서와 실제 동작 일치, 버전/자산 재현성, 필수 CI 모두 통과. 결함/미완/라이선스 오류 시 릴리스 보류.

### 012_14 — 최종 main 병합 및 v1.4.0 정식 릴리스
- **실행:** 전용 통합 브랜치를 보호된 main으로 최종 PR 한 건으로 squash-merge, 병합 커밋 기준 CI·자산·SHA-256 확인 후 공식 v1.4.0 태그와 GitHub Release 등록.
- **산출:** 공식 release URL, 태그·커밋 SHA, Windows x64/ARM64·macOS arm64 패키지/체크섬, 최종 검증 보고서.
- **완료:** 릴리스 URL·모든 자산 존재/무결성 확인, 문서 최신화, 이전 v1.3.5 태그/배포 파일 그대로 보존.

## 5. 공통 검증 fixture

| 구분 | 검증 관점 |
| --- | --- |
| 이상적 손실 0 THRU | ILD ≈0, NRZ Eye 오픈, ICN=0 (COM은 프로파일 기준 별도) |
| 주파수 의존 손실·리플 | fitted IL 및 ILD 편차, Eye 대역제한·반사 |
| 단일/복수 누화 Aggressor | ICN 누화 기여·포트 방향·전력 합산 |
| DC 누락·희박/비등간격 샘플 | transform 경고 / NOT EVALUATED |
| 프로젝트 교체·Mapping 변경·이전 워커 완료 | STALE 및 캐시/창 무효화 |
| IEEE COM 참조 채널 세트 | 알고리즘·프로파일·수치 oracle 일치 |

모든 golden 자료의 출처·라이선스·SHA 및 오차 기준은 012_01~02에서 미리 확정한다. 근거 없이 모든 결과에 동일한 ±1% 같은 허용 오차를 적용하지 않는다.

## 6. PR 및 현황 관리 정책

- 번호별 개발 명령은 실제 코드를 작성·검증하고 완결한다. 별도의 중복/장황한 체크리스트를 만들지 않는다.
- PR에는 목적, 수정 파일, 사용 fixture, 검증 명령·실행 로그, CI 링크, 테스트 결과, 종료 SHA, 미검증 사유만 간결히 기록한다.
- 중간 개발 PR은 v1.4.0 통합 브랜치에 병합, 최종 main PR은 하나만 사용. 전용 통합 브랜치가 현재 원격 보호 규칙과 맞지 않으면 012_01에서 실제 ruleset을 검사하여 동등한 안전한 방식으로 확정한다.
- 기존 공통 코드 파일 변경은 인터페이스를 먼저 확정해 모듈 간 충돌을 방지한다.
- 오류를 별도 새 번호로 무작정 넘기지 않고 해당 012 항목 안에서 수정한다. 실제로 범위가 변하는 경우에만 명시적으로 계획 개정한다.
- **v1.4.0 출시 필수 게이트:** 4개 기능 실제 구현 + 독립 수치 검증 + 3개 대상 OS GUI·패키지 + 기존 기능 회귀 + COM 라이선스/표준 근거 + 최종 CI·SHA-256. 하나라도 미완이면 v1.4.0 미출시.

### 시작 현황

| 항목 | 상태 | 증거 |
| --- | --- | --- |
| 012_01 기준선/설계 | REQUIREMENTS LOCKED; 통합 브랜치 CI는 실행 후 별도 판정 | [v1.3.5 baseline + CI evidence](Channel_Analysis_Baseline_012_01.md), [기능/API/수치 검증 계약](Channel_Analysis_Requirements_v1.4.0.md) |
| 012_02 COM 참조 준비 | **COMPLETE (REFERENCE_READY)**: IEEE 공식 4.17 source/licensing + 실제 독립 COM 2조건 숫자 재현 PASS; 승인 PHY 최종 숫자·Test1/Test2·3건 동규격 golden은 **012_09 필수** | [COM 프로파일](COM_Profile_Lock_012_02.md) · [참조 검증](COM_Reference_Qualification_012_02.md) · [라이선스](COM_License_Review_012_02.md) |
| 012_03 공통 Core | **CODE + INITIAL CI PASS**; squash merge 및 병합 후 검증은 [PR #37](https://github.com/july-rust/SParamView-Public/pull/37)에서 추적 | [검증 로그](https://github.com/july-rust/SParamView-Public/actions/runs/43702830271), [구현 계약](Channel_Analysis_Core_012_03.md) |
| 012_04 GUI/워커 | **COMPLETE (프레임워크)** | [병합 PR #38](https://github.com/july-rust/SParamView-Public/pull/38) · [CI PASS](https://github.com/july-rust/SParamView-Public/actions/runs/43708927772); 미완성 엔진 메뉴 비활성화 |
| 012_05 ILD 엔진·GUI | **COMPLETE** (선언 범위: 일반 ILD 계산·GUI·자동 CI) | [병합 PR #40](https://github.com/july-rust/SParamView-Public/pull/40) · [CI 43766947020](https://github.com/july-rust/SParamView-Public/actions/runs/43766947020) · 통합 SHA `90a59ada15a9c17aa787ffb317fcafefb4285c75` |
| 012_06 ICN 엔진·GUI | **COMPLETE (Generic 수치/자동화 범위)** | [병합 PR #42](https://github.com/july-rust/SParamView-Public/pull/42) · [3개 플랫폼 CI PASS](https://github.com/july-rust/SParamView-Public/actions/runs/43775680490) · 통합 SHA `1bcf54008d3031c5b8ef634e4d2bef1479eee96a` |
| 012_07 Eye numerical engine | **COMPLETE (NUMERICAL ONLY)** | [PR #45](https://github.com/july-rust/SParamView-Public/pull/45) 병합 `ae417b64`; [전체 CI #43798693800](https://github.com/july-rust/SParamView-Public/actions/runs/43798693800) 필수 플랫폼/Native ARM64 PASS; GUI는 012_08 | 
| 012_08 Eye GUI & SourceSnapshot | **COMPLETE (GUI / Core integration / automatic CI)** | PR #47 squash merged `17bcec47`; PR CI #43811255898 and post-merge integration CI #43812686401 **SUCCESS**; physical GUI acceptance belongs to 012_12 |
| 012_09 COM 수치 엔진 | PARTIAL / BLOCKED | 역사적 C2C 3채널×2패키지의 Python/Octave 독립 COM 교차검증 PASS [CI #43972559021](https://github.com/july-rust/SParamView-Public/actions/runs/43972559021). **최종 승인 IEEE Table120F-8과 C++ COM 수치 엔진 및 최종 PHY Golden 미완**; [검증 한계](Channel_Analysis_COM_012_09.md) |
| **새 012_13** COM 독립 참조 수치 | **COMPLETE (6개 수치 Golden 후보; 이력/모델 명시, PHY 정식 승인 제외)** | [정식 120F-8 수치·외부 물리모델 구분](COM_120F8_Reference_Numerical_012_13.md) · [6개 Golden 후보 JSON](../tests/fixtures/channel/com_reference/012_13_numerical_golden_candidate.json) · [실행 CI #44070019965](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965) · [PR #71](https://github.com/july-rust/SParamView-Public/pull/71) |
| 012_10~012_14 과거 GUI·통합·릴리스 번호 계획 | HISTORICAL RECORD (새 번호와 구분) | v1.3.5 유지; v1.4.0 미출시 |
| 계획 문서 | PLANNED | 본 문서 및 링크된 상위 로드맵 |

## 7. 출처

- 프로젝트 기준: [v1.3.5 릴리스 노트](Release_1.3.5.md), [최종 검증 범위](Final_Qualification_1.3.5.md), [상위 채널 분석 로드맵](Channel_Analysis_Development_Plan.md).
- 실제 코드: [CMakeLists.txt](../CMakeLists.txt), [core.hpp](../include/si/core.hpp), [trace.cpp](../src/trace.cpp), [tdr.cpp](../src/tdr.cpp), [window_ui.cpp](../src/window_ui.cpp).
- Qt 공식: [QMainWindow/QMenuBar](https://doc.qt.io/qt-6/qmainwindow.html), [QPointer](https://doc.qt.io/qt-6/qpointer.html).
- IEEE 공식: [COM ad hoc](https://www.ieee802.org/3/ad_hoc/COM/), [IEEE SA 802-COM](https://opensource.ieee.org/802-com).
