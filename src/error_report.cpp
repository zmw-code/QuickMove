#include "error_report.h"

#include <windows.h>

#include <cwchar>
#include <cstring>
#include <string>
#include <vector>

namespace quickmove {
namespace {

const wchar_t kAppTitle[] = L"快捷式文件转移";
const WORD kIdDetailEdit = 101;    // 只读多行明细
const WORD kIdCopyButton = 100;    // 复制失败信息
const size_t kMaxFallbackChars = 4000;

struct ReportContext {
    std::wstring text;  // 编辑框全文 = 复制到剪贴板的内容
};

// ---- 内存 DLGTEMPLATE 组装 ----
// 对齐规则：DLGTEMPLATE 头按 WORD 对齐，每个 DLGITEMTEMPLATE 必须从 DWORD 边界开始；
// 标准模板（非 EX）的项内 id 是 WORD，且没有 helpId 字段。
void alignBuffer(std::vector<BYTE>& buffer, size_t boundary) {
    while (buffer.size() % boundary != 0) {
        buffer.push_back(0);
    }
}

void putWord(std::vector<BYTE>& buffer, WORD value) {
    alignBuffer(buffer, sizeof(WORD));
    const auto* bytes = reinterpret_cast<const BYTE*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(WORD));
}

void putDword(std::vector<BYTE>& buffer, DWORD value) {
    alignBuffer(buffer, sizeof(DWORD));
    const auto* bytes = reinterpret_cast<const BYTE*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(DWORD));
}

void putString(std::vector<BYTE>& buffer, const wchar_t* text) {
    alignBuffer(buffer, sizeof(WORD));
    const size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    const auto* start = reinterpret_cast<const BYTE*>(text);
    buffer.insert(buffer.end(), start, start + bytes);
}

void putItem(std::vector<BYTE>& buffer, DWORD style, DWORD exStyle, WORD x, WORD y, WORD cx, WORD cy,
             WORD id, const wchar_t* className, const wchar_t* title) {
    putDword(buffer, style);        // putDword 先对齐，保证项从 DWORD 边界开始
    putDword(buffer, exStyle);
    putWord(buffer, x);
    putWord(buffer, y);
    putWord(buffer, cx);
    putWord(buffer, cy);
    putWord(buffer, id);
    putString(buffer, className);
    putString(buffer, title);
    putWord(buffer, 0);             // cbExtra
}

std::vector<BYTE> buildTemplate() {
    std::vector<BYTE> buffer;
    buffer.reserve(1024);

    // DLGTEMPLATE 头：布局单位 DLU，9pt 微软雅黑（中文界面标准字体）
    putDword(buffer, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER);
    putDword(buffer, 0);                    // dwExtendedStyle
    putWord(buffer, 3);                     // cdit
    putWord(buffer, 0);                     // x
    putWord(buffer, 0);                     // y
    putWord(buffer, 210);                   // cx
    putWord(buffer, 124);                   // cy
    putWord(buffer, 0);                     // menu：无
    putWord(buffer, 0);                     // class：默认对话框类
    putString(buffer, kAppTitle);           // title
    putWord(buffer, 9);                     // pointsize
    putString(buffer, L"Microsoft YaHei UI");

    // 只读多行明细
    putItem(buffer,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_READONLY,
            WS_EX_CLIENTEDGE,
            7, 7, 196, 92, kIdDetailEdit, L"Edit", L"");

    // 复制失败信息
    putItem(buffer,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            7, 106, 64, 14, kIdCopyButton, L"Button", L"复制失败信息(&C)");

    // 关闭（默认按钮）
    putItem(buffer,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0,
            139, 106, 64, 14, IDOK, L"Button", L"关闭(&O)");

    return buffer;
}

bool copyTextToClipboard(HWND owner, const std::wstring& text) {
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (global == nullptr) {
        return false;
    }
    void* locked = GlobalLock(global);
    if (locked == nullptr) {
        GlobalFree(global);
        return false;
    }
    memcpy(locked, text.c_str(), bytes);
    GlobalUnlock(global);

    // 防死锁：剪贴板可能被其它进程短暂占用，有限次重试后放弃，绝不无限等待
    bool opened = false;
    for (int attempt = 0; attempt < 10 && !opened; ++attempt) {
        opened = OpenClipboard(owner) != FALSE;
        if (!opened) {
            Sleep(10);
        }
    }
    if (!opened) {
        GlobalFree(global);
        return false;
    }

    EmptyClipboard();
    const bool set = SetClipboardData(CF_UNICODETEXT, global) != nullptr;
    CloseClipboard();
    if (!set) {
        GlobalFree(global);  // SetClipboardData 成功后内存归系统所有，不得释放
    }
    return set;
}

INT_PTR CALLBACK reportDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_INITDIALOG: {
            auto* context = reinterpret_cast<ReportContext*>(lparam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
            SetDlgItemTextW(dialog, kIdDetailEdit, context->text.c_str());
            SetForegroundWindow(dialog);
            SetFocus(GetDlgItem(dialog, IDOK));
            return FALSE;  // 焦点已自行设置
        }
        case WM_COMMAND: {
            const WORD id = LOWORD(wparam);
            if (id == kIdCopyButton && HIWORD(wparam) == BN_CLICKED) {
                auto* context = reinterpret_cast<ReportContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
                if (context != nullptr && copyTextToClipboard(dialog, context->text)) {
                    SetDlgItemTextW(dialog, kIdCopyButton, L"已复制");
                }
                return TRUE;
            }
            if (id == IDOK || id == IDCANCEL) {
                EndDialog(dialog, id);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

}  // namespace

void showFailureReport(const std::wstring& summary, const std::vector<FailureEntry>& entries) {
    std::wstring text = summary;
    for (const FailureEntry& entry : entries) {
        text += L"\r\n\r\n";
        text += entry.path;
        text += L"\r\n    ";
        text += entry.reason;
    }

    ReportContext context;
    context.text = text;

    const std::vector<BYTE> templateData = buildTemplate();
    // vector 连续存储由 operator new 分配（x64 下 16 字节对齐），满足模板的 DWORD 对齐要求
    const INT_PTR result =
        DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                                reinterpret_cast<LPCDLGTEMPLATEW>(templateData.data()),
                                nullptr, reportDialogProc, reinterpret_cast<LPARAM>(&context));
    if (result == -1) {
        // 回退：模板对话框创建失败（极端环境），退化为截断的消息框
        std::wstring fallback = text;
        if (fallback.size() > kMaxFallbackChars) {
            fallback.resize(kMaxFallbackChars);
            fallback += L"\r\n…（内容过长已截断）";
        }
        MessageBoxW(nullptr, fallback.c_str(), kAppTitle, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    }
}

}  // namespace quickmove
