#pragma once
// 黑簿 原生「另存为」对话框 — 仅 Windows（GetSaveFileNameW）。非 Windows 返回空串（由调用方回退）。
// eui-neo 只提供打开对话框（openFileDialog），保存路径需自取。
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace heibu {

// 弹出原生保存对话框。filterName/默认扩展名/默认文件名按 UTF-8 传入。
// 返回用户选择的完整路径；取消或失败返回空串。
inline std::string saveFileDialog(const std::string& /*title*/,
                                  const std::string& /*filterName*/,
                                  const std::string& defaultExt,
                                  const std::string& defaultName) {
#ifdef _WIN32
    wchar_t buf[1024] = {0};
    MultiByteToWideChar(CP_UTF8, 0, defaultName.c_str(), -1, buf, 1024);
    const std::wstring defExt = std::wstring(defaultExt.begin(), defaultExt.end());
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"CSV 文件 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = defExt.empty() ? L"csv" : defExt.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&ofn)) {
        const std::wstring ws(buf);
        const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                                            nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<std::size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                            &out[0], len, nullptr, nullptr);
        return out;
    }
#else
    (void)defaultExt;
    (void)defaultName;
#endif
    return std::string();
}

} // namespace heibu
