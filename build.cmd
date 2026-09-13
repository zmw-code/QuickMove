@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ---- locate MSVC x64 toolchain ----
set "VCVARS="
for %%P in (
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "D:\VS\VS2026\VC\Auxiliary\Build\vcvars64.bat"
  "D:\VS\VS2026BASE\VC\Auxiliary\Build\vcvars64.bat"
) do (
  if not defined VCVARS if exist "%%~P" set "VCVARS=%%~P"
)

if not defined VCVARS (
  echo [ERROR] MSVC toolchain not found. Please install Visual Studio C++ build tools.
  exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
  echo [ERROR] failed to initialize MSVC environment.
  exit /b 1
)

if not exist "build" mkdir "build"

echo [INFO] compiling QuickMove.exe ...
cl /nologo /std:c++17 /utf-8 /W4 /O2 /GL /MT /DNDEBUG /EHsc /DUNICODE /D_UNICODE ^
   src\*.cpp /Fobuild\ /Fdbuild\ /Fe:build\QuickMove.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:src\QuickMove.manifest
if errorlevel 1 (
  echo [ERROR] build failed.
  exit /b 1
)

echo [OK] build\QuickMove.exe
endlocal