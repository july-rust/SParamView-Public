@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
if not exist "si_hardening_diagnostics.exe" (
  echo Missing si_hardening_diagnostics.exe. Extract the whole diagnostic ZIP first.
  pause
  exit /b 2
)
:choose_output
set "DIAG_DIR=%TEMP%\SParamView-Cache-Diagnostics-%RANDOM%-%RANDOM%"
if exist "%DIAG_DIR%" goto choose_output
mkdir "%DIAG_DIR%"
if errorlevel 1 (
  echo Cannot create diagnostic output folder.
  pause
  exit /b 2
)
set "DIAG_LOG=%DIAG_DIR%\results.txt"
echo SParamView ARM64 cache diagnostics only > "%DIAG_LOG%"
echo Started: %DATE% %TIME% >> "%DIAG_LOG%"
ver >> "%DIAG_LOG%"
echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE% >> "%DIAG_LOG%"
echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432% >> "%DIAG_LOG%"
if exist "BUILD_SOURCE.txt" type "BUILD_SOURCE.txt" >> "%DIAG_LOG%"

echo Running snapshot...
echo. >> "%DIAG_LOG%"
echo [snapshot] >> "%DIAG_LOG%"
"si_hardening_diagnostics.exe" snapshot "%DIAG_DIR%\snapshot" >> "%DIAG_LOG%" 2>&1
set "SNAPSHOT_EXIT=%ERRORLEVEL%"
echo ExitCode=%SNAPSHOT_EXIT% >> "%DIAG_LOG%"

echo Running parallel_import...
echo. >> "%DIAG_LOG%"
echo [parallel_import] >> "%DIAG_LOG%"
"si_hardening_diagnostics.exe" parallel_import "%DIAG_DIR%\parallel_import" >> "%DIAG_LOG%" 2>&1
set "PARALLEL_EXIT=%ERRORLEVEL%"
echo ExitCode=%PARALLEL_EXIT% >> "%DIAG_LOG%"

set "DIAG_FAILED=0"
if not "%SNAPSHOT_EXIT%"=="0" set "DIAG_FAILED=1"
if not "%PARALLEL_EXIT%"=="0" set "DIAG_FAILED=1"
echo. >> "%DIAG_LOG%"
echo snapshot exit=%SNAPSHOT_EXIT%, parallel_import exit=%PARALLEL_EXIT% >> "%DIAG_LOG%"
echo Diagnostic only. This is not the full release verification. >> "%DIAG_LOG%"
type "%DIAG_LOG%"
echo.
echo Send this file: %DIAG_LOG%
pause
exit /b %DIAG_FAILED%
