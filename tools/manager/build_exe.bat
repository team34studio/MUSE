@echo off
setlocal
rem === Package MUSE Manager into a single-file .exe (PyInstaller) ===
rem   pip install pyinstaller   (one time)
rem
rem If a "prebuilt\MUSE" folder exists next to this script (a full plugin folder
rem with Binaries + Source), it gets bundled so end users can install without
rem any engine or source. For your own dev use you can skip that — the exe will
rem just deploy from your local build instead.

cd /d "%~dp0"

rem Pick a Python launcher (prefer the py launcher, fall back to python).
set "PY=python"
where py >nul 2>nul && set "PY=py"

%PY% -c "import PyInstaller" >nul 2>nul
if errorlevel 1 (
  echo [ERROR] PyInstaller not found for %PY%. Install it with:  %PY% -m pip install pyinstaller
  exit /b 1
)

set "ADDDATA="
if exist "prebuilt\MUSE\MUSE.uplugin" (
  echo Bundling prebuilt plugin from prebuilt\MUSE
  set "ADDDATA=--add-data prebuilt\MUSE;MUSE"
) else (
  echo No prebuilt\MUSE found — building a dev exe (deploys from your local build^).
)

set "ICON="
if exist "muse.ico" set "ICON=--icon muse.ico"

%PY% -m PyInstaller --noconfirm --onefile --windowed --name "MUSE Manager" %ICON% %ADDDATA% muse_manager.py
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [OK] Built -^> "%~dp0dist\MUSE Manager.exe"
) else (
  echo [FAIL] PyInstaller failed with code %RC%.
)
endlocal
exit /b %RC%
