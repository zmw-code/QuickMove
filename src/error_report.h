#pragma once

#include <string>
#include <vector>

namespace quickmove {

// FR-05：单条失败记录（展示路径 + 中文原因）
struct FailureEntry {
    std::wstring path;
    std::wstring reason;
};

// FR-05.6：轻量失败列表对话框——只读明细 + “复制失败信息”写入剪贴板。
// 纯 Win32 内存模板对话框（无资源脚本依赖）；模板创建失败时回退为截断的 MessageBoxW。
// 单线程模态弹出，剪贴板操作带有限次重试，不产生阻塞等待。
void showFailureReport(const std::wstring& summary, const std::vector<FailureEntry>& entries);

}  // namespace quickmove
