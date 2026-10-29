# SParamView v1.2.5

SParamView는 Touchstone S-parameter 데이터를 열고 채널 기준으로 분석하는 C++20 / Qt 6 데스크톱 프로그램입니다.

> **중요 안내**
> SParamView는 제공된 Touchstone 데이터를 계산하고 시각화하는 참고용 도구입니다. 입력 데이터의 상태, 주파수 범위, 포트/채널 매핑, 보간·근사 및 기타 분석 조건에 따라 표시된 그래프와 계산 결과가 실제 시스템 또는 계측 결과와 다를 수 있습니다. 프로그램의 결과를 100% 정확하거나 절대적인 값으로 간주하지 말고 설계·검토를 위한 참고 자료로 사용해 주십시오. 중요한 판단은 원본 데이터와 신뢰할 수 있는 계측 또는 별도의 분석 결과를 통해 다시 확인하시기 바랍니다.

## 주요 기능

- Touchstone 파일 및 `.siproject` 프로젝트 열기
- Single-ended / Differential 채널 매핑
- Return Loss (RL / S11 / Sdd11)
- Insertion Loss (IL / S21 / Sdd21)
- NEXT / FEXT 분석
- Quick TDR 표시
- 결과표 확인 및 내보내기 워크플로

Quick TDR은 제한된 주파수 대역을 이용한 추정값이며 계측기 TDR 측정을 대체하는 값으로 해석하면 안 됩니다.

## 사용설명서

- [한국어 사용자 설명서](docs/USER_GUIDE_KO.md)
- [English User Guide](docs/USER_GUIDE_EN.md)
- [전체 변경 이력](docs/Revision_History.md)

## v1.2.5 주요 변경사항

- GUI 실행별 임시 캐시 폴더를 분리하고 정상 종료 시 변환 캐시 및 Excel 임시파일을 자동 삭제합니다.
- 비정상 종료로 남은 프로그램 소유 폴더는 다음 실행 때 소유권 및 실행 중 잠금을 검사한 후 안전하게 정리합니다.
- 기본 CLI 캐시도 명령 종료 시 삭제하며, 명시적으로 지정한 캐시 폴더는 보존합니다.
- 원본 Touchstone, 프로젝트, 보고서, 매핑 파일, UI 설정을 삭제하지 않습니다.
- 오래된 버전의 공유 캐시는 실행 중인 이전 버전에 영향을 주지 않도록 자동 삭제 대상에서 제외합니다.

[1.2.5 공식 릴리스 노트](docs/Release_1.2.5.md) 및 [임시 캐시 정리 설계](docs/Temporary_Cache_Cleanup.md)를 참고하십시오.

## 이전 v1.2.4a 변경사항

- 선택 채널·Aggressor·매핑 등 실제 분석 요청이 일치할 때만 결과를 재사용합니다.
- Single-ended 선택을 매핑 재검토와 프로젝트 복원 후에도 유지합니다.
- 프로젝트 복원 시 원본 전체 SHA-256을 검증하고 변경 시 후보를 다시 생성합니다.
- 저장 대상 설정의 변경 상태와 프로젝트 전환 시 저장·취소 처리를 보강했습니다.
- Touchstone 2.1의 추가 옵션 줄 무시 규칙과 대칭 mixed-mode 삼각 행렬을 지원합니다.
- Excel에 분석 시점의 신호·매핑·원본 근거를 기록하고 검증된 패키지만 공식 공개합니다.
- v1.2.2의 간결한 설정 UI와 v1.2.3의 신호 감지 개선을 포함합니다.

**기존 v1.2.4a의 2026-10-13 검증 유지보수:** GCC Release와 Clang ASan/UBSan 검증, macOS Cocoa GUI 시험, Windows ARM64 매핑 보존 반복시험(10/10 PASS)을 적용했습니다. [공식 Actions](https://github.com/july-rust/SParamView-Public/actions/runs/38383938009)에서 컴파일러·Qt 테스트 환경·다운로드 관련 경고 0건을 확인했습니다. 분석 엔진 및 [기존 v1.2.4a 배포 파일](https://github.com/july-rust/SParamView-Public/releases/tag/v1.2.4a)은 교체하지 않았으며 SHA-256도 그대로입니다.

과거 변경 범위는 [v1.2.4a Release Notes](docs/Release_1.2.4a.md)를 확인하세요. 이전 변경사항: [v1.1.6](docs/Release_1.1.6.md), [v1.1.7](docs/Release_1.1.7.md), [v1.2.0](docs/Release_1.2.0.md).

## 지원 릴리스 플랫폼

- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

릴리스 CI는 빌드 전에 Core header, CMake metadata, Windows VERSIONINFO의 버전 일치를 확인합니다. Windows ARM64는 cross-build 후 native ARM64 runner에서 실행 검증하고, macOS는 Apple Silicon runner에서 빌드와 테스트를 수행합니다.

현재 macOS 패키지는 ad-hoc 서명 상태이며 notarization은 적용하지 않았습니다.

## 빌드

필요 항목:

- C++20 compiler
- CMake
- Qt 6 개발 패키지: Core, Gui, Widgets, Concurrent

공식 릴리스 CI는 현재 Qt 6.8.3을 사용합니다.

일반적인 CMake 빌드 흐름:

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

플랫폼별 배포와 검증 절차는 `.github/workflows/`와 `scripts/`에 정의되어 있습니다.

## 분석 결과 해석

- RL/IL은 가능한 경우 S11/Sdd11, S21/Sdd21 표기를 함께 사용합니다.
- NEXT는 Near-end aggressor가 Near-end victim에 미치는 응답입니다.
- FEXT는 Near-end aggressor가 Far-end victim에 미치는 응답입니다.
- OK는 활성화된 Limit 기준을 통과했다는 의미이고, NG는 위반이 검출되었다는 의미입니다. N/A는 PASS가 아닙니다.
- Differential P-N 100 ohm 종단과 P-GND/N-GND 각각 50 ohm 종단은 서로 다른 구성입니다.

## 예제 데이터

`examples/` 아래 파일은 회귀시험과 UI 확인을 위해 생성한 synthetic demonstration fixture입니다. 실제 제품의 측정 데이터가 아닙니다.

## 라이선스

SParamView 프로그램 소스는 MIT License를 사용합니다. Third-party component, runtime notice, font는 각각의 라이선스를 유지합니다. `LICENSE`, `COPYRIGHT.txt`, `third_party/`를 확인하세요.


### v1.3.0 Touchstone 및 Windows 배포 구조

Touchstone 1.0/1.1 (버전 키워드 없이 선언) 및 2.0/2.1 (`[Version]` 선언)의 S-파라미터 파일을 지원합니다. Y/Z/H/G 행렬은 잘못된 S-파라미터로 해석하지 않고 별도 변환 필요 오류를 표시합니다. 제조사 확장 주석 `! Port1_U1_2::NET_P`를 물리 포트 이름으로 표시하고 `!.DiffChannels`의 인덱스·NET·극성과 교차 검증합니다. Windows에서는 최상위 EXE는 `SParamView.exe` 하나이고, CLI는 `tools/`, 테스트 EXE는 `tests/`로 분리합니다.
