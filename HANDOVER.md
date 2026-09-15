# QuickMove 交接存档（2026-09-15）

> 本文由会话中断前写入：记录 v1.1 批量移动的交付状态、多选 bug 的诊断证据与已定修复方案。
> 接手者从这里继续即可，不需要重新排查。

## 1. 仓库状态（已提交，干净）

| 提交 | 内容 | 状态 |
| --- | --- | --- |
| `132840f` | docs: v1.1 使用文档 | ✅ |
| `23bb8ad` | feat: FR-05 批量移动主体 | ✅ |
| `e15404d` | feat: FR-05 失败列表对话框模块 | ✅ |
| `fe4807b` | chore: 阶段0收尾（tests/ 入库 + SHA256 输出） | ✅ |
| `c0d50ae` | docs: REQUIREMENTS.md 需求分析 | ✅ |

- `build.cmd` 全绿：主程序编译零警告，`tests\selftest.exe` 45 项断言全 PASS。
- 用户机器已安装（install.cmd 已重跑）：注册表两键均 `MultiSelectModel=Player`，`%LOCALAPPDATA%\Programs\QuickMove\QuickMove.exe` 为最新构建。
- 未跟踪：`file_easy_transfer.txt`（需求缺口原始清单，随本提交入库）、`.zcode/`（工作区元数据，勿提交）。

## 2. 未决 bug（用户实测报告）

**现象**：资源管理器里 Ctrl 多选 3 个文件 → 右键"移动到…" → 选目标后，只移动了光标最后指向的那一个文件，多选不生效。

## 3. 诊断结论（实机证据，勿再重复排查）

实测环境：Windows 11 build 26200，`HKCU\Software\Classes\*\shell\QuickMove` 与 `Directory\shell\QuickMove` 均为 `MultiSelectModel=Player`。

用临时探测动词（cmd echo 写文件）经真实右键菜单路径（3 文件多选 → 显示更多选项 → 触发动词）验证：

1. `Player` 模式下经典静态动词**不会**把 N 个路径合并进一个命令行（与网上文档的描述不符）。
2. Shell 的实际派发方式：**每个选中文件启动一个独立 QuickMove 进程，各自只带一个路径参数**。
3. 三个进程的启动时间戳：`0:03:29.19 / .24 / .28` —— **并行启动，互不等待**（90ms 内全部拉起）。

**根因**：v1.1 的批量实现假设"单次调用收到 argv[1..N]"（`parseSourceArguments` 支持 N 参数本身没错，但 Shell 根本不这么传），因此每次调用只处理一个文件——即用户看到的现象。

**附带发现（重要，影响阶段 3）**：在本 build 上，Shift+F10 打开的是 **Win11 新菜单**（内含"显示更多选项"），经典菜单必须再点一次才展开；经典静态动词不出现在新菜单中。这佐证了 FR-09（MSIX + IExplorerCommand）的必要性。

## 4. 已定修复方案：实例间聚合（batch_ipc）——未编码

> **2026-09-15 更新：本方案已被取代，不实施。** 补充探测证实 `MultiSelectModel=Single`
> 在多选时会隐藏右键菜单项（用户人工确认），因此批量入口改走“文件夹空白处右键 →
> 批量移动…（/batch 参数）+ 原生文件对话框多选”，见 REQUIREMENTS 决策 D10。
> 以下聚合设计仅作归档备用（若未来需要支持“多选文件直接右键”再启用）。

利用"并行启动"特性做单实例聚合（与用户口头确认过的方向一致）：

- **角色判定**：进程启动解析完参数后，`CreateMutexW(nullptr, TRUE, L"Local\\QuickMove.BatchCoordinator")`；`ERROR_ALREADY_EXISTS` → 自己是**转发者**；否则是**协调者**。
- **转发者**：`FindWindowExW(HWND_MESSAGE, NULL, L"QuickMove.BatchWnd", NULL)` 找协调者的消息窗口（重试 ~10×30ms 覆盖创建竞态），把自身路径逐条 `SendMessageTimeoutW(WM_COPYDATA, 2000ms, SMTO_ABORTIFHUNG)` 发送后 `return 0` 退出。任一步失败 → 降级为独立运行（走完整批量流程），绝不挂起。
- **协调者**：创建互斥体后注册窗口类并建 `HWND_MESSAGE` 消息窗口；窗口过程收到 `WM_COPYDATA`（`dwData==1`，payload 为 UTF-16 路径含结尾 NUL）仅做入队（`std::vector<std::wstring> g_forwarded`，同线程访问，无需锁）。
- **主流程接入点（main.cpp，共 3 处）**：
  1. 参数解析后、预校验前：`pumpForwardedPaths(150ms 空闲, 1500ms 上限)` 后把队列并入 `rawSources`（单选仅多付 ~0.15s）；
  2. `pickFolder` 返回之后：`takeForwardedPaths()` 非阻塞收割对话框期间到达的转发，逐条 `validateSource` 补校验并入 `pending`，再 `dedupePaths`（用户取消时丢弃不并入）；
  3. 退出前无需显式清理（互斥体随进程释放；窗口随进程销毁）。
- **防死锁要点**：全程单线程无锁；转发用 `SendMessageTimeout` 有界等待，超时即降级；窗口过程零阻塞；`pump` 用 `GetTickCount64()` + `Sleep(10)` 有界循环。
- **兼容性**：若其它 Windows 版本真的合并传参（单次调用 N 参数），则只有协调者一个实例、`rawSources` 直接含 N 条路径——两种 Shell 行为都正确工作，`parseSourceArguments` 保持不变。
- **已知边界（接受并记录）**：在汇总对话框弹出期间才到达的"迟来转发"会被丢弃（转发者已退出且无 UI）。发生窗口为毫秒级，概率可忽略。
- 新模块建议名：`src/batch_ipc.h/.cpp`；selftest 为控制台程序不覆盖 IPC，依赖 E2E 手测。

## 5. 修复后验证清单

1. `build.cmd` 全绿（含 selftest）。
2. E2E（真机 Explorer，参照本次探测的操作序列）：3 文件多选 → 显示更多选项 → 移动到… → **应只出现一个**目标选择框 → 选定后提示"已移动 3 项至…"。
3. 单选回归：单文件仍秒开对话框（~0.15s 延迟可接受）。
4. 取消回归：多选后取消对话框 → 零改动。
5. 降级路径：手动结束协调者进程（任务管理器）后再多选 → 各实例独立弹框（旧行为），无挂死。
6. 更新 README"实现要点"与 REQUIREMENTS 决策记录（新增 D9：静态动词每文件并行派发的实测结论与聚合方案）。

## 6. 环境坑备忘（本机自动化排查经验）

- Git Bash 里 `reg query ... /s` 会被 MSYS 参数转换弄坏（报"无效语法"或找不到键）；查 `*` 类键用 PowerShell：`Get-ItemProperty -LiteralPath 'HKCU:\Software\Classes\*\shell\QuickMove'`。
- PowerShell 对含 `*` 的注册表路径，`Test-Path`/`New-Item -Path` 会当通配符展开（枚举整个 HKCR，表现为"卡死"）；必须 `-LiteralPath` 或用 `[Microsoft.Win32.Registry]` API。
- 命令行模板里 `%TIME%` 会被 Shell 模板解析吃掉（`%T` 被当占位符）；要传时间戳用 cmd 延迟扩展 `!TIME!`（`cmd /v:on`）。
- Explorer 前台检测偶发抖动导致键盘注入被拒；`WScript.Shell.SendKeys`（配合 SetForegroundWindow/AppActivate）是可靠后备。经典菜单是独立弹出窗，a11y 树里常不可见，需截图后按坐标点击。
- 诊断序列（可复用）：Shift+F10 → 新菜单 → 点"显示更多选项" → 经典菜单 → 目标项坐标点击。

## 7. 遗留事项

- [x] 探测残留已清理：注册表 `QMProbe` 键已删（`exists=False`）、`build/probe_*` 与 `probe.vbs` 已删。
- [ ] 桌面上可能残留一个打开的 `probe_files` 资源管理器窗口（目录已删），手动关闭即可。
- [ ] `codegraph sync`：本机未找到 codegraph CLI，5 个提交未同步索引；安装后补跑（pw 命令已交付用户）。
- [ ] 修复实现（§4）→ 验证（§5）→ README/REQUIREMENTS 同步 → 提交。
- [ ] v1.2（FR-06a 单文件跨卷）按 REQUIREMENTS 继续推进。
