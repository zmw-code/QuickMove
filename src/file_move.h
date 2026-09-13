#pragma once

#include <windows.h>

#include <string>

namespace quickmove {

enum class MoveStatus {
    Success,
    CrossVolume,    // 跨卷（ERROR_NOT_SAME_DEVICE）
    NameConflict,   // 目标已存在同名对象
    AccessDenied,   // 权限不足
    InUse,          // 文件被占用
    NotFound,       // 源或目标路径不存在
    PathInvalid,    // 路径过长或名称无效
    Unknown,        // 其它错误，附错误码
};

struct MoveOutcome {
    MoveStatus status = MoveStatus::Unknown;
    DWORD errorCode = 0;
};

// 同卷移动：MoveFileExW(src, dst, 0)，固定不带 MOVEFILE_COPY_ALLOWED（FR-04）
MoveOutcome moveByRename(const std::wstring& source, const std::wstring& destination);

// 失败原因的中文描述（FR-04 要求给出具体原因）
std::wstring describeMoveFailure(const MoveOutcome& outcome);

}  // namespace quickmove