#pragma once

#include <windows.h>

#include <string>

namespace quickmove {

enum class PickFolderStatus {
    Picked,
    Cancelled,
    Failed,
};

// 复用系统原生文件夹选择对话框（IFileOpenDialog）（FR-03）
PickFolderStatus pickFolder(HWND owner, const std::wstring& initialDir, std::wstring& chosen);

}  // namespace quickmove