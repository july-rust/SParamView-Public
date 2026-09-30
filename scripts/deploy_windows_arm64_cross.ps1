param(
  [Parameter(Mandatory=$true)][string]$QtRoot,
  [Parameter(Mandatory=$true)][string]$HostQtRoot,
  [string]$BundleRoot = '',
  [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$targetRoot = (Resolve-Path -LiteralPath $QtRoot).Path.Replace('\','/')
$hostRoot = (Resolve-Path -LiteralPath $HostQtRoot).Path.Replace('\','/')
$hostDeploy = Join-Path $hostRoot 'bin/windeployqt.exe'
$hostPaths = Join-Path $hostRoot 'bin/qtpaths.exe'
$hostCore = Join-Path $hostRoot 'bin/Qt6Core.dll'
foreach ($file in @($hostDeploy, $hostPaths, $hostCore)) {
  if (-not (Test-Path -LiteralPath $file)) { throw "Missing host tool dependency: $file" }
}
if (-not (Test-Path (Join-Path $targetRoot 'mkspecs/win32-arm64-msvc/qmake.conf'))) {
  throw 'The target Qt kit is missing the Windows ARM64 mkspec.'
}

# Use ARM64 CRT DLLs instead of the redistributable installer, so the
# portable bundle needs no installer and every bundled PE can be audited.
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsPath = & $vswhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsPath) { throw 'Visual Studio 2022 ARM64 tools were not found.' }
$redistRoot = Join-Path $vsPath.Trim() 'VC/Redist/MSVC'
# Visual Studio may also provide aliases such as v143; only numeric
# version directories can be sorted as System.Version.
$runtimeCandidates = @(Get-ChildItem -LiteralPath $redistRoot -Directory |
  Where-Object {
    $parsedVersion = $null
    [version]::TryParse($_.Name, [ref]$parsedVersion)
  } |
  Sort-Object { [version]$_.Name } -Descending |
  ForEach-Object { Join-Path $_.FullName 'arm64/Microsoft.VC143.CRT' })
# Support installations that expose the runtime only through the v143 alias.
$runtimeCandidates += (Join-Path $redistRoot 'v143/arm64/Microsoft.VC143.CRT')
$runtimeDir = $runtimeCandidates |
  Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
  Select-Object -First 1
if (-not $runtimeDir) { throw 'ARM64 Microsoft.VC143.CRT runtime DLLs were not found.' }
foreach ($name in @('msvcp140.dll','vcruntime140.dll')) {
  if (-not (Test-Path (Join-Path $runtimeDir $name))) { throw "Missing ARM64 runtime: $name" }
}
# The SDK CRT folder may also contain an x64 vcruntime140_1.dll.
# Select native ARM64 files and verify runtime dependencies before deployment.
python (Join-Path $PSScriptRoot 'deploy_arm64_crt.py') $runtimeDir
if ($LASTEXITCODE -ne 0) { throw 'ARM64 CRT selection or dependency audit failed.' }

# Qt 6.8 cross kits have no target windeployqt.exe. The host tool can
# inspect ARM64 binaries, but its qtpaths query MUST point at the target.
# Qt 6.8 windeployqt launches qtpaths with CreateProcessW, so do not
# pass the cross kit's .bat wrapper. Use a private host qtpaths copy
# with an explicit qt.conf; never alter the installed host kit.
$tempTools = Join-Path ([IO.Path]::GetTempPath()) ('SParamView-qtpaths-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempTools | Out-Null
try {
  Copy-Item -LiteralPath $hostPaths -Destination $tempTools
  Copy-Item -LiteralPath $hostCore -Destination $tempTools
  @"
[Paths]
Prefix=$targetRoot
HostPrefix=$hostRoot
HostData=$targetRoot
TargetSpec=win32-arm64-msvc
HostSpec=win32-msvc
"@ | Set-Content -LiteralPath (Join-Path $tempTools 'qt.conf') -Encoding utf8
  $queryTool = Join-Path $tempTools 'qtpaths.exe'
  $queryOutput = & $queryTool --query
  if ($LASTEXITCODE -ne 0) { throw 'Target Qt path query failed.' }
  $properties = @{}
  foreach ($line in $queryOutput) {
    if ($line -match '^([^:]+):(.*)$') { $properties[$matches[1]] = $matches[2] }
  }
  $expectedPaths = @{
    QT_INSTALL_BINS = (Join-Path $targetRoot 'bin')
    QT_INSTALL_PLUGINS = (Join-Path $targetRoot 'plugins')
    QT_INSTALL_ARCHDATA = $targetRoot
  }
  foreach ($key in $expectedPaths.Keys) {
    if (-not $properties.ContainsKey($key)) { throw "Missing Qt query key: $key" }
    $actual = [IO.Path]::GetFullPath($properties[$key]).TrimEnd([char[]]'\/')
    $expected = [IO.Path]::GetFullPath($expectedPaths[$key]).TrimEnd([char[]]'\/')
    if ($actual -ine $expected) { throw "Qt query points at wrong kit: $key=$actual, expected $expected" }
  }
  if ($properties['QT_VERSION'] -ne '6.8.3' -or $properties['QMAKE_XSPEC'] -ne 'win32-arm64-msvc') {
    throw 'Qt deployment query version or target mkspec mismatch.'
  }
  Write-Host "Cross deployment preflight PASS: Qt $($properties['QT_VERSION']), $($properties['QMAKE_XSPEC'])"
  if (-not $CheckOnly) {
    if (-not $BundleRoot -or -not (Test-Path (Join-Path $BundleRoot 'SParamView.exe'))) {
      throw 'BundleRoot must contain the built SParamView.exe.'
    }
    & $hostDeploy --qtpaths $queryTool --release --no-translations --no-compiler-runtime --no-opengl-sw (Join-Path $BundleRoot 'SParamView.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Host windeployqt with target Qt paths failed.' }
    $offscreen = Join-Path $targetRoot 'plugins/platforms/qoffscreen.dll'
    if (-not (Test-Path $offscreen)) { throw 'Target qoffscreen.dll was not found.' }
    New-Item -ItemType Directory -Path (Join-Path $BundleRoot 'platforms') -Force | Out-Null
    Copy-Item -LiteralPath $offscreen -Destination (Join-Path $BundleRoot 'platforms') -Force
    python (Join-Path $PSScriptRoot 'deploy_arm64_crt.py') $runtimeDir --bundle $BundleRoot
    if ($LASTEXITCODE -ne 0) { throw 'ARM64 CRT deployment or bundle dependency audit failed.' }
  }
} finally {
  Remove-Item -LiteralPath $tempTools -Recurse -Force
}
