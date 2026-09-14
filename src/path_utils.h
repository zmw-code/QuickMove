#pragma once

#include <string>
#include <vector>

namespace quickmove {

// FR-02/FR-05：命令行参数解析结果
enum class ArgParseStatus {
    Ok,
    MissingArgument,   // 未检测到参数（直接双击运行）
};

// 解析命令行参数：argv[1..argc-1] 均视为源路径（FR-05 批量移动，单选即 N=1）；
// 逐项去引号与首尾空白，跳过空项；全部为空时返回 MissingArgument
ArgParseStatus parseSourceArguments(int argc, wchar_t* const argv[],
                                    std::vector<std::wstring>& sources);

// FR-05：单项源路径预校验（存在性 → 规范化 → 长度 → 黑名单），失败项进入汇总而不中断批次
enum class SourceStatus {
    Ok,
    NotExist,        // 源路径无效
    CanonicalFailed, // 规范化失败（视为无效）
    TooLong,         // 超过 260 字符
    Protected,       // 系统关键目录
};

SourceStatus validateSource(const std::wstring& raw, std::wstring& canonical);

// 校验结果的中文描述（FR-05 失败汇总用）
std::wstring describeSourceStatus(SourceStatus status);

// FR-05：规范化路径按大小写不敏感去重，保留首次出现顺序
void dedupePaths(std::vector<std::wstring>& paths);

// FR-05：按路径长度降序稳定排序（深路径先移：父子文件夹同选时子项先移出，父项随后整体移动）
void sortDeepestFirst(std::vector<std::wstring>& paths);

// 去除路径首尾可能存在的引号与空白（FR-02）
std::wstring stripSurroundingQuotes(const std::wstring& path);

// 去掉末尾多余分隔符，但保留 "C:\" 这类根（FR-05 拼接目标路径用）
std::wstring trimTrailingSeparators(std::wstring path);

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