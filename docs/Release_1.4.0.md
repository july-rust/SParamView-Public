# SParamView v1.4.0

Engineering-reference desktop Touchstone analyzer for Windows x64, Windows ARM64 and macOS Apple Silicon.

### What's new in v1.4.0

- Independent ILD, Generic ICN and simulated NRZ Eye Diagram analysis windows. Select the mapped victim/path; use Auto Analysis/Auto Preview for initial conditions or Run Manual for known conditions. Review assumptions, warnings, and quality status; export CSV/PNG.
- ILD displays measured/fitted insertion loss and deviation. ICN integrates mapped NEXT/FEXT paths with explicit signal/filter and independent-path RSS assumptions. Eye Diagram is a band-limited simulation, not measured BER. Auto settings are engineering assumptions, not inferred PHY settings.
- Eye Diagram checks DC quality. For missing DC, start frequency up to 10 kHz has no frequency-based confirmation, above 10 kHz and below 100 kHz requires confirmation, and at or above 100 kHz is blocked; other quality checks still apply. Conditional AC-like one-pole extrapolation does not establish physical topology or long-pattern accuracy.
- Runtime English/Korean/Japanese language switching; Language menu stays in English. New resets the workspace after unsaved-change confirmation; multiple Touchstone files are processed sequentially with individual mapping. Last directory and main-window state are remembered. Analysis windows adapt to the display; guidance scrolls independently of plots. Table refresh, overlay and rendering are optimized.
- Default limits: RL -10 dB, IL -3 dB, NEXT/FEXT -20 dB, TDR -10%; checks initially off. These are configurable examples, not standards.
- Windows user ZIP excludes optional test executables and verification launcher; separate QA ZIP supports validation.

**Scope:** engineering-reference calculations only. COM is deferred and has no production menu. No IEEE/SNIA/MIPI compliance, measured BER or exhaustive physical-platform/HiDPI certification is claimed.

### v1.4.0 주요 기능과 사용법

- Analysis 메뉴에서 ILD, Generic ICN, Eye Diagram 독립 창을 실행합니다. 매핑된 피해 채널과 경로를 선택하고 Auto Analysis/Auto Preview로 초기 조건을 설정하거나 Run Manual로 실제 조건을 입력합니다. 적용 조건, 품질 경고를 확인하고 CSV/PNG를 내보냅니다.
- ILD는 측정 삽입 손실·적합 곡선·편차를 표시합니다. ICN은 매핑된 NEXT/FEXT 경로, 신호/필터 및 독립 경로 RSS 가정에 따른 누화 적분입니다. Eye Diagram은 대역 제한 NRZ 시뮬레이션이며 측정 BER이 아닙니다. 자동 설정은 실제 PHY 설정의 보증이 아닌 참고용 가정입니다.
- Eye Diagram의 DC 누락 시 시작 주파수 10 kHz 이하는 주파수 기준 확인 없이, 10 kHz 초과~100 kHz 미만은 확인 후 실행하며 100 kHz 이상은 차단합니다. 다른 품질 조건은 계속 적용합니다. AC-like 단일 Pole 외삽은 실제 토폴로지나 장시간 패턴을 보증하지 않습니다.
- Language 메뉴에서 English/Korean/Japanese를 재시작 없이 전환합니다. New는 미저장 변경을 확인하고 작업 공간을 초기화합니다. 복수 Touchstone 파일은 개별 매핑과 순차 처리, 마지막 폴더 및 메인 창 상태 복원을 지원합니다. 분석 창은 화면에 적응하며 설명은 스크롤하고 그래프 영역을 유지합니다. 테이블·Overlay·렌더링을 최적화했습니다.
- 기본 Limit: RL -10 dB, IL -3 dB, NEXT/FEXT -20 dB, TDR -10%; 기본 체크 해제. 규격이 아닌 변경 가능한 예시입니다.
- Windows 사용자 ZIP과 검증용 전체 QA ZIP을 분리했습니다.

**적용 범위:** 엔지니어링 참고용입니다. COM은 개발 보류되어 메뉴에 없으며 IEEE/SNIA/MIPI 적합성, 측정 BER, 모든 물리 장치/HiDPI 검증을 보증하지 않습니다.

## Qualification and limitations

- The release owner reported smooth operation and no particular problems in actual use on 2026-11-27 KST and authorized release and closure of issues #118 and #121. Scoped user acceptance is not exhaustive physical certification.
- Integration baseline [e0270a9](https://github.com/july-rust/SParamView-Public/commit/e0270a9ff72c7dcdb6f144c983c07d46041708d0): [three-platform CI](https://github.com/july-rust/SParamView-Public/actions/runs/45985134331), [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/45985134272), [012_61 evidence](https://github.com/july-rust/SParamView-Public/pull/129#issuecomment-7216980722), [012_62 package audit](https://github.com/july-rust/SParamView-Public/issues/118#issuecomment-7217121886). Core and sanitizer 70/70 each, x64 89/89, macOS 85/85, native Windows ARM64 packaged gate passed. These precede the version bump; final release CI verifies the stamped build.
- No exhaustive physical display/HiDPI, GPU driver, prolonged process/GPU memory, private S36P final-package workflow, instrument correlation or dynamic localization inventory certification. CI 1000-row median approximately 21–23 ms; x64/macOS P95 exceeded 100 ms. No FPS or universal latency guarantee.
- macOS ad-hoc signed, not notarized. Excel desktop and Windows/macOS interactive Calc round-trips not exhaustively verified. Quick TDR is a finite-band estimate. Verify important engineering decisions independently.
- Final release workflow validates exact commit, packages, native smoke receipts and SHA-256 before publication. Prior release tags/assets are immutable.

## Downloads

Windows x64 and ARM64 user ZIPs and optional QA ZIPs; macOS Apple Silicon ZIP/DMG. English/Korean user guides, README_FIRST, validation evidence, release status JSON and SHA-256 checksums.

[English user guide](https://github.com/july-rust/SParamView-Public/blob/v1.4.0/docs/USER_GUIDE_EN.md) · [한국어 사용자 설명서](https://github.com/july-rust/SParamView-Public/blob/v1.4.0/docs/USER_GUIDE_KO.md) · [Historical v1.3.5](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.5).
