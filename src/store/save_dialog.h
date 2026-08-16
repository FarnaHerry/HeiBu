#pragma once
// 黑簿 原生「另存为」对话框 + UTF-8 路径写文件 — 仅 Windows。
// eui-neo 只提供打开对话框（openFileDialog），保存路径需自取。
//
// ⚠️ 不要在 Windows 上用 std::ofstream(char* 路径) 写含中文的文件名：C 运行时的
// fopen 系列把 char* 当 ANSI/GBK 解释，UTF-8 中文路径会乱码。写文件必须走宽字符
// API（_wfopen / CreateFileW）。
#include <cstdio>
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

// 以 UTF-8 路径写二进制内容（宽字符 API，兼容中文文件名）。返回是否成功。
inline bool writeFileUtf8(const std::string& utf8Path, const std::string& content) {
#ifdef _WIN32
    wchar_t wpath[1024] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, wpath, 1024) == 0) {
        return false;
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, wpath, L"wb") != 0 || !f) {
        return false;
    }
    const bool ok = fwrite(content.data(), 1, content.size(), f) == content.size();
    fclose(f);
    return ok;
#else
    FILE* f = std::fopen(utf8Path.c_str(), "wb");
    if (!f) {
        return false;
    }
    const bool ok = fwrite(content.data(), 1, content.size(), f) == content.size();
    fclose(f);
    return ok;
#endif
}

} // namespace heibu
