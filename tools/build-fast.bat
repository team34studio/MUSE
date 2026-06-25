@echo off
setlocal
rem === Incremental compile of the MUSE plugin (only changed files) ===
rem First run does a full compile and builds the Intermediate cache; every run
rem after that recompiles ONLY the files you changed (seconds, not minutes).
rem Use build.bat instead for a clean packaged build (e.g. before a release).
if not defined ENGINE set "ENGINE=E:\UE\UE_5.7"
set "ROOT=%~dp0.."
set "UPROJECT=%ROOT%\.dev\HostProject\HostProject.uproject"
set "BUILD=%ENGINE%\Engine\Build\BatchFiles\Build.bat"

if not exist "%BUILD%" (
  echo [ERROR] UE 5.7 not found at "%ENGINE%". Edit ENGINE at the top of this script.
  exit /b 1
)
if not exist "%UPROJECT%" (
  echo [ERROR] Dev host project not found at "%UPROJECT%".
  exit /b 1
)

rem Link the plugin into the host project (directory junction; no copy). UE finds
rem it as a normal project plugin, and Binaries/Intermediate land in the real
rem MUSE folder. Recreated automatically if missing (e.g. after a fresh clone).
set "LINK=%ROOT%\.dev\HostProject\Plugins\MUSE"
if not exist "%LINK%\MUSE.uplugin" (
  if not exist "%ROOT%\.dev\HostProject\Plugins" mkdir "%ROOT%\.dev\HostProject\Plugins"
  echo Linking plugin into host project ...
  mklink /J "%LINK%" "%ROOT%\MUSE" >nul
)

echo Incremental build of MUSE (HostProjectEditor) ...
call "%BUILD%" HostProjectEditor Win64 Development -Project="%UPROJECT%" -WaitMutex
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [OK] Build succeeded. Binaries -^> "%ROOT%\MUSE\Binaries\Win64"
  echo Next: python "%~dp0deploy.py"
) else (
  echo [FAIL] Build failed with code %RC% — see the log above.
)
endlocal & exit /b %RC%
