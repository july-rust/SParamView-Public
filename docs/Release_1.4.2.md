# SParamView v1.4.2

Maintenance update to [v1.4.1](Release_1.4.1.md) — 014 Eye Diagram Multi-Graph Display & Adaptive Rendering Qualification.

## Changes

- Eye Diagram now offers **All three graphs** alongside the existing individual views: Eye overlay (2 UI), TX / RX waveform and Impulse / Unit pulse. All panels share one accepted numerical result, use independent axes and interaction, and support PNG export of the selected view. The combined layout adapts to logical width; narrow windows can scroll. The existing simulation-only qualification limits remain unchanged.
- Eye Diagram에 기존 개별 표시와 함께 **그래프 3개 모두** 옵션을 추가했습니다. Eye overlay (2 UI), TX/RX 파형, Impulse/Unit pulse를 같은 분석 결과에서 표시하며, 독립 축·확대·이동 및 선택한 화면의 PNG Export를 지원합니다. 좁은 화면에서는 세로 배치와 스크롤을 사용합니다. 기존 시뮬레이션 한계는 변경하지 않습니다.
- Display selection is remembered outside automated tests. Existing individual view indices remain unchanged. Multi View uses the existing Qt painter, independent interaction, per-panel backing layers with bounded allocation, and a shared immutable result. No numerical model changes are intended.
- Existing v1.4.1 geometry, workspace lifecycle, source integrity, DC/AC-coupling policies and deferred COM scope remain in force.

## Qualification

The new GUI regression covers three-panel axes, independent zoom, identical CSV bytes, PNG provenance, and repeated display switching without restarting numerical analysis. Exact candidate CI results are required before publication; these statements describe implemented tests, not a claim of executed success. Physical LCD/DPI coverage, memory measurements and native interactive performance must be reported separately where available.

## Downloads

The existing release workflow publishes validated Windows x64/ARM64 ZIPs, optional QA bundles, macOS Apple Silicon ZIP/DMG, guides and SHA-256 checksums after mandatory gates. Previous release assets remain immutable.

[English guide](USER_GUIDE_EN.md) · [한국어 사용자 설명서](USER_GUIDE_KO.md) · [Revision history](Revision_History.md)
