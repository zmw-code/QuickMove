#pragma once

#include <string>

namespace quickmove {

// 配置文件路径：%LOCALAPPDATA%\QuickMove\config.json
std::wstring configFilePath();

// 读取 last_used_path；文件缺失、内容损坏或字段为空时返回 false
bool loadLastUsedPath(std::wstring& path);

// 原子写入 last_used_path：先写临时文件 config.tmp，再以替换方式重命名（FR-03）
bool saveLastUsedPath(const std::wstring& path);

}  // namespace quickmove