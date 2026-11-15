# SParamView v1.4.0 — 012 시리즈 전체 개발·검증·릴리스 계획

**기준일:** 2026-11-15 (KST)
**공식 기준:** v1.3.5 (tag/release 불변)
**목표:** ILD·ICN·Eye Diagram·COM을 각각 독립적인 C++20 수치 엔진, Qt6 창, 테스트로 구현하고 **v1.4.0 한 번에 릴리스**
**상태:** 012_01 COMPLETE / 012_02 COMPLETE (REFERENCE_READY: COM 참조·독립 수치 재현 완료; IEEE PHY 최종 수치 적합성은 012_09/11 필수) / 분석 엔진 미구현 / v1.4.0 미출시
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
- **실행:** 불변 SourceSnapshot(파일 SHA·Revision·포트·매핑 세대), typed result, 진단/quality enum, 공통 주파수축·복소 trace·FFT/재표본화/시간영역 변환·데이터 검증 API 구현.
- **산출:** UI 독립 C++20 공통 라이브러리, 계산 캐시 키/메타데이터, 단위 테스트.
- **완료:** DC 부족·불충분 대역·포트 오류·비등간격 데이터 처리 및 구형 TDR/RL/IL/NEXT/FEXT 수치 회귀 통과.

### 012_04 — Analysis 메뉴·독립 Qt 창·워커 프레임워크
- **실행:** ILD/ICN/Eye/COM을 별도의 최상위 비모달 창으로 연결한다. 진행·취소·상태·내보내기 공통 컨트롤 및 비동기 안전성 구현. 수치 미완 항목은 disabled 처리.
- **산출:** 독립 Window Manager, 4개 전용 창 기반, GUI/lifecycle 테스트.
- **완료:** 서로 다른 창의 동시 실행·재클릭 포커스·단독 종료·프로젝트 변경 시 결과 무효화 및 안전한 워커 종료가 실제 GUI 테스트에서 확인됨.

### 012_05 — ILD 수치 엔진·GUI 완성
- **실행:** IL, Fitted IL, ILD(f), peak/RMS 구현. 선택한 피팅 함수·가중치·주파수 구간을 출력에 명시하고 안정적 계산법 적용. 공식 규격 프로파일 없이 범용 ILD를 PASS/FAIL로 표기하지 않는다.
- **산출:** ild.cpp, IldWindow, 원본/피팅/편차 그래프, CSV/PNG, ild_tests.
- **완료:** 이상 무리플 채널/의도적 리플 채널의 독립 골든 값 일치, 빈/손상 입력 fail-closed, 채널 방향·주파수 범위 검증.

### 012_06 — ICN 수치 엔진·GUI 완성
- **실행:** Victim 및 여러 Aggressor의 NEXT/FEXT 방향·극성을 검증하여 S-파라미터 추출, TX 스펙트럼·진폭 및 RX 필터를 이용해 주파수 적분·전력 합산. 누화 성분별 기여 표시.
- **산출:** icn.cpp, IcnWindow, 개별/전체 기여 그래프, 조건·단위·CSV/PNG, icn_tests.
- **완료:** 0누화/1개·다수 Aggressor/진폭 배율/극성·방향에 대한 독립 oracle 일치. TX 진폭 미상이라면 절대 mV 출력 금지.

### 012_07 — Eye Diagram 수치 엔진
- **실행:** 위상을 보존한 S21/Sdd21의 impulse/pulse response 계산, NRZ pattern 및 bit rate와 컨볼루션, 수신 파형·UI·eye height/width 생성. DC 외삽·재표본화·최고 주파수·인과성/대역 한계와 정량 오차 관리.
- **산출:** eye.cpp, 수치 API, 테스트 파형 및 eye_tests.
- **완료:** 이상 채널/대역제한/반사 채널의 독립 참조 파형 일치. 실제 BER과 PHY 적합성은 계산되지 않았다고 명시.

### 012_08 — Eye Diagram 독립 창 완성
- **실행:** EyeWindow의 bit rate, pattern, TX 조건, overlay graph, sampling point, Eye Height/Width, 줌/커서, CSV/PNG 및 비동기 Cancel 구현.
- **산출:** 실제 작동하는 Simulated Channel Eye GUI와 Qt 자동화 검증.
- **완료:** 그래프·숫자 동일 데이터 일치, 큰 데이터 처리/창 종료 안정성, 측정된 Eye와 혼동되지 않는 라벨 확인.

### 012_09 — COM 수치 엔진 및 **최종 승인 PHY 수치 프로파일 자격 검증**
- **실행:** 012_02가 고정한 승인 PHY의 **최종 IEEE 802.3ck-2022 Annex 120F.4.1/Table 120F-8 수치와 예외 규칙 전체**를 정식 자료에서 항목별 검증하고, 지정한 Test1/Test2 패키지 설정 각각 COM을 재현한다. 실제 C++ 엔진의 채널·패키지·TX/RX EQ, ISI/누화/잡음 및 최적화를 구현한다.
- **산출:** com.cpp, 파라미터 출처/버전·테이블 대조, 최소 **3개 이상 같은 정식 802.3ck 프로파일의 독립 COM golden 수치 사례**, 단위/오차/수렴 근거 및 원본 SHA; 독립 참조 차이 보고서.
- **완료:** 전체 Table 120F-8 값 및 Test1/Test2 실제 재현, 3개 동규격 oracle 각각 허용 오차 통과, profile 불일치·미지원은 NOT_EVALUATED. 단순 P802.3dj 예제의 수치 PASS를 802.3ck PHY 적합성으로 전용하지 않는다. **이 게이트가 실패하면 012_09 미완료·v1.4.0 미출시**.

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
| 012_03~012_14 구현/릴리스 | NOT STARTED | 4개 분석 엔진 아직 미구현; 기존 v1.3.5 불변 |
| 계획 문서 | PLANNED | 본 문서 및 링크된 상위 로드맵 |

## 7. 출처

- 프로젝트 기준: [v1.3.5 릴리스 노트](Release_1.3.5.md), [최종 검증 범위](Final_Qualification_1.3.5.md), [상위 채널 분석 로드맵](Channel_Analysis_Development_Plan.md).
- 실제 코드: [CMakeLists.txt](../CMakeLists.txt), [core.hpp](../include/si/core.hpp), [trace.cpp](../src/trace.cpp), [tdr.cpp](../src/tdr.cpp), [window_ui.cpp](../src/window_ui.cpp).
- Qt 공식: [QMainWindow/QMenuBar](https://doc.qt.io/qt-6/qmainwindow.html), [QPointer](https://doc.qt.io/qt-6/qpointer.html).
- IEEE 공식: [COM ad hoc](https://www.ieee802.org/3/ad_hoc/COM/), [IEEE SA 802-COM](https://opensource.ieee.org/802-com).
