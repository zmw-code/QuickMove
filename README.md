# 快捷式文件转移 (QuickMove) v1.0

Windows 资源管理器右键菜单里的“移动到…”小工具：把“复制 → 切换目录 → 粘贴 → 删除源文件”压缩成两次点击。

- 技术栈：C++17 + Win32 API（无第三方运行时、无 Qt、无后台服务，单文件 exe）
- 目标系统：Windows 10 (1903+) / Windows 11
- 开源协议：MIT

## 目录结构

    src/                源码（main / path_utils / config_store / folder_picker / file_move）
    build.cmd           一键编译（自动定位 MSVC，产物 build\QuickMove.exe）
    install.cmd         安装：复制 exe 到 %LOCALAPPDATA%\Programs\QuickMove 并注册右键菜单
    uninstall.reg       卸载：清理右键菜单注册表项
    LICENSE             MIT 协议

## 构建

双击或命令行运行 `build.cmd`，产物为 `build\QuickMove.exe`。

## 安装

双击 `install.cmd`。它只写入 `HKCU\Software\Classes`，**不需要管理员权限**，
`%LOCALAPPDATA%` 在脚本内展开为真实路径后写入注册表。

安装后菜单项：

- 任意文件：`HKCU\Software\Classes\*\shell\QuickMove`
- 任意文件夹：`HKCU\Software\Classes\Directory\shell\QuickMove`
- 名称：`移动到…`，单选模型：`MultiSelectModel=Single`

> Windows 11 首次使用时请点“显示更多选项”（或按 Shift+F10）展开经典菜单。

## 使用

1. 右键点击一个文件或文件夹 → “移动到…”
2. 在系统文件夹选择框中选择目标位置 → 确定
3. 成功后会提示“文件已移动至：<目标文件夹>”

下一次打开对话框时会自动定位到上一次移动成功的目标位置。

## 行为与限制（v1 范围）

- **仅支持单选**：多选时该系统项不可用；程序侧也会在收到多条路径时提示“v1 暂不支持批量移动”。
- **仅支持同一卷**：跨分区/跨盘（C: → D:、网络盘 → 本地盘等）直接报错，不做“复制+删除”降级。
- **同名不覆盖**：目标位置已存在同名对象时移动终止，不提供覆盖/重命名/跳过选项。
- **系统关键目录黑名单**：拒绝移动 `C:\Windows`、`C:\Program Files`、`C:\Program Files (x86)`、
  `C:\ProgramData`、盘符根、用户目录根、`%LOCALAPPDATA%`、`%APPDATA%` 等位置。
- **路径长度**：不支持超过 260 字符的路径。
- **不做**：批量处理、设置面板、收藏夹、拖拽移动、长路径支持。

## 配置

    %LOCALAPPDATA%\QuickMove\config.json

仅保存一个字段 `last_used_path`（上一次移动成功的目标目录）。写入采用
“先写 `config.tmp` → 原子替换”的方式；文件缺失或内容损坏时按“未记录”处理，
回退到“文档”目录。仅当移动成功后才会更新。

## 卸载

双击 `uninstall.reg`（删除上述两个菜单项）。程序本体与配置不会被删除，
如需彻底清理请手动删除 `%LOCALAPPDATA%\Programs\QuickMove` 与 `%LOCALAPPDATA%\QuickMove`。

## 退出码

    0  移动成功
    1  未执行（参数错误、用户取消、校验未通过）
    2  执行失败（移动失败或无法打开对话框）

## 风险提示（必读）

- **请勿移动已安装软件的目录**：移动 `C:\Program Files`、`C:\Program Files (x86)` 已被黑名单拦截，
  但安装在其它位置的软件目录仍可被移动。移动后会导致注册表路径失效、快捷方式失效、
  服务无法启动、无法卸载等后果。
- **暂不支持跨分区移动**：本版本仅支持同一磁盘内的快速移动。需要跨盘归档时请使用系统的复制/粘贴。
- **文件被占用**：移动正被其他程序打开的文件会失败，提示“文件正在使用，请关闭相关程序后重试”。

## 实现要点

- 移动：`MoveFileExW(src, dst, 0)`，不携带 `MOVEFILE_COPY_ALLOWED`，保证跨卷必然失败而不静默降级为复制；
  失败后用 `GetLastError()` 映射为具体中文原因。
- 跨卷判定：`PathIsSameRootW`，判定失败时不做任何移动动作。
- 目录选择：`IFileOpenDialog`（`FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST`），系统原生对话框。
- 路径校验：`GetFullPathNameW` + `GetLongPathNameW` 规范化，大小写不敏感比较，防止 `..` 与 8.3 短名绕过。
- 崩溃隔离：独立进程，不注入资源管理器。

## 许可证

MIT，见 LICENSE。