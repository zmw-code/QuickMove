#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace quickmove {

enum class PickFolderStatus {
    Picked,
    Cancelled,
    Failed,
};

// 复用系统原生文件夹选择对话框（IFileOpenDialog）（FR-03）
PickFolderStatus pickFolder(HWND owner, const std::wstring& initialDir, std::wstring& chosen);

enum class PickFilesStatus {
    Picked,     // 至少选中一项
    Cancelled,  // 用户取消
    Failed,     // 无法获取结果
};

// D10 批量入口：原生文件对话框多选源项目（不设 FOS_PICKFOLDERS，文件与文件夹均可选）
PickFilesStatus pickFiles(HWND owner, const std::wstring& initialDir, std::vector<std::wstring>& chosen);

}  // namespace quickmove