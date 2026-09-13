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

}  // namespace quickmove