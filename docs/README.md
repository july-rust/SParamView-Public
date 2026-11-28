# SParamView documentation / 문서 색인

**Current official release / 현재 공식 릴리즈:** [v1.4.3](https://github.com/july-rust/SParamView-Public/releases/tag/v1.4.3). The tagged source and release assets are immutable. This index and post-release clarifications on main do not replace those assets.

## Current user documentation / 현재 사용자 문서

- [English User Guide](USER_GUIDE_EN.md) · [한국어 사용자 설명서](USER_GUIDE_KO.md)
- [English README](../README_EN.md) · [한국어 README](../README_KO.md)
- [v1.4.3 release notes](Release_1.4.3.md) · [Revision History](Revision_History.md)
- [Temporary cache policy](Temporary_Cache_Cleanup.md) · [Screenshot provenance](images/README.md)
- [Examples and synthetic fixture provenance](../examples/README.txt)
- [License](../LICENSE) · [Copyright and third-party notices](../COPYRIGHT.txt)

## Qualification and implementation reference / 구현·검증 근거

- [016 pre-release candidate CI](https://github.com/july-rust/SParamView-Public/actions/runs/46120571235) (earlier candidate; not final v1.4.3 evidence)

- [Final v1.4.2 release CI](https://github.com/july-rust/SParamView-Public/actions/runs/46093773220) · [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46093773213)
- [ILD](Channel_Analysis_ILD_012_05.md) · [Generic ICN](Channel_Analysis_ICN_012_06.md)
- [Eye numerical contract](Channel_Analysis_Eye_012_07.md) · [Eye GUI integration](Channel_Analysis_Eye_GUI_012_08.md) · [DC and AC/Auto qualification](qualification/012_54_AC_Auto_Analysis.md)
- [Workspace/file workflow](File_Workflow_Qualification_012_48.md) · [Native platform qualification](Native_Platform_Package_Qualification_012_50.md)

These records establish the scope and evidence of their stated development stages. Current product claims are governed by the current release notes, published validation evidence and actual source. Exhaustive physical LCD/DPI and native interactive performance comparison have not been established.

## Historical archive / 과거 기록

- [v1.4.0 development closeout and historical plan](Development_Plan_012_v1.4.0.md)
- [Channel analysis architecture and historical planning](Channel_Analysis_Development_Plan.md)
- [COM freeze and release scope](Release_Scope_Stabilization_012_43.md) · [COM research](Channel_Analysis_COM_012_09.md)
- [Prior release notes and development timeline](Revision_History.md)

Numbered development plans, prior release notes, COM research, raw qualification logs and historical screenshots are preserved for reproducibility. They do not constitute active work orders, current validation evidence or new release claims. COM remains deferred and has no production menu. The program is an engineering-reference tool, not a standards compliance or measured BER instrument.

## Repository maintenance / 저장소 관리

Keep source, tests, fixtures, licensing and reproducibility evidence. Remove only demonstrably redundant working references or temporary CI outputs after checking their status and dependencies. Do not delete official release tags or assets, rewrite history or remove research evidence merely because it is old. The retired v1.2.1 document refresh tooling was removed via [PR #133](https://github.com/july-rust/SParamView-Public/pull/133).
