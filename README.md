# 快捷式文件转移 (QuickMove) v1.1

Windows 资源管理器右键菜单里的“移动到…”小工具：把“复制 → 切换目录 → 粘贴 → 删除源文件”压缩成两次点击。

- 技术栈：C++17 + Win32 API（无第三方运行时、无 Qt、无后台服务，单文件 exe）
- 目标系统：Windows 10 (1903+) / Windows 11
- 开源协议：MIT

## 目录结构

    src/                源码（main / path_utils / config_store / folder_picker / file_move / error_report）
    tests/              自测程序（build.cmd 自动编译并运行，失败即中断）
    build.cmd           一键编译（自动定位 MSVC，产物 build\QuickMove.exe，附 SHA256）
    install.cmd         安装：复制 exe 到 %LOCALAPPDATA%\Programs\QuickMove 并注册右键菜单
    uninstall.cmd       卸载：清理右键菜单注册表项
    REQUIREMENTS.md     需求分析与路线图（FR-01 ~ FR-10）
    HANDOVER.md         交接存档（诊断证据与决策）
    LICENSE             MIT 协议

## 构建

双击或命令行运行 `build.cmd`，产物为 `build\QuickMove.exe`，同时：

- 生成 `build\checksum.txt`（QuickMove.exe 的 SHA256）
- 编译并运行 `tests\selftest.exe` 自动测试，全部通过才算构建成功

## 安装

双击 `install.cmd`。它只写入 `HKCU\Software\Classes`，**不需要管理员权限**，
`%LOCALAPPDATA%` 在脚本内展开为真实路径后写入注册表。

安装后菜单项：

- 任意文件：`HKCU\Software\Classes\*\shell\QuickMove`，名称“移动到…”（单选模型）
- 任意文件夹：`HKCU\Software\Classes\Directory\shell\QuickMove`，名称“移动到…”（单选模型）
- 文件夹空白处：`HKCU\Software\Classes\Directory\Background\shell\QuickMoveBatch`，名称“批量移动…”

> Windows 11 首次使用时请点“显示更多选项”（或按 Shift+F10）展开经典菜单。

## 使用

**移动单个文件/文件夹**：

1. 右键点击一个文件或文件夹 → “移动到…”
2. 在系统文件夹选择框中选择目标位置 → 确定
3. 成功后提示“文件已移动至：<目标文件夹>”

**批量移动**：

1. 在文件夹窗口的**空白处**右键 → “批量移动…”
2. 在弹出的系统文件对话框中多选要移动的项目（按住 Ctrl 点选、框选均可，文件和文件夹都支持）
3. 打开 → 选择目标文件夹 → 确定
4. 结果反馈与单选相同；存在失败时弹出失败列表（含“复制失败信息”按钮），成功项不受影响

下一次打开目标选择框时会自动定位到上一次移动成功的目标位置。

### 为什么多选文件时右键没有“移动到…”？

Windows 资源管理器对经典右键菜单动词是**每个选中文件启动一个独立进程**（实测 Win11 26200），
无法在一次调用里拿到全部所选路径。因此本工具不在多选右键上做批量（该场景下菜单项自动隐藏），
而是提供“批量移动…”入口：在原生文件对话框里完成多选，一次进程处理全部所选项目，
天然支持任意数量、无并发问题。

### 批量移动的边界行为

- 混合选择“父文件夹 + 其子项”时按深路径优先处理：子项先移出，父项随后整体移动，两者都成功
- 多个不同目录的同名文件移入同一目标时，先到者成功，后者计入失败列表
- 用户取消任一对话框时**零改动**：不移动任何项、不更新位置记忆

## 下载校验（SHA256）

Release 页与 `build.cmd` 均输出 `checksum.txt`。校验方法（PowerShell）：

    Get-FileHash .\QuickMove.exe -Algorithm SHA256

将结果与 `checksum.txt` 中的哈希比对，一致说明文件完整未被篡改。

## 行为与限制（v1.1 范围）

- **批量移动**：经“批量移动…”入口，一次移动任意数量的项目（v1.1 新增）。
- **仅支持同一卷**：跨分区/跨盘（C: → D:、网络盘 → 本地盘等）的项会被明确拒绝并在失败列表中
  说明“跨卷移动将于 v1.2 支持”，不做“复制+删除”静默降级。
- **同名不覆盖**：目标位置已存在同名对象时该项跳过并计入失败列表，不提供覆盖/重命名选项。
- **系统关键目录黑名单**：拒绝移动 `C:\Windows`、`C:\Program Files`、`C:\Program Files (x86)`、
  `C:\ProgramData`、盘符根、用户目录根、`%LOCALAPPDATA%`、`%APPDATA%` 等位置。
- **路径长度**：不支持超过 260 字符的路径。
- **不做**：跨卷移动（v1.2）、设置面板、收藏夹、拖拽移动、长路径支持。

## 配置

    %LOCALAPPDATA%\QuickMove\config.json

仅保存一个字段 `last_used_path`（上一次移动成功的目标目录）。写入采用
“先写 `config.tmp` → 原子替换”的方式；文件缺失或内容损坏时按“未记录”处理，
回退到“文档”目录。仅当批量中至少一项移动成功后才更新。

## 卸载

双击 `uninstall.cmd`（删除上述三个菜单项，仅 HKCU，无需管理员权限）。程序本体与配置不会被删除，
如需彻底清理请手动删除 `%LOCALAPPDATA%\Programs\QuickMove` 与 `%LOCALAPPDATA%\QuickMove`。

## 退出码

    0  全部移动成功
    1  未执行（参数错误、用户取消、校验未通过，含全部项被拒）
    2  执行失败（有项进入移动后失败或无法打开对话框；部分成功也返回 2）

## 风险提示（必读）

- **请勿移动已安装软件的目录**：移动 `C:\Program Files`、`C:\Program Files (x86)` 已被黑名单拦截，
  但安装在其它位置的软件目录仍可被移动。移动后会导致注册表路径失效、快捷方式失效、
  服务无法启动、无法卸载等后果。
- **暂不支持跨分区移动**：本版本仅支持同一磁盘内的快速移动。需要跨盘归档时请使用系统的复制/粘贴。
- **文件被占用**：移动正被其他程序打开的文件会失败，该项计入失败列表，
  提示“文件正在使用，请关闭相关程序后重试”，其余项继续。

## 实现要点

- 移动：`MoveFileExW(src, dst, 0)`，不携带 `MOVEFILE_COPY_ALLOWED`，保证跨卷必然失败而不静默降级为复制；
  失败后用 `GetLastError()` 映射为具体中文原因。
- 跨卷判定：`PathIsSameRootW`，判定失败时不做任何移动动作。
- 批量入口：`Directory\Background\shell` 动词以 `/batch "%V"` 启动 → `IFileOpenDialog`
  （`FOS_ALLOWMULTISELECT`，文件模式）多选源 → 与单选共用同一条“预校验/去重/深路径优先/
  逐项校验/失败汇总”管线（决策 D10）。
- 多选右键隐藏：文件/文件夹动词保持 `MultiSelectModel=Single`，多选时菜单项不出现，
  规避“每文件一个进程”的派发模型（决策 D9）。
- 失败列表：纯 Win32 内存 `DLGTEMPLATE` 对话框（无资源脚本依赖），只读明细 +
  “复制失败信息”写 `CF_UNICODETEXT` 剪贴板；模板创建失败回退为截断的消息框。
- 目录选择：`IFileOpenDialog`（`FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST`），系统原生对话框。
- 路径校验：`GetFullPathNameW` + `GetLongPathNameW` 规范化，大小写不敏感比较，防止 `..` 与 8.3 短名绕过。
- 防死锁：全程单线程无锁；剪贴板操作有限次重试后放弃；移动失败不做等待重试循环，立即返回并汇总。
- 崩溃隔离：独立进程，不注入资源管理器。

## 测试

`build.cmd` 自动编译并运行 `tests\selftest.exe`（覆盖参数解析、路径规范化、黑名单、
跨卷判定、目标校验、同卷移动、配置读写、去重/深路径排序等 45 项断言）。
跨卷移动用例需要传入一个其它卷上的源路径作为命令行参数，日常构建自动跳过该用例。

## 许可证

MIT，见 LICENSE。
