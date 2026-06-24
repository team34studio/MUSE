@echo off
setlocal
rem === Compile the MUSE plugin against vanilla UE 5.7 (produces Binaries) ===
set "ENGINE=E:\UE\UE_5.7"
set "ROOT=%~dp0.."
set "PLUGIN=%ROOT%\MUSE\MUSE.uplugin"
set "OUT=%ROOT%\build\MUSE"

if not exist "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" (
  echo [ERROR] UE 5.7 not found at "%ENGINE%". Edit ENGINE at the top of this script.
  exit /b 1
)
if not exist "%PLUGIN%" (
  echo [ERROR] Plugin descriptor not found at "%PLUGIN%".
  exit /b 1
)

if exist "%OUT%" (
  echo Cleaning previous output "%OUT%" ...
  rmdir /s /q "%OUT%"
)

echo Building MUSE against "%ENGINE%" (first build can take 10-20 min) ...
call "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%" -Package="%OUT%" -TargetPlatforms=Win64
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [OK] Build succeeded -^> "%OUT%"
  echo Next: python "%~dp0deploy.py"
) else (
  echo [FAIL] Build failed with code %RC% — see the log above.
)
endlocal
exit /b %RC%
