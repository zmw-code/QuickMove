// QuickMove 自测程序（随仓库入库；build.cmd 自动编译并运行，失败即中断构建）
// 覆盖：FR-02 参数解析与路径校验、FR-03 配置读写、FR-04 同卷移动；
//       跨卷用例需要传入一个位于其它卷的源路径作为 argv[1]。
// 说明：配置读写用例通过 tests/config_store_redirect.cpp 把 configFilePath()
//       重定向到 build\selftest 草稿目录，避免污染真实 %LOCALAPPDATA% 配置。
#include <windows.h>

#include <cstdio>
#include <string>

#include "config_store.h"
#include "file_move.h"
#include <shlobj.h>

#include "path_utils.h"

using namespace quickmove;

static int g_failures = 0;

static void check(bool condition, const char* label) {
    printf("%-52s %s\n", label, condition ? "PASS" : "FAIL");
    if (!condition) {
        ++g_failures;
    }
}

static bool same(const std::wstring& lhs, const std::wstring& rhs) {
    return CompareStringOrdinal(lhs.c_str(), static_cast<int>(lhs.size()),
                                rhs.c_str(), static_cast<int>(rhs.size()),
                                TRUE) == CSTR_EQUAL;
}

static bool exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static void touch(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
}

static std::wstring knownFolderOrEmpty(const KNOWNFOLDERID& id) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw)) || raw == nullptr) {
        CoTaskMemFree(raw);
        return std::wstring();
    }
    const std::wstring value(raw);
    CoTaskMemFree(raw);
    return value;
}

int wmain(int argc, wchar_t** argv) {
    const std::wstring root = L"build\\selftest";
    const std::wstring srcDir = root + L"\\src_dir";
    const std::wstring innerDir = srcDir + L"\\inner";
    const std::wstring targetDir = root + L"\\target_dir";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(srcDir.c_str(), nullptr);
    CreateDirectoryW(innerDir.c_str(), nullptr);
    CreateDirectoryW(targetDir.c_str(), nullptr);

    const wchar_t* scratch[] = {L"move_a.txt", L"move_b.txt", L"move_c.txt", L"cross_move.txt"};
    for (const wchar_t* name : scratch) {
        DeleteFileW((root + L"\\" + name).c_str());
        DeleteFileW((targetDir + L"\\" + name).c_str());
    }

    const std::wstring rootCanon = canonicalizePath(root);
    const std::wstring srcDirCanon = canonicalizePath(srcDir);
    const std::wstring innerDirCanon = canonicalizePath(innerDir);

    // ---- FR-02 参数解析 ----
    std::wstring out;
    wchar_t* one[] = {const_cast<wchar_t*>(L"QuickMove.exe")};
    check(parseSourceArgument(1, one, out) == ArgParseStatus::MissingArgument && out.empty(),
          "argc=1 -> MissingArgument");

    wchar_t* quoted[] = {const_cast<wchar_t*>(L"QuickMove.exe"), const_cast<wchar_t*>(L"\"C:\\a.txt\"")};
    check(parseSourceArgument(2, quoted, out) == ArgParseStatus::Ok && out == L"C:\\a.txt",
          "argv[1] quoted -> Ok + quotes stripped");

    // ---- 路径规范化与工具函数 ----
    check(same(canonicalizePath(root + L"\\..\\selftest"), rootCanon), "canonicalize collapses '..'");
    check(same(canonicalizePath(srcDir + L"\\\\"), srcDirCanon), "canonicalize trims trailing separator");
    check(same(fileName(L"C:\\dir\\name.txt"), L"name.txt"), "fileName extracts last segment");

    check(isSameOrUnder(srcDirCanon, srcDirCanon), "isSameOrUnder: identical");
    check(isSameOrUnder(innerDirCanon, srcDirCanon), "isSameOrUnder: child");
    check(!isSameOrUnder(srcDirCanon, innerDirCanon), "isSameOrUnder: parent is not a child");

    check(pathExists(srcDir), "pathExists: existing folder");
    check(!pathExists(root + L"\\no_such_thing"), "pathExists: missing path");
    check(isDirectory(srcDir) && !isDirectory(root + L"\\no_such_thing"), "isDirectory works");
    check(isDriveRoot(L"C:\\") && !isDriveRoot(L"C:\\Windows"), "isDriveRoot works");
    check(isPathTooLong(std::wstring(260, L'a')) && !isPathTooLong(std::wstring(259, L'a')),
          "isPathTooLong at 260 chars");

    // ---- 系统关键目录黑名单 ----
    check(isProtectedSource(L"C:\\Windows"), "protected: C:\\Windows");
    check(isProtectedSource(L"C:\\Windows\\System32"), "protected: C:\\Windows\\System32");
    check(isProtectedSource(L"C:\\Program Files"), "protected: C:\\Program Files");
    check(isProtectedSource(L"C:\\Program Files\\SomeApp"), "protected: program subfolder");
    check(isProtectedSource(L"C:\\ProgramData"), "protected: C:\\ProgramData");
    check(isProtectedSource(L"C:\\"), "protected: drive root");
    check(!isProtectedSource(srcDir), "NOT protected: normal folder");
    {
        const std::wstring profile = knownFolderOrEmpty(FOLDERID_Profile);
        if (!profile.empty()) {
            check(isProtectedSource(profile), "protected: user profile root");
            check(!isProtectedSource(profile + L"\\Desktop"), "NOT protected: Desktop inside profile");
        } else {
            printf("%-52s %s\n", "protected: user profile root", "SKIP (folder unresolvable here)");
        }
        const std::wstring localAppData = localAppDataFolder();
        if (!localAppData.empty()) {
            check(isProtectedSource(localAppData), "protected: %LOCALAPPDATA% (exact match)");
            check(!isProtectedSource(localAppData + L"\\SomeApp"), "NOT protected: child of %LOCALAPPDATA%");
        }
    }

    // ---- 跨卷判定 ----
    check(isSameVolume(L"C:\\a.txt", L"C:\\some_dir"), "same volume: C: vs C:");
    check(!isSameVolume(L"C:\\a.txt", L"D:\\some_dir"), "different volume: C: vs D:");

    // ---- FR-04 目标校验 ----
    const std::wstring fileA = root + L"\\move_a.txt";
    touch(fileA);
    std::wstring destination;
    check(checkDestination(fileA, root, destination) == DestinationStatus::SameAsSource,
          "dest check: same as source (in-place)");
    check(checkDestination(srcDir, innerDir, destination) == DestinationStatus::InsideSource,
          "dest check: target inside source folder");
    check(checkDestination(fileA, root + L"\\missing", destination) == DestinationStatus::TargetNotDirectory,
          "dest check: target folder missing");
    const DestinationStatus okStatus = checkDestination(fileA, srcDir, destination);
    check(okStatus == DestinationStatus::Ok && destination == srcDir + L"\\move_a.txt",
          "dest check: ok keeps source file name");

    // ---- FR-04 移动与错误映射 ----
    const std::wstring conflict = targetDir + L"\\move_b.txt";
    const std::wstring fileB = root + L"\\move_b.txt";
    touch(fileB);
    touch(conflict);
    const MoveOutcome conflictOutcome = moveByRename(fileB, conflict);
    check(conflictOutcome.status == MoveStatus::NameConflict && exists(fileB),
          "move onto existing name -> NameConflict, source kept");

    const std::wstring fileC = root + L"\\move_c.txt";
    touch(fileC);
    std::wstring destC;
    checkDestination(fileC, targetDir, destC);
    const MoveOutcome moveOutcome = moveByRename(fileC, destC);
    check(moveOutcome.status == MoveStatus::Success && exists(destC) && !exists(fileC),
          "same-volume move -> Success (source gone, target present)");

    check(moveByRename(L"", L"D:\\x").status == MoveStatus::PathInvalid, "empty source rejected");
    check(moveByRename(root + L"\\nope.txt", targetDir + L"\\nope.txt").status == MoveStatus::NotFound,
          "missing source -> NotFound");

    // 跨卷：源在其它卷（运行时以 argv[1] 提供，如 C:\ 下的临时文件）
    if (argc > 1) {
        const std::wstring crossSource = argv[1];
        touch(crossSource);
        const std::wstring crossTarget = targetDir + L"\\cross_move.txt";
        const MoveOutcome crossOutcome = moveByRename(crossSource, crossTarget);
        check(exists(crossSource) && !exists(crossTarget) && crossOutcome.status == MoveStatus::CrossVolume,
              "cross-volume move -> CrossVolume, both sides untouched");
        DeleteFileW(crossSource.c_str());
    }

    // ---- FR-03 配置读写（原子替换 + JSON 转义；路径已重定向到草稿目录）----
    const bool configExisted = exists(configFilePath());
    const std::wstring probe = L"D:\\测试 目录\\子文件夹";
    const bool saved = saveLastUsedPath(probe);
    std::wstring loaded;
    check(saved && loadLastUsedPath(loaded) && same(loaded, probe),
          "config round-trip via atomic write");
    const std::wstring probe2 = targetDir;
    check(saveLastUsedPath(probe2) && loadLastUsedPath(loaded) && same(loaded, probe2),
          "config overwrite (atomic replace)");
    {
        HANDLE file = CreateFileW(configFilePath().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            const char junk[] = "{ this is not valid json";
            DWORD written = 0;
            WriteFile(file, junk, static_cast<DWORD>(sizeof(junk) - 1), &written, nullptr);
            CloseHandle(file);
        }
        std::wstring broken;
        check(!loadLastUsedPath(broken), "corrupt config -> load returns false");
    }
    if (!configExisted) {
        DeleteFileW(configFilePath().c_str());
        DeleteFileW((configFilePath() + L".tmp").c_str());
    }

    printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL CHECKS PASSED" : "FAILURES DETECTED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
