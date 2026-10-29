SParamView v1.3.2
=================

SParamView v1.3.2 is a reference tool for calculating and visualizing Touchstone S-parameter data. Supported S-parameter file versions: Touchstone 1.0/1.1/2.0/2.1. Y/Z/H/G are not automatically converted to S parameters.

IMPORTANT NOTICE
----------------
The displayed graphs and calculated results may differ from the actual system or measurement results depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions.

Do not treat SParamView output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

중요 안내
---------
SParamView는 제공된 Touchstone 데이터를 계산하고 시각화하는 참고용 도구입니다. 입력 데이터의 상태, 주파수 범위, 포트/채널 매핑, 보간·근사 및 기타 분석 조건에 따라 표시된 그래프와 계산 결과가 실제 시스템 또는 계측 결과와 다를 수 있습니다.

프로그램의 결과를 100% 정확하거나 절대적인 값으로 간주하지 말고 설계·검토를 위한 참고 자료로 사용해 주십시오. 중요한 판단은 원본 데이터와 신뢰할 수 있는 계측 또는 별도의 분석 결과를 통해 다시 확인하시기 바랍니다.

The title bar shows the currently selected original Touchstone filename (including extension).
Hover over the Revision selector for its full source path. Open Project rotates and clears
the previous project cache/plot/comparison state after successful import; Open Touchstone
continues to append revisions for comparison.

Windows ZIP layout:
- SParamView.exe is the only root-level EXE; required DLLs remain alongside it.
- tools/si_cli.exe contains the optional command-line analyzer.
- tests/ contains auxiliary validation EXEs.

Official release: https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.2

Supported release targets:
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:
- README_EN.md
- README_KO.md
- docs/Release_1.3.1.md

License: MIT
Contact: sparamview@gmail.com
