param(
  [string]$QtRoot = $env:QT_ROOT_DIR,
  [ValidateSet('MSVC','MinGW')][string]$Compiler = 'MSVC',
  [ValidateSet('x64','ARM64')][string]$Architecture = 'x64',
  [string]$HostQtRoot = $env:QT_HOST_PATH,
  [switch]$SkipTests,
  [switch]$SkipDeploy,
  [switch]$SkipPreflight
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cmakeProject = Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw
if ($cmakeProject -notmatch 'project\(SParamView VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
  throw 'Unable to determine SParamView release version from CMakeLists.txt.'
}
$releaseVersion = $Matches[1]

if (-not $QtRoot -or -not (Test-Path $QtRoot)) {
  throw 'Set -QtRoot to the Qt target kit. x64: msvc2022_64 or mingw_64; ARM64: msvc2022_arm64.'
}
if ($Architecture -eq 'ARM64' -and $Compiler -ne 'MSVC') {
  throw 'Windows ARM64 is supported with MSVC 2022 only. MinGW ARM64 is not supported by the Qt 6.8 Windows target used by SParamView.'
}

function Invoke-Checked {
  param([string]$Program, [string[]]$Arguments)
  & $Program @Arguments
  if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}

function Get-FileSha256([string]$Path) {
  return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

$archTag = if ($Architecture -eq 'ARM64') { 'ARM64' } else { 'x64' }
$buildRoot = Join-Path $projectRoot ("build-windows-" + $archTag.ToLowerInvariant())
$deployRoot = Join-Path $projectRoot ("dist\SParamView-Windows-" + $archTag)
$validationRoot = Join-Path $projectRoot ("validation-windows-" + $archTag.ToLowerInvariant())
New-Item -ItemType Directory -Path $validationRoot -Force | Out-Null

if ($Architecture -eq 'ARM64' -and -not $SkipPreflight) {
  $preflight = Join-Path $PSScriptRoot 'preflight_windows_arm64.ps1'
  $report = Join-Path $validationRoot 'preflight.json'
  $preflightArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$preflight,'-QtRoot',$QtRoot,'-Report',$report)
  if ($HostQtRoot) { $preflightArgs += @('-HostQtRoot',$HostQtRoot) }
  Invoke-Checked 'powershell.exe' $preflightArgs
}

$cmakeArgs = @('-S',$projectRoot,'-B',$buildRoot,"-DCMAKE_PREFIX_PATH=$QtRoot")
if ($Compiler -eq 'MSVC') {
  $cmakeArgs += @('-G','Visual Studio 17 2022','-A',$Architecture)
} else {
  $cmakeArgs += @('-G','Ninja','-DCMAKE_BUILD_TYPE=Release')
}

# A target ARM64 Qt package can use x64 host tools while cross-compiling.
if ($Architecture -eq 'ARM64' -and $HostQtRoot) {
  if (-not (Test-Path $HostQtRoot)) { throw "Host Qt path does not exist: $HostQtRoot" }
  $cmakeArgs += "-DQT_HOST_PATH=$HostQtRoot"
}

Invoke-Checked 'cmake' $cmakeArgs
Invoke-Checked 'cmake' @('--build',$buildRoot,'--config','Release','--parallel','4')

# Fail early if the produced executable architecture is not the requested one.
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { throw 'python is required for release architecture verification.' }
$appCandidate = Join-Path $buildRoot 'Release\SParamView.exe'
if (-not (Test-Path $appCandidate)) { $appCandidate = Join-Path $buildRoot 'SParamView.exe' }
if (-not (Test-Path $appCandidate)) { throw "SParamView.exe not found under $buildRoot" }
$versionInfo = (Get-Item $appCandidate).VersionInfo
$versionRegex = '^' + [regex]::Escape($releaseVersion) + '(?:\.0)?$'
if ($versionInfo.ProductName -ne 'SParamView') { throw "EXE ProductName mismatch: $($versionInfo.ProductName)" }
if ($versionInfo.ProductVersion -notmatch $versionRegex) { throw "EXE ProductVersion mismatch: $($versionInfo.ProductVersion) expected $releaseVersion" }
if ($versionInfo.FileVersion -notmatch $versionRegex) { throw "EXE FileVersion mismatch: $($versionInfo.FileVersion) expected $releaseVersion" }
if ($versionInfo.LegalCopyright -ne 'Copyright © 2026 july') { throw "EXE LegalCopyright mismatch: $($versionInfo.LegalCopyright)" }
$exeMetadata = [ordered]@{
  product_name = $versionInfo.ProductName
  product_version = $versionInfo.ProductVersion
  file_version = $versionInfo.FileVersion
  legal_copyright = $versionInfo.LegalCopyright
}
$exeMetadata | ConvertTo-Json | Set-Content -Path (Join-Path $validationRoot 'exe_metadata.json') -Encoding UTF8
Invoke-Checked $python.Source @((Join-Path $PSScriptRoot 'verify_pe_machine.py'),$appCandidate,$Architecture,(Join-Path $validationRoot 'build_architecture.json'))

$osArch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$canRunTarget = ($Architecture -eq 'x64' -and $osArch -in @('X64','Arm64')) -or ($Architecture -eq 'ARM64' -and $osArch -eq 'Arm64')
if (-not $SkipTests) {
  if ($canRunTarget) {
    $env:PATH = (Join-Path $QtRoot 'bin') + [IO.Path]::PathSeparator + $env:PATH
    $env:QT_PLUGIN_PATH = Join-Path $QtRoot 'plugins'
    Invoke-Checked 'ctest' @('--test-dir',$buildRoot,'-C','Release','--output-on-failure')
  } else {
    Write-Warning "Skipping target runtime tests: OS=$osArch target=$Architecture. Native Windows $Architecture execution is mandatory before release."
  }
}

if (Test-Path $deployRoot) { Remove-Item $deployRoot -Recurse -Force }
Invoke-Checked 'cmake' @('--install',$buildRoot,'--config','Release','--prefix',$deployRoot)

if (-not $SkipDeploy -and $Architecture -eq 'ARM64' -and $osArch -ne 'Arm64') {
  if (-not $HostQtRoot) { throw 'ARM64 cross deployment requires -HostQtRoot.' }
  & (Join-Path $PSScriptRoot 'deploy_windows_arm64_cross.ps1') -QtRoot $QtRoot -HostQtRoot $HostQtRoot -BundleRoot $deployRoot
} elseif (-not $SkipDeploy) {
  $deployQt = Join-Path $QtRoot 'bin\windeployqt.exe'
  if (Test-Path $deployQt) {
    try {
      Invoke-Checked $deployQt @('--release','--no-translations','--compiler-runtime',(Join-Path $deployRoot 'SParamView.exe'))
      $offscreen = Join-Path $QtRoot 'plugins\platforms\qoffscreen.dll'
      if (Test-Path $offscreen) {
        New-Item -ItemType Directory -Path (Join-Path $deployRoot 'platforms') -Force | Out-Null
        Copy-Item $offscreen (Join-Path $deployRoot 'platforms') -Force
      }
    } catch {
      if ($Architecture -eq 'ARM64' -and $osArch -ne 'Arm64') {
        throw 'ARM64 build succeeded, but windeployqt could not run on this x64 host. Re-run with -SkipDeploy and perform deployment on a native Windows ARM64 machine, or use compatible Qt host deployment tooling.'
      }
      throw
    }
  } else {
    throw "windeployqt.exe not found under target Qt: $QtRoot"
  }
}

Copy-Item (Join-Path $projectRoot 'README_KO.md') $deployRoot -Force
Copy-Item (Join-Path $projectRoot 'README_FIRST.txt') $deployRoot -Force
Copy-Item (Join-Path $projectRoot 'Run_Windows_Verification.cmd') $deployRoot -Force
Copy-Item (Join-Path $projectRoot 'LICENSE') $deployRoot -Force
Copy-Item (Join-Path $projectRoot 'COPYRIGHT.txt') $deployRoot -Force
Copy-Item (Join-Path $projectRoot 'third_party') $deployRoot -Recurse -Force

# Every bundled EXE/DLL must match the release architecture. This catches an
# accidental x64 Qt DLL in an ARM64 bundle before native testing.
Invoke-Checked $python.Source @((Join-Path $PSScriptRoot 'verify_pe_machine.py'),$deployRoot,$Architecture,(Join-Path $validationRoot 'bundle_architecture.json'))

$binaryFiles = Get-ChildItem -Path $deployRoot -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') }
$manifest = [ordered]@{
  application = 'SParamView'
  version = $releaseVersion
  architecture = $Architecture
  compiler = $Compiler
  os_architecture = $osArch
  qt_root = (Resolve-Path $QtRoot).Path
  qt_host_root = if ($HostQtRoot) { (Resolve-Path $HostQtRoot).Path } else { $null }
  generated_utc = [DateTime]::UtcNow.ToString('o')
  binaries = @($binaryFiles | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($deployRoot.Length).TrimStart([char]'\'); bytes = $_.Length; sha256 = Get-FileSha256 $_.FullName }
  })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $validationRoot 'build_manifest.json') -Encoding UTF8
Write-Host "Ready: $deployRoot\SParamView.exe ($Architecture)"
Write-Host "Validation metadata: $validationRoot"
