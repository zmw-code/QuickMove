// 快捷式文件转移 (QuickMove) v1.0
// 需求：v1.0 需求分析文档（FR-01 ~ FR-04）

#include <windows.h>

#include <shellapi.h>

#include <string>

#include "config_store.h"
#include "file_move.h"
#include "folder_picker.h"
#include "path_utils.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")

using namespace quickmove;

namespace {

const wchar_t kAppTitle[] = L"快捷式文件转移";

// 退出码
const int kExitSuccess = 0;      // 移动成功
const int kExitNotExecuted = 1;  // 未执行（参数错误 / 用户取消 / 校验未通过）
const int kExitFailed = 2;       // 执行失败

void showMessage(const std::wstring& text, UINT icon) {
    MessageBoxW(nullptr, text.c_str(), kAppTitle, MB_OK | icon | MB_SETFOREGROUND);
}

void showInfo(const std::wstring& text) {
    showMessage(text, MB_ICONINFORMATION);
}

void showWarning(const std::wstring& text) {
    showMessage(text, MB_ICONWARNING);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        showWarning(L"命令行参数解析失败，请通过右键菜单使用本工具");
        return kExitNotExecuted;
    }

    // FR-02：读取 argv[1]、去引号、校验
    std::wstring source;
    const ArgParseStatus parseStatus = parseSourceArgument(argc, argv, source);
    LocalFree(argv);

    if (parseStatus == ArgParseStatus::MissingArgument) {
        showWarning(L"请通过右键菜单使用本工具");
        return kExitNotExecuted;
    }
    if (parseStatus == ArgParseStatus::TooManyArguments) {
        showWarning(L"v1 暂不支持批量移动，请每次只选择一个文件或文件夹");
        return kExitNotExecuted;
    }

    if (!pathExists(source)) {
        showWarning(L"源路径无效");
        return kExitNotExecuted;
    }

    const std::wstring sourcePath = canonicalizePath(source);
    if (sourcePath.empty()) {
        showWarning(L"源路径无效");
        return kExitNotExecuted;
    }

    if (isPathTooLong(sourcePath)) {
        showWarning(L"路径过长：v1 暂不支持超过 260 字符的路径");
        return kExitNotExecuted;
    }

    if (isProtectedSource(sourcePath)) {
        showWarning(L"该位置属于系统关键目录，v1 拒绝移动：\r\n" + sourcePath);
        return kExitNotExecuted;
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

    std::wstring targetDir;
    const PickFolderStatus pickStatus = pickFolder(nullptr, initialDir, targetDir);
    CoUninitialize();

    if (pickStatus == PickFolderStatus::Cancelled) {
        return kExitNotExecuted;  // 用户取消：不更新记录
    }
    if (pickStatus != PickFolderStatus::Picked) {
        showWarning(L"未能获取目标文件夹，操作终止");
        return kExitFailed;
    }

    // FR-04：目标合法性校验
    std::wstring destination;
    switch (checkDestination(sourcePath, targetDir, destination)) {
        case DestinationStatus::SameAsSource:
            showWarning(L"源文件已位于所选目标位置，无需移动");
            return kExitNotExecuted;
        case DestinationStatus::InsideSource:
            showWarning(L"无法移动：目标位置是源文件夹自身或其子目录");
            return kExitNotExecuted;
        case DestinationStatus::PathTooLong:
            showWarning(L"路径过长：移动后的完整路径将超过 260 字符，v1 暂不支持");
            return kExitNotExecuted;
        case DestinationStatus::TargetNotDirectory:
            showWarning(L"目标路径无效，请重新选择");
            return kExitNotExecuted;
        case DestinationStatus::Ok:
        default:
            break;
    }

    // FR-04：跨分区判定（不调用移动，直接报错）
    if (!isSameVolume(sourcePath, targetDir)) {
        showWarning(L"当前版本暂不支持跨分区移动");
        return kExitNotExecuted;
    }

    // FR-04：同名冲突（不提供覆盖选项）
    if (pathExists(destination)) {
        showWarning(L"目标位置已存在同名文件，移动终止");
        return kExitNotExecuted;
    }

    const MoveOutcome outcome = moveByRename(sourcePath, destination);
    if (outcome.status != MoveStatus::Success) {
        showWarning(L"移动失败：\r\n" + describeMoveFailure(outcome));
        return kExitFailed;
    }

    // FR-03：仅在移动成功后更新 last_used_path
    saveLastUsedPath(targetDir);

    showInfo(L"文件已移动至：\r\n" + targetDir);
    return kExitSuccess;
}