#include "folder_picker.h"

#include <shobjidl.h>
#include <wrl/client.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Uuid.lib")

using Microsoft::WRL::ComPtr;

namespace quickmove {

PickFolderStatus pickFolder(HWND owner, const std::wstring& initialDir, std::wstring& chosen) {
    chosen.clear();

    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))) ||
        dialog == nullptr) {
        return PickFolderStatus::Failed;
    }

    FILEOPENDIALOGOPTIONS options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        options |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        dialog->SetOptions(options);
    }

    dialog->SetTitle(L"选择移动目标文件夹");

    if (!initialDir.empty()) {
        ComPtr<IShellItem> folder;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialDir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            // 优先定位到上次成功移动的目标路径（FR-03）
            dialog->SetFolder(folder.Get());
        }
    }

    const HRESULT shown = dialog->Show(owner);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return PickFolderStatus::Cancelled;
    }
    if (FAILED(shown)) {
        return PickFolderStatus::Failed;
    }

    ComPtr<IShellItem> result;
    if (FAILED(dialog->GetResult(&result)) || result == nullptr) {
        return PickFolderStatus::Failed;
    }

    PWSTR fileSystemPath = nullptr;
    if (FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &fileSystemPath)) || fileSystemPath == nullptr) {
        if (fileSystemPath != nullptr) {
            CoTaskMemFree(fileSystemPath);
        }
        return PickFolderStatus::Failed;
    }

    chosen.assign(fileSystemPath);
    CoTaskMemFree(fileSystemPath);
    return PickFolderStatus::Picked;
}

PickFilesStatus pickFiles(HWND owner, const std::wstring& initialDir, std::vector<std::wstring>& chosen) {
    chosen.clear();

    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))) ||
        dialog == nullptr) {
        return PickFilesStatus::Failed;
    }

    FILEOPENDIALOGOPTIONS options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        // 文件模式（不设 FOS_PICKFOLDERS）：文件与文件夹都可作为移动源
        options |= FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        dialog->SetOptions(options);
    }

    dialog->SetTitle(L"选择要移动的项目（可按住 Ctrl 或框选多选）");

    if (!initialDir.empty()) {
        ComPtr<IShellItem> folder;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialDir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
    }

    const HRESULT shown = dialog->Show(owner);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return PickFilesStatus::Cancelled;
    }
    if (FAILED(shown)) {
        return PickFilesStatus::Failed;
    }

    ComPtr<IShellItemArray> results;
    if (FAILED(dialog->GetResults(&results)) || results == nullptr) {
        return PickFilesStatus::Failed;
    }

    DWORD count = 0;
    if (FAILED(results->GetCount(&count)) || count == 0) {
        return PickFilesStatus::Failed;
    }

    chosen.reserve(count);
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        if (FAILED(results->GetItemAt(i, &item)) || item == nullptr) {
            continue;
        }
        PWSTR fileSystemPath = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &fileSystemPath)) &&
            fileSystemPath != nullptr) {
            chosen.emplace_back(fileSystemPath);
            CoTaskMemFree(fileSystemPath);
        }
    }

    if (chosen.empty()) {
        return PickFilesStatus::Failed;
    }
    return PickFilesStatus::Picked;
}

}  // namespace quickmove