@echo off
chcp 936 >nul
rem 清理 QuickMove 的右键菜单注册表项（仅 HKCU，无需管理员权限）
reg delete "HKCU\Software\Classes\*\shell\QuickMove" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Directory\shell\QuickMove" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Directory\Background\shell\QuickMoveBatch" /f >nul 2>&1
echo.
echo 已清理 QuickMove 的右键菜单项（移动到… / 批量移动…）。
echo 程序本体与配置不会被删除；如需彻底清理请手动删除：
echo   %LOCALAPPDATA%\Programs\QuickMove
echo   %LOCALAPPDATA%\QuickMove
echo.
pause
