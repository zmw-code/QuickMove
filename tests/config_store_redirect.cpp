// 测试专用桩：覆盖 config_store.cpp 的 configFilePath()，
// 把配置文件重定向到 build\selftest 草稿目录，避免自测用例污染真实
// %LOCALAPPDATA%\QuickMove\config.json。构建 selftest.exe 时用它替换
// src\config_store.cpp（见 build.cmd），其余实现保持与源码一致。
// 注意：修改 src\config_store.cpp 时必须同步本文件。
#include "config_store.h"

#include "path_utils.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace quickmove {
namespace {

const wchar_t kConfigDirName[] = L"QuickMove";
const wchar_t kConfigFileName[] = L"config.json";
const wchar_t kTempFileName[] = L"config.tmp";
const char kLastUsedKey[] = "last_used_path";

bool isJsonSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int length = static_cast<int>(text.size());
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, &result[0], needed, nullptr, nullptr);
    return result;
}

bool utf8ToWide(const std::string& text, std::wstring& out) {
    out.clear();
    if (text.empty()) {
        return true;
    }
    const int length = static_cast<int>(text.size());
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), length, nullptr, 0);
    if (needed <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(needed));
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), length, &out[0], needed);
    if (written <= 0) {
        out.clear();
        return false;
    }
    out.resize(static_cast<size_t>(written));
    return true;
}

std::wstring parentDirectory(const std::wstring& path) {
    const size_t position = path.find_last_of(L"\\/");
    if (position == std::wstring::npos) {
        return std::wstring();
    }
    return path.substr(0, position);
}

bool readAllBytes(const std::wstring& path, std::string& out) {
    out.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    char buffer[4096];
    bool ok = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(file, buffer, static_cast<DWORD>(sizeof(buffer)), &read, nullptr)) {
            ok = false;
            break;
        }
        if (read == 0) {
            break;
        }
        out.append(buffer, read);
    }
    CloseHandle(file);

    if (!ok) {
        out.clear();
    }
    return ok;
}

bool writeAllBytes(const std::wstring& path, const std::string& data) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool ok = true;
    size_t offset = 0;
    while (offset < data.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 64u * 1024u));
        DWORD written = 0;
        if (!WriteFile(file, data.data() + offset, chunk, &written, nullptr) || written != chunk) {
            ok = false;
            break;
        }
        offset += written;
    }
    if (ok && !FlushFileBuffers(file)) {
        ok = false;
    }
    CloseHandle(file);

    if (!ok) {
        DeleteFileW(path.c_str());
    }
    return ok;
}

void appendUtf8CodePoint(std::string& out, unsigned int codePoint) {
    if (codePoint <= 0x7Fu) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80u | (codePoint >> 6) & 0x3Fu));
    } else {
        out.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80u | (codePoint >> 12) & 0x3Fu));
        out.push_back(static_cast<char>(0x80u | (codePoint >> 6) & 0x3Fu));
    }
}

bool readHex4(const std::string& text, size_t offset, unsigned int& value) {
    if (offset + 4 > text.size()) {
        return false;
    }
    unsigned int result = 0;
    for (size_t i = 0; i < 4; ++i) {
        const char c = text[offset + i];
        unsigned int digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned int>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned int>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned int>(c - 'A' + 10);
        } else {
            return false;
        }
        result = (result << 4) | digit;
    }
    value = result;
    return true;
}

// 取出指定键的字符串值，转义序列还原为 UTF-8 字节
bool extractJsonString(const std::string& json, const std::string& key, std::string& value) {
    value.clear();

    const std::string needle = "\"" + key + "\"";
    size_t position = json.find(needle);
    if (position == std::string::npos) {
        return false;
    }
    position += needle.size();

    while (position < json.size() && isJsonSpace(json[position])) {
        ++position;
    }
    if (position >= json.size() || json[position] != ':') {
        return false;
    }
    ++position;
    while (position < json.size() && isJsonSpace(json[position])) {
        ++position;
    }
    if (position >= json.size() || json[position] != '"') {
        return false;
    }
    ++position;

    while (position < json.size()) {
        const char c = json[position];
        if (c == '"') {
            return true;
        }
        if (c != '\\') {
            value.push_back(c);
            ++position;
            continue;
        }

        ++position;
        if (position >= json.size()) {
            return false;
        }
        switch (json[position]) {
            case '"': value.push_back('"'); ++position; break;
            case '\\': value.push_back('\\'); ++position; break;
            case '/': value.push_back('/'); ++position; break;
            case 'b': value.push_back('\b'); ++position; break;
            case 'f': value.push_back('\f'); ++position; break;
            case 'n': value.push_back('\n'); ++position; break;
            case 'r': value.push_back('\r'); ++position; break;
            case 't': value.push_back('\t'); ++position; break;
            case 'u': {
                unsigned int codePoint = 0;
                if (!readHex4(json, position + 1, codePoint)) {
                    return false;
                }
                position += 5;
                if (codePoint >= 0xD800u && codePoint <= 0xDBFFu && position + 1 < json.size() &&
                    json[position] == '\\' && json[position + 1] == 'u') {
                    unsigned int low = 0;
                    if (readHex4(json, position + 2, low) && low >= 0xDC00u && low <= 0xDFFFu) {
                        codePoint = 0x10000u + ((codePoint - 0xD800u) << 10) + (low - 0xDC00u);
                        position += 6;
                    }
                }
                appendUtf8CodePoint(value, codePoint);
                break;
            }
            default:
                return false;
        }
    }
    return false;
}

std::string jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: {
                const unsigned char byte = static_cast<unsigned char>(c);
                if (byte < 0x20u) {
                    char buffer[8];
                    sprintf_s(buffer, "\\u%04X", static_cast<unsigned int>(byte));
                    out += buffer;
                } else {
                    out.push_back(c);
                }
                break;
            }
        }
    }
    return out;
}

}  // namespace

// 与 src\config_store.cpp 的唯一差异：基目录改为 build\selftest 草稿目录
std::wstring configFilePath() {
    return std::wstring(L"build\\selftest") + L"\\" + kConfigDirName + L"\\" + kConfigFileName;
}

bool loadLastUsedPath(std::wstring& path) {
    path.clear();

    const std::wstring file = configFilePath();
    if (file.empty()) {
        return false;
    }

    std::string content;
    if (!readAllBytes(file, content)) {
        return false;
    }

    std::string raw;
    if (!extractJsonString(content, kLastUsedKey, raw)) {
        return false;
    }

    std::wstring wide;
    if (!utf8ToWide(raw, wide) || wide.empty()) {
        return false;
    }

    path = wide;
    return true;
}

bool saveLastUsedPath(const std::wstring& path) {
    const std::wstring file = configFilePath();
    if (file.empty() || path.empty()) {
        return false;
    }

    const std::wstring dir = parentDirectory(file);
    if (!dir.empty()) {
        if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }

    const std::string content =
        "{\r\n  \"" + std::string(kLastUsedKey) + "\": \"" + jsonEscape(wideToUtf8(path)) + "\"\r\n}\r\n";

    const std::wstring temp = dir.empty() ? (file + L".tmp") : (dir + L"\\" + kTempFileName);
    if (!writeAllBytes(temp, content)) {
        return false;
    }
    if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

}  // namespace quickmove
