@echo off
setlocal
cd /d "%~dp0"
set "QA_DIR=%TEMP%\SIAnalyzer-QA-%RANDOM%-%RANDOM%"
mkdir "%QA_DIR%"
set "QA_FAILED=0"
set "QT_QPA_PLATFORM=offscreen"
for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "(Get-Item '%~dp0SParamView.exe').VersionInfo.ProductVersion"`) do set "SPARAMVIEW_VERSION=%%V"
if not defined SPARAMVIEW_VERSION set "SPARAMVIEW_VERSION=unknown"
echo SParamView %SPARAMVIEW_VERSION% Windows verification > "%QA_DIR%\results.txt"
ver >> "%QA_DIR%\results.txt"
for %%T in (si_tests si_mapping_tests si_mapping_policy_tests si_polarity_tests si_termination_tests si_performance_tests si_qt_tests) do (
  echo Running %%T
  "%%T.exe" >> "%QA_DIR%\results.txt" 2>&1
  if errorlevel 1 set "QA_FAILED=1"
)
"si_direction_tests.exe" "%QA_DIR%\direction" >> "%QA_DIR%\results.txt" 2>&1
if errorlevel 1 set "QA_FAILED=1"
"si_golden_tests.exe" "%QA_DIR%\golden" >> "%QA_DIR%\results.txt" 2>&1
if errorlevel 1 set "QA_FAILED=1"
for %%C in (snapshot cr_only false_noise valid_noise empty_tdr truncated_live_cache invalid_cached_frequency zero_transfer cancel_reopen parallel_import) do (
  "si_hardening_tests.exe" %%C "%QA_DIR%\hardening\%%C" >> "%QA_DIR%\results.txt" 2>&1
  if errorlevel 1 set "QA_FAILED=1"
)
"si_navigation_tests.exe" "%QA_DIR%\navigation" >> "%QA_DIR%\results.txt" 2>&1
if errorlevel 1 set "QA_FAILED=1"
"si_open_tests.exe" "%QA_DIR%\open" fallback >> "%QA_DIR%\results.txt" 2>&1
if errorlevel 1 set "QA_FAILED=1"
"si_workspace_tests.exe" "%~dp0examples\demo.siproject" "%QA_DIR%\workspace" 1366 768 >> "%QA_DIR%\results.txt" 2>&1
if errorlevel 1 set "QA_FAILED=1"
start "" /wait "SParamView.exe" --selftest "%~dp0examples" "%QA_DIR%\workflow"
if errorlevel 1 set "QA_FAILED=1"
if "%QA_FAILED%"=="0" (echo PASS >> "%QA_DIR%\results.txt") else (echo FAIL >> "%QA_DIR%\results.txt")
echo.
type "%QA_DIR%\results.txt"
echo.
echo Results are saved in: %QA_DIR%
echo This automated check does not replace visual DPI/driver or instrument correlation tests.
pause
exit /b %QA_FAILED%
