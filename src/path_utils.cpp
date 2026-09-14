#include "path_utils.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Uuid.lib")

namespace quickmove {
namespace {

// 系统关键目录黑名单：includeChildren 为 true 时连同子目录一并拦截
struct ProtectedFolder {
    const KNOWNFOLDERID* id;
    bool includeChildren;
};

const ProtectedFolder kProtectedFolders[] = {
    {&FOLDERID_Windows, true},          // C:\Windows
    {&FOLDERID_ProgramFiles, true},     // C:\Program Files
    {&FOLDERID_ProgramFilesX86, true},  // C:\Program Files (x86)
    {&FOLDERID_ProgramData, true},      // C:\ProgramData
    {&FOLDERID_Profile, false},         // 用户目录根（桌面/文档等子目录不受影响）
    {&FOLDERID_LocalAppData, false},    // %LOCALAPPDATA%
    {&FOLDERID_RoamingAppData, false},  // %APPDATA%
};

// 已知文件夹路径；失败返回空串
std::wstring knownFolderPath(const KNOWNFOLDERID& id) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw)) || raw == nullptr) {
        if (raw != nullptr) {
            CoTaskMemFree(raw);
        }
        return std::wstring();
    }
    const std::wstring result(raw);
    CoTaskMemFree(raw);
    return result;
}

std::wstring environmentPath(const wchar_t* name) {
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) {
        return std::wstring();
    }
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, &value[0], needed);
    if (written == 0) {
        return std::wstring();
    }
    value.resize(written);
    return value;
}

// 去掉末尾多余分隔符，但保留 "C:\" 这类根
bool equalsIgnoreCase(const std::wstring& lhs, const std::wstring& rhs) {
    return CompareStringOrdinal(lhs.c_str(), static_cast<int>(lhs.size()),
                                rhs.c_str(), static_cast<int>(rhs.size()),
                                TRUE) == CSTR_EQUAL;
}

bool startsWithIgnoreCase(const std::wstring& text, const std::wstring& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return CompareStringOrdinal(text.c_str(), static_cast<int>(prefix.size()),
                                prefix.c_str(), static_cast<int>(prefix.size()),
                                TRUE) == CSTR_EQUAL;
}

}  // namespace

// 去掉末尾多余分隔符，但保留 "C:\" 这类根（导出：FR-05 拼接目标路径用）
std::wstring trimTrailingSeparators(std::wstring path) {
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) {
        path.pop_back();
    }
    return path;
}

ArgParseStatus parseSourceArguments(int argc, wchar_t* const argv[],
                                    std::vector<std::wstring>& sources) {
    sources.clear();
    if (argv == nullptr || argc <= 1) {
        return ArgParseStatus::MissingArgument;
    }
    // FR-05：资源管理器多选时每条路径作为独立参数传入（带引号）
    for (int i = 1; i < argc; ++i) {
        std::wstring item = stripSurroundingQuotes(argv[i] == nullptr ? L"" : argv[i]);
        if (!item.empty()) {
            sources.push_back(std::move(item));
        }
    }
    if (sources.empty()) {
        return ArgParseStatus::MissingArgument;
    }
    return ArgParseStatus::Ok;
}

SourceStatus validateSource(const std::wstring& raw, std::wstring& canonical) {
    canonical.clear();
    if (raw.empty() || !pathExists(raw)) {
        return SourceStatus::NotExist;
    }
    canonical = canonicalizePath(raw);
    if (canonical.empty()) {
        return SourceStatus::CanonicalFailed;
    }
    if (isPathTooLong(canonical)) {
        canonical.clear();
        return SourceStatus::TooLong;
    }
    if (isProtectedSource(canonical)) {
        canonical.clear();
        return SourceStatus::Protected;
    }
    return SourceStatus::Ok;
}

std::wstring describeSourceStatus(SourceStatus status) {
    switch (status) {
        case SourceStatus::NotExist:
        case SourceStatus::CanonicalFailed:
            return L"源路径无效";
        case SourceStatus::TooLong:
            return L"路径过长：暂不支持超过 260 字符的路径";
        case SourceStatus::Protected:
            return L"该位置属于系统关键目录，拒绝移动";
        case SourceStatus::Ok:
        default:
            return std::wstring();
    }
}

void dedupePaths(std::vector<std::wstring>& paths) {
    std::vector<std::wstring> unique;
    unique.reserve(paths.size());
    for (std::wstring& path : paths) {
        bool duplicate = false;
        for (const std::wstring& kept : unique) {
            if (equalsIgnoreCase(path, kept)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique.push_back(std::move(path));
        }
    }
    paths.swap(unique);
}

void sortDeepestFirst(std::vector<std::wstring>& paths) {
    // 深路径先处理：路径 a 为 b 的前缀时必有 size(a) < size(b)，
    // 稳定排序保证同类深度下维持用户传入顺序
    std::stable_sort(paths.begin(), paths.end(),
                     [](const std::wstring& a, const std::wstring& b) {
                         return a.size() > b.size();
                     });
}

std::wstring stripSurroundingQuotes(const std::wstring& path) {
    size_t begin = 0;
    size_t end = path.size();
    while (begin < end && (path[begin] == L'"' || path[begin] == L' ')) {
        ++begin;
    }
    while (end > begin && (path[end - 1] == L'"' || path[end - 1] == L' ')) {
        --end;
    }
    return path.substr(begin, end - begin);
}

std::wstring canonicalizePath(const std::wstring& path) {
    if (path.empty()) {
        return std::wstring();
    }

    const DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        return trimTrailingSeparators(path);
    }

    std::wstring full(needed, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), needed, &full[0], nullptr);
    if (written == 0) {
        return trimTrailingSeparators(path);
    }
    full.resize(written);

    // 展开 8.3 短名；路径不存在时保持原样
    std::wstring longName(full.size() + 1, L'\0');
    const DWORD longWritten =
        GetLongPathNameW(full.c_str(), &longName[0], static_cast<DWORD>(longName.size()));
    if (longWritten > 0 && longWritten <= longName.size()) {
        longName.resize(longWritten);
        return trimTrailingSeparators(longName);
    }
    return trimTrailingSeparators(full);
}

bool pathExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool isDriveRoot(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    return PathIsRootW(path.c_str()) != FALSE;
}

bool isPathTooLong(const std::wstring& path) {
    return path.size() >= MAX_PATH;
}

bool isSameOrUnder(const std::wstring& child, const std::wstring& parent) {
    if (child.empty() || parent.empty()) {
        return false;
    }

    const std::wstring childPath = trimTrailingSeparators(child);
    const std::wstring parentPath = trimTrailingSeparators(parent);
    if (!startsWithIgnoreCase(childPath, parentPath)) {
        return false;
    }
    if (childPath.size() == parentPath.size()) {
        return true;
    }
    if (parentPath.back() == L'\\') {
        return true;
    }
    return childPath[parentPath.size()] == L'\\';
}

bool isProtectedSource(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    const std::wstring full = canonicalizePath(path);
    if (full.empty()) {
        return false;
    }

    // 盘符根、UNC 根一律拦截
    if (isDriveRoot(full)) {
        return true;
    }

    for (const ProtectedFolder& folder : kProtectedFolders) {
        const std::wstring base = knownFolderPath(*folder.id);
        if (base.empty()) {
            continue;
        }
        const std::wstring baseFull = canonicalizePath(base);
        if (folder.includeChildren) {
            if (isSameOrUnder(full, baseFull)) {
                return true;
            }
        } else if (equalsIgnoreCase(full, baseFull)) {
            return true;
        }
    }
    return false;
}

bool isSameVolume(const std::wstring& source, const std::wstring& targetDir) {
    if (source.empty() || targetDir.empty()) {
        return false;
    }
    return PathIsSameRootW(source.c_str(), targetDir.c_str()) != FALSE;
}

std::wstring fileName(const std::wstring& path) {
    const std::wstring trimmed = trimTrailingSeparators(path);
    const size_t position = trimmed.find_last_of(L"\\/");
    if (position == std::wstring::npos) {
        return trimmed;
    }
    return trimmed.substr(position + 1);
}

DestinationStatus checkDestination(const std::wstring& source,
                                   const std::wstring& targetDir,
                                   std::wstring& destination) {
    destination.clear();

    if (!isDirectory(targetDir)) {
        return DestinationStatus::TargetNotDirectory;
    }

    const std::wstring dir = trimTrailingSeparators(targetDir);
    const std::wstring name = fileName(source);
    if (dir.empty() || name.empty()) {
        // 防御分支：源为根目录等情况在调用本函数前已被拦截
        return DestinationStatus::TargetNotDirectory;
    }

    destination = dir + L"\\" + name;

    // 1) 原地移动
    if (equalsIgnoreCase(canonicalizePath(source), canonicalizePath(destination))) {
        return DestinationStatus::SameAsSource;
    }

    // 2) 目标为源文件夹自身或其子目录
    if (isDirectory(source) && isSameOrUnder(canonicalizePath(dir), canonicalizePath(source))) {
        return DestinationStatus::InsideSource;
    }

    // 3) 路径超长
    if (isPathTooLong(destination)) {
        return DestinationStatus::PathTooLong;
    }

    return DestinationStatus::Ok;
}

std::wstring documentsFolder() {
    const std::wstring known = knownFolderPath(FOLDERID_Documents);
    if (!known.empty()) {
        return known;
    }
    const std::wstring profile = environmentPath(L"USERPROFILE");
    if (!profile.empty()) {
        return profile + L"\\Documents";
    }
    return std::wstring();
}

std::wstring localAppDataFolder() {
    const std::wstring known = knownFolderPath(FOLDERID_LocalAppData);
    if (!known.empty()) {
        return known;
    }
    return environmentPath(L"LOCALAPPDATA");
}

}  // namespace quickmove