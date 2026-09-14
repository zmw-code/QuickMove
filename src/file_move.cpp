#include "file_move.h"

namespace quickmove {

const wchar_t kCrossVolumeText[] = L"当前版本暂不支持跨分区移动";
const wchar_t kNameConflictText[] = L"目标位置已存在同名文件";
const wchar_t kAccessDeniedText[] = L"权限不足，无法完成移动";
const wchar_t kInUseText[] = L"文件正在使用，请关闭相关程序后重试";
const wchar_t kNotFoundText[] = L"源文件或目标路径不存在";
const wchar_t kPathInvalidText[] = L"路径过长或名称无效，v1 暂不支持超过 260 字符的路径";

MoveOutcome moveByRename(const std::wstring& source, const std::wstring& destination) {
    MoveOutcome outcome;

    if (source.empty() || destination.empty()) {
        outcome.status = MoveStatus::PathInvalid;
        return outcome;
    }

    // 第三个参数固定为 0：不带 MOVEFILE_COPY_ALLOWED，
    // 跨卷时必然失败（ERROR_NOT_SAME_DEVICE），不会静默回退为“复制 + 删除”。
    if (MoveFileExW(source.c_str(), destination.c_str(), 0)) {
        outcome.status = MoveStatus::Success;
        return outcome;
    }

    const DWORD error = GetLastError();
    outcome.errorCode = error;

    switch (error) {
        case ERROR_NOT_SAME_DEVICE:
            outcome.status = MoveStatus::CrossVolume;
            break;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
            outcome.status = MoveStatus::NameConflict;
            break;
        case ERROR_ACCESS_DENIED:
        case ERROR_NETWORK_ACCESS_DENIED:
            outcome.status = MoveStatus::AccessDenied;
            break;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            outcome.status = MoveStatus::InUse;
            break;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            outcome.status = MoveStatus::NotFound;
            break;
        case ERROR_FILENAME_EXCED_RANGE:
        case ERROR_INVALID_NAME:
            outcome.status = MoveStatus::PathInvalid;
            break;
        default:
            outcome.status = MoveStatus::Unknown;
            break;
    }
    return outcome;
}

std::wstring describeMoveFailure(const MoveOutcome& outcome) {
    switch (outcome.status) {
        case MoveStatus::CrossVolume:
            return kCrossVolumeText;
        case MoveStatus::NameConflict:
            return kNameConflictText;
        case MoveStatus::AccessDenied:
            return kAccessDeniedText;
        case MoveStatus::InUse:
            return kInUseText;
        case MoveStatus::NotFound:
            return kNotFoundText;
        case MoveStatus::PathInvalid:
            return kPathInvalidText;
        case MoveStatus::Success:
            return std::wstring();
        case MoveStatus::Unknown:
        default: {
            wchar_t buffer[160];
            swprintf_s(buffer, _countof(buffer), L"移动失败（Windows 错误码 %lu）",
                       static_cast<unsigned long>(outcome.errorCode));
            return std::wstring(buffer);
        }
    }
}

}  // namespace quickmove