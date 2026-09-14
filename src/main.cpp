// 快捷式文件转移 (QuickMove) v1.1
// 需求：REQUIREMENTS.md（FR-01 ~ FR-05，v1.1 批量移动）

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <vector>

#include "config_store.h"
#include "error_report.h"
#include "file_move.h"
#include "folder_picker.h"
#include "path_utils.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")

using namespace quickmove;

namespace {

const wchar_t kAppTitle[] = L"快捷式文件转移";

// 退出码
const int kExitSuccess = 0;      // 全部移动成功
const int kExitNotExecuted = 1;  // 未执行（参数错误 / 用户取消 / 校验未通过，含全部项被拒）
const int kExitFailed = 2;       // 执行失败（有项进入移动但失败或对话框失败，含部分成功）

void showMessage(const std::wstring& text, UINT icon) {
    MessageBoxW(nullptr, text.c_str(), kAppTitle, MB_OK | icon | MB_SETFOREGROUND);
}

void showInfo(const std::wstring& text) {
    showMessage(text, MB_ICONINFORMATION);
}

void showWarning(const std::wstring& text) {
    showMessage(text, MB_ICONWARNING);
}

// FR-05.6：收尾汇总——无失败弹成功提示；有失败弹失败列表，并按语义返回退出码
int finishWithReport(int successCount, int attemptedCount,
                     const std::vector<FailureEntry>& failures,
                     const std::wstring& targetDir) {
    if (failures.empty()) {
        if (successCount == 1) {
            showInfo(L"文件已移动至：\r\n" + targetDir);
        } else {
            showInfo(L"已移动 " + std::to_wstring(successCount) + L" 项至：\r\n" + targetDir);
        }
        return kExitSuccess;
    }

    std::wstring summary;
    if (successCount > 0) {
        summary = L"移动完成：成功 " + std::to_wstring(successCount) +
                  L" 项，失败 " + std::to_wstring(failures.size()) + L" 项";
    } else {
        summary = L"未移动任何项目";
    }
    showFailureReport(summary, failures);
    // 全部在校验阶段被拒视为“未执行”；有任何一项进入过移动则视为“执行失败”
    return (successCount > 0 || attemptedCount > 0) ? kExitFailed : kExitNotExecuted;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        showWarning(L"命令行参数解析失败，请通过右键菜单使用本工具");
        return kExitNotExecuted;
    }

    // FR-05：argv[1..n] 均为源路径（多选批量；单选即 N=1）
    std::vector<std::wstring> rawSources;
    const ArgParseStatus parseStatus = parseSourceArguments(argc, argv, rawSources);
    LocalFree(argv);

    if (parseStatus == ArgParseStatus::MissingArgument) {
        showWarning(L"请通过右键菜单使用本工具");
        return kExitNotExecuted;
    }

    // FR-05.2：逐项预校验（存在性/规范化/长度/黑名单），失败项进汇总，不中断其余项
    std::vector<std::wstring> pending;  // 预校验通过的规范化路径
    std::vector<FailureEntry> failures;
    for (const std::wstring& raw : rawSources) {
        std::wstring canonical;
        const SourceStatus status = validateSource(raw, canonical);
        if (status == SourceStatus::Ok) {
            pending.push_back(std::move(canonical));
        } else {
            failures.push_back({raw, describeSourceStatus(status)});
        }
    }
    dedupePaths(pending);

    if (pending.empty()) {
        return finishWithReport(0, 0, failures, std::wstring());
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(comResult)) {
        showWarning(L"初始化系统对话框失败，无法继续");
        return kExitFailed;
    }

    // FR-03：优先定位到上次成功移动的目标路径，失效则回退到“文档”
    std::wstring initialDir;
    if (loadLastUsedPath(initialDir) && !isDirectory(initialDir)) {
        initialDir.clear();
    }
    if (initialDir.empty()) {
        initialDir = documentsFolder();
    }

    // FR-05.3：整批共用一次系统原生目标选择（多对一）
    std::wstring targetDir;
    const PickFolderStatus pickStatus = pickFolder(nullptr, initialDir, targetDir);
    CoUninitialize();

    if (pickStatus == PickFolderStatus::Cancelled) {
        // 用户取消：不执行、不更新记录；此前被拒的项仍说明原因
        if (!failures.empty()) {
            return finishWithReport(0, 0, failures, std::wstring());
        }
        return kExitNotExecuted;
    }
    if (pickStatus != PickFolderStatus::Picked) {
        showWarning(L"未能获取目标文件夹，操作终止");
        return kExitFailed;
    }

    // FR-05.4：逐项目标校验 + 跨卷判定；目标对全体相同，目标无效时整体终止
    std::vector<std::wstring> movable;
    for (const std::wstring& source : pending) {
        std::wstring destination;
        switch (checkDestination(source, targetDir, destination)) {
            case DestinationStatus::SameAsSource:
                failures.push_back({source, L"源已位于所选目标位置，无需移动"});
                continue;
            case DestinationStatus::InsideSource:
                failures.push_back({source, L"目标位置是源文件夹自身或其子目录"});
                continue;
            case DestinationStatus::PathTooLong:
                failures.push_back({source, L"移动后的完整路径将超过 260 字符，暂不支持"});
                continue;
            case DestinationStatus::Ok:
                break;
            case DestinationStatus::TargetNotDirectory:
            default:
                showWarning(L"目标路径无效，操作终止");
                return kExitFailed;
        }

        // FR-05.5：v1.1 批量仍限同卷；跨卷项明确拒绝，原因预告 v1.2 支持（不静默降级）
        if (!isSameVolume(source, targetDir)) {
            failures.push_back({source, L"暂不支持跨分区移动（跨卷移动将于 v1.2 支持）"});
            continue;
        }
        if (pathExists(destination)) {
            failures.push_back({source, L"目标位置已存在同名文件，已跳过"});
            continue;
        }
        movable.push_back(source);
    }

    // FR-05：深路径先移——父子文件夹同选时子项先移出，父项随后整体移动，两者均可成功
    sortDeepestFirst(movable);

    const std::wstring normalizedTarget = trimTrailingSeparators(targetDir);
    int successCount = 0;
    int attemptedCount = 0;
    for (const std::wstring& source : movable) {
        const std::wstring destination = normalizedTarget + L"\\" + fileName(source);
        ++attemptedCount;
        const MoveOutcome outcome = moveByRename(source, destination);
        if (outcome.status == MoveStatus::Success) {
            ++successCount;
            continue;
        }
        failures.push_back({source, describeMoveFailure(outcome)});
    }

    // FR-03：任一项成功即更新 last_used_path
    if (successCount > 0) {
        saveLastUsedPath(targetDir);
    }

    return finishWithReport(successCount, attemptedCount, failures, targetDir);
}
