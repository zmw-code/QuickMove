@echo off
chcp 936 >nul
setlocal
cd /d "%~dp0"

set "TARGET_DIR=%LOCALAPPDATA%\Programs\QuickMove"
set "EXE=%TARGET_DIR%\QuickMove.exe"
set "SRC=%~dp0build\QuickMove.exe"
if not exist "%SRC%" set "SRC=%~dp0QuickMove.exe"
if not exist "%SRC%" (
  echo [错误] 未找到 QuickMove.exe，请先运行 build.cmd 编译。
  pause
  exit /b 1
)

if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
copy /y "%SRC%" "%EXE%" >nul
if errorlevel 1 (
  echo [错误] 复制 QuickMove.exe 到 "%TARGET_DIR%" 失败。
  pause
  exit /b 1
)

rem 仅写入 HKCU，无需管理员权限；%LOCALAPPDATA% 在此处展开为真实路径
rem MultiSelectModel=Player：允许多选，一次批量移动（重跑本脚本即可从 v1.0 升级）
reg add "HKCU\Software\Classes\*\shell\QuickMove" /ve /t REG_SZ /d "移动到…" /f >nul
reg add "HKCU\Software\Classes\*\shell\QuickMove" /v "MultiSelectModel" /t REG_SZ /d "Player" /f >nul
reg add "HKCU\Software\Classes\*\shell\QuickMove\command" /ve /t REG_SZ /d "\"%EXE%\" \"%%1\"" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove" /ve /t REG_SZ /d "移动到…" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove" /v "MultiSelectModel" /t REG_SZ /d "Player" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove\command" /ve /t REG_SZ /d "\"%EXE%\" \"%%1\"" /f >nul
if errorlevel 1 (
  echo [错误] 写入注册表失败。
  pause
  exit /b 1
)

echo.
echo 安装完成：%EXE%
echo 右键菜单已注册；Windows 11 请在“显示更多选项”中查看。
echo 卸载请双击 uninstall.reg。
echo.
pause
endlocal