ARM64 NOTE
==========
The scripts in this directory are historical x64 release-audit machinery and
contain assumptions from older releases. They are not the ARM64 entry point.

For Windows ARM64 use:
  ..\prepare_arm64_release.ps1

Current release and user guidance:
  ..\..\README.md
  ..\..\docs\USER_GUIDE_EN.md
  ..\..\docs\USER_GUIDE_KO.md
  ..\..\docs\README.md

The native ARM64 packaged-binary validation is defined by the current release
workflow in .github/workflows/; published evidence is attached to the release.
