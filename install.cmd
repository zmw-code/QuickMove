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
rem 文件/文件夹动词为 Single：多选时右键项自动隐藏（Explorer 对经典静态动词按每文件
rem 并行派发独立进程，见 REQUIREMENTS 决策 D9）；批量统一走“空白处右键 → 批量移动…”（D10）
reg add "HKCU\Software\Classes\*\shell\QuickMove" /ve /t REG_SZ /d "移动到…" /f >nul
reg add "HKCU\Software\Classes\*\shell\QuickMove" /v "MultiSelectModel" /t REG_SZ /d "Single" /f >nul
reg add "HKCU\Software\Classes\*\shell\QuickMove\command" /ve /t REG_SZ /d "\"%EXE%\" \"%%1\"" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove" /ve /t REG_SZ /d "移动到…" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove" /v "MultiSelectModel" /t REG_SZ /d "Single" /f >nul
reg add "HKCU\Software\Classes\Directory\shell\QuickMove\command" /ve /t REG_SZ /d "\"%EXE%\" \"%%1\"" /f >nul
reg add "HKCU\Software\Classes\Directory\Background\shell\QuickMoveBatch" /ve /t REG_SZ /d "批量移动…" /f >nul
reg add "HKCU\Software\Classes\Directory\Background\shell\QuickMoveBatch\command" /ve /t REG_SZ /d "\"%EXE%\" /batch \"%%V\"" /f >nul
if errorlevel 1 (
  echo [错误] 写入注册表失败。
  pause
  exit /b 1
)

echo.
echo 安装完成：%EXE%
echo 右键菜单已注册；Windows 11 请在“显示更多选项”中查看。
echo 卸载请运行 uninstall.cmd。
echo.
pause
endlocal