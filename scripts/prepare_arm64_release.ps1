param(
  [Parameter(Mandatory=$true)][string]$QtRoot,
  [string]$HostQtRoot = $env:QT_HOST_PATH,
  [string]$SamplesDir = ""
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$osArch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$buildRoot = Join-Path $projectRoot 'build-windows-arm64'
$bundleRoot = Join-Path $projectRoot 'dist\SParamView-Windows-ARM64'
$validationRoot = Join-Path $projectRoot 'validation-windows-arm64'
New-Item -ItemType Directory -Path $validationRoot -Force | Out-Null

$buildScript = Join-Path $PSScriptRoot 'build_windows.ps1'
$buildArgs = @{ QtRoot = $QtRoot; Compiler = 'MSVC'; Architecture = 'ARM64' }
if ($HostQtRoot) { $buildArgs.HostQtRoot = $HostQtRoot }
# Native validation runs CTest once after deployment; cross builds cannot run it.
$buildArgs.SkipTests = $true
& $buildScript @buildArgs

$nativeValidated = $false
$releaseReady = $false
$releaseBlockers = @('Windows ARM64 native execution is required.')
$nativeReport = $null
if ($osArch -eq 'Arm64') {
  if (-not $SamplesDir) { throw 'Native ARM64 build succeeded, but -SamplesDir is required for the final seven-file release validation.' }
  $nativeOut = Join-Path $validationRoot 'native'
  if (Test-Path $nativeOut) { Remove-Item $nativeOut -Recurse -Force }
  python (Join-Path $PSScriptRoot 'validate_arm64_native.py') --build $buildRoot --bundle $bundleRoot --samples $SamplesDir --output $nativeOut
  if ($LASTEXITCODE -ne 0) { throw 'Native ARM64 validation failed' }
  $nativeReport = Join-Path $nativeOut 'ARM64_Native_Validation.json'
  $nativeResult = Get-Content -LiteralPath $nativeReport -Raw | ConvertFrom-Json
  if ($nativeResult.passed -ne $true) { throw 'Native ARM64 report did not pass all required validation gates.' }
  $nativeValidated = $true
  $releaseReady = ($nativeResult.release_ready -eq $true)
  $releaseBlockers = @($nativeResult.release_blockers)
} else {
  Write-Warning 'This is an x64 Windows host. ARM64 was cross-built and statically audited, but native ARM64 execution is still required before final release.'
}

$deliverables = Join-Path $projectRoot 'deliverables-arm64'
New-Item -ItemType Directory -Path $deliverables -Force | Out-Null
$label = if ($releaseReady) { 'SParamView_Windows_ARM64_v1.1.2' } else { 'SParamView_Windows_ARM64_v1.1.2_Candidate' }
$zip = Join-Path $deliverables ($label + '.zip')
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $bundleRoot -DestinationPath $zip -CompressionLevel Optimal
$zipSha = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()

$status = [ordered]@{
  application = 'SParamView'
  version = '1.0.5'
  target = 'Windows ARM64'
  host_os_architecture = $osArch
  native_validated = $nativeValidated
  release_ready = $releaseReady
  release_blockers = $releaseBlockers
  package = $zip
  package_sha256 = $zipSha
  native_validation_report = $nativeReport
  note = if ($releaseReady) { 'All required native validation and final release gates passed.' } elseif ($nativeValidated) { 'Native automated checks passed. Candidate only; final release checks listed in release_blockers remain incomplete.' } else { 'Cross-build candidate only. Native Windows ARM64 validation is still required.' }
}
$statusPath = Join-Path $deliverables 'ARM64_Release_Status.json'
$status | ConvertTo-Json -Depth 6 | Set-Content $statusPath -Encoding UTF8
$status | ConvertTo-Json -Depth 6
Write-Host "Package: $zip"
Write-Host "SHA-256: $zipSha"
