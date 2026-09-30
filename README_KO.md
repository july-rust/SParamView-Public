# SParamView v1.1.5

SParamView는 Touchstone S-parameter 데이터를 열고 채널 기준으로 분석하는 C++20 / Qt 6 데스크톱 프로그램입니다.

## 주요 기능

- Touchstone 파일 및 `.siproject` 프로젝트 열기
- Single-ended / Differential 채널 매핑
- Return Loss (RL / S11 / Sdd11)
- Insertion Loss (IL / S21 / Sdd21)
- NEXT / FEXT 분석
- Quick TDR 표시
- 결과표 확인 및 내보내기 워크플로

Quick TDR은 제한된 주파수 대역을 이용한 추정값이며 계측기 TDR 측정을 대체하는 값으로 해석하면 안 됩니다.

## v1.1.5 변경사항

- 우측 상단 헤더에 **Open Project**, **Save Project** 버튼을 **+ Open Touchstone** 옆에 추가했습니다.
- 기존 프로젝트 Load/Save 경로를 그대로 재사용하며 프로젝트 파일 형식은 변경하지 않았습니다.
- 이번 릴리스는 UI와 플랫폼 지원 확대가 중심이며 기존 Touchstone 파싱 및 분석 계산식은 의도적으로 변경하지 않았습니다.

세부 범위는 [v1.1.5 Release Notes](docs/Release_1.1.5.md)를 확인하세요.

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
