#pragma once

#include <string>

namespace quickmove {

// FR-02：命令行参数解析结果
enum class ArgParseStatus {
    Ok,
    MissingArgument,   // 未检测到参数（直接双击运行）
    TooManyArguments,  // 收到多条路径（多选），v1 不支持批量
};

// 解析命令行参数，argv[1] 视为源路径（FR-02）
ArgParseStatus parseSourceArgument(int argc, wchar_t* const argv[], std::wstring& source);

// 去除路径首尾可能存在的引号与空白（FR-02）
std::wstring stripSurroundingQuotes(const std::wstring& path);

// 规范化路径：展开为绝对路径 + 展开 8.3 短名 + 去掉末尾多余分隔符
std::wstring canonicalizePath(const std::wstring& path);

// 路径是否存在（FR-02 的“检查该路径是否真实存在”）
bool pathExists(const std::wstring& path);

// 是否为目录
bool isDirectory(const std::wstring& path);

// 是否为盘符根或 UNC 根（C:\ 之类）
bool isDriveRoot(const std::wstring& path);

// 是否超过 MAX_PATH（FR-04 路径长度校验）
bool isPathTooLong(const std::wstring& path);

// child 是否等于 parent，或位于 parent 之下（大小写不敏感）
bool isSameOrUnder(const std::wstring& child, const std::wstring& parent);

// 源路径是否命中系统关键目录黑名单
bool isProtectedSource(const std::wstring& path);

// 源与目标目录是否位于同一卷（FR-04 跨卷判定）
bool isSameVolume(const std::wstring& source, const std::wstring& targetDir);

// 取路径的最后一段名称
std::wstring fileName(const std::wstring& path);

// FR-04 目标合法性检查（原地移动 / 源子目录 / 路径过长 / 目标不可用）
enum class DestinationStatus {
    Ok,
    TargetNotDirectory,
    SameAsSource,
    InsideSource,
    PathTooLong,
};

DestinationStatus checkDestination(const std::wstring& source,
                                   const std::wstring& targetDir,
                                   std::wstring& destination);

// 默认定位目录（FR-03）
std::wstring documentsFolder();
std::wstring localAppDataFolder();

}  // namespace quickmove