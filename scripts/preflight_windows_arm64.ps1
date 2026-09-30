param(
  [Parameter(Mandatory=$true)][string]$QtRoot,
  [string]$HostQtRoot = $env:QT_HOST_PATH,
  [string]$Report = ""
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Get-PEMachine([string]$Path) {
  $stream = [System.IO.File]::OpenRead($Path)
  try {
    $reader = New-Object System.IO.BinaryReader($stream)
    if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not an MZ executable: $Path" }
    $stream.Position = 0x3C
    $peOffset = $reader.ReadUInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) { throw "Missing PE signature: $Path" }
    return $reader.ReadUInt16()
  } finally { $stream.Dispose() }
}

function Require-Command([string]$Name) {
  $c = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $c) { throw "Required command not found: $Name" }
  return $c.Source
}

function Get-QtVersion([string]$Root) {
  $qconfig = Join-Path $Root 'mkspecs\qconfig.pri'
  if (Test-Path $qconfig) {
    $m = Select-String -Path $qconfig -Pattern '^QT_VERSION\s*=\s*(.+)$' | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value.Trim() }
  }
  return $null
}

$checks = [ordered]@{}
$checks['os_architecture'] = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$checks['process_architecture'] = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
$checks['cmake'] = Require-Command 'cmake'
$checks['python'] = Require-Command 'python'
$checks['qt_root'] = (Resolve-Path $QtRoot).Path

$qtConfig = Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'
$qtCore = Join-Path $QtRoot 'bin\Qt6Core.dll'
if (-not (Test-Path $qtConfig)) { throw "Qt6Config.cmake not found: $qtConfig" }
if (-not (Test-Path $qtCore)) { throw "Qt6Core.dll not found: $qtCore" }
$targetMachine = Get-PEMachine $qtCore
if ($targetMachine -ne 0xAA64) { throw ("Target Qt is not ARM64. Qt6Core.dll machine=0x{0:X4}; expected 0xAA64" -f $targetMachine) }
$checks['target_qt_machine'] = ('0x{0:X4}' -f $targetMachine)
$checks['target_qt_version'] = Get-QtVersion $QtRoot
if ($checks['target_qt_version'] -ne '6.8.3') { throw "SParamView 1.1.2 ARM64 release is pinned to Qt 6.8.3 so runtime, notices, and included corresponding source stay aligned. Found: $($checks['target_qt_version'])" }

if ($HostQtRoot) {
  if (-not (Test-Path $HostQtRoot)) { throw "Host Qt path does not exist: $HostQtRoot" }
  $hostCore = Join-Path $HostQtRoot 'bin\Qt6Core.dll'
  if (-not (Test-Path $hostCore)) { throw "Host Qt6Core.dll not found: $hostCore" }
  $hostMachine = Get-PEMachine $hostCore
  if ($hostMachine -notin @(0x8664, 0xAA64)) { throw ("Unsupported host Qt architecture: 0x{0:X4}" -f $hostMachine) }
  $checks['host_qt_root'] = (Resolve-Path $HostQtRoot).Path
  $checks['host_qt_machine'] = ('0x{0:X4}' -f $hostMachine)
  $checks['host_qt_version'] = Get-QtVersion $HostQtRoot
  if ($checks['target_qt_version'] -and $checks['host_qt_version'] -and $checks['target_qt_version'] -ne $checks['host_qt_version']) {
    throw "Qt host/target versions differ: target=$($checks['target_qt_version']) host=$($checks['host_qt_version'])"
  }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
  $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath
  if (-not $vsPath) { throw 'Visual Studio installation with the ARM64 C++ tools was not found. Install Desktop development with C++ and MSVC ARM64 build tools.' }
  $checks['visual_studio'] = $vsPath.Trim()
  $checks['arm64_build_tools'] = $true
} else {
  $checks['visual_studio'] = 'vswhere.exe not found; CMake configure will perform the final compiler check.'
  $checks['arm64_build_tools'] = 'unverified'
}

$cmakeText = Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw
$coreText = Get-Content (Join-Path $projectRoot 'include\si\core.hpp') -Raw
$rcText = Get-Content (Join-Path $projectRoot 'assets\SParamView.rc') -Raw
$cmakeVersion = [regex]::Match($cmakeText, 'project\(SParamView VERSION ([0-9.]+)').Groups[1].Value
$coreVersion = [regex]::Match($coreText, 'version\s*=\s*"([0-9.]+)"').Groups[1].Value
$rcVersion = [regex]::Match($rcText, 'VALUE "ProductVersion", "([0-9.]+)').Groups[1].Value
if (-not $cmakeVersion -or $cmakeVersion -ne $coreVersion -or $cmakeVersion -ne $rcVersion) {
  throw "Application version mismatch: CMake=$cmakeVersion core=$coreVersion resource=$rcVersion"
}
$checks['application_version'] = $cmakeVersion

$drive = Get-PSDrive -Name ([System.IO.Path]::GetPathRoot($projectRoot).Substring(0,1)) -ErrorAction SilentlyContinue
if ($drive) {
  $freeGB = [math]::Round($drive.Free / 1GB, 2)
  $checks['free_disk_gb'] = $freeGB
  if ($freeGB -lt 2) { throw "Less than 2 GB free on project drive ($freeGB GB)." }
}

$result = [ordered]@{ passed = $true; checks = $checks }
if ($Report) {
  $parent = Split-Path -Parent $Report
  if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
  $result | ConvertTo-Json -Depth 6 | Set-Content -Path $Report -Encoding UTF8
}
$result | ConvertTo-Json -Depth 6
Write-Host 'ARM64 preflight PASS'
