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

rem ---- SHA256 checksum (Release 页校验用) ----
echo [INFO] SHA256 ...
if exist "build\checksum.txt" del "build\checksum.txt"
> "build\checksum.txt" echo QuickMove.exe SHA256
certutil -hashfile "build\QuickMove.exe" SHA256 | findstr /v /i /c:"CertUtil" /c:"hash of" >> "build\checksum.txt"
type "build\checksum.txt"

rem ---- selftest（随仓库入库的自动测试，失败即中断） ----
if exist "tests\selftest.cpp" (
  echo [INFO] compiling selftest.exe ...
  cl /nologo /std:c++17 /utf-8 /W4 /O2 /MT /EHsc /DUNICODE /D_UNICODE /Isrc ^
     tests\selftest.cpp src\path_utils.cpp src\file_move.cpp tests\config_store_redirect.cpp ^
     /Fobuild\ /Fdbuild\ /Fe:build\selftest.exe /link /SUBSYSTEM:CONSOLE
  if errorlevel 1 (
    echo [ERROR] selftest build failed.
    exit /b 1
  )
  echo [INFO] running selftest ...
  build\selftest.exe
  if errorlevel 1 (
    echo [ERROR] selftest FAILED.
    exit /b 1
  )
)

endlocal