#pragma once
// 黑簿 OS 主题探测 + 主题模式。Windows 读注册表 AppsUseLightTheme；其余平台回退。
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace heibu {

enum class ThemeMode { System, Dark, Light };

// 探测系统是否深色。Windows：AppsUseLightTheme (HKCU\...\Themes\Personalize)，0=深色 1=浅色。
inline bool osDark() {
#ifdef _WIN32
    // SHGetValueW 是 shlwapi.dll 的导出，动态加载避免显式链接 shlwapi。
    using ShGetValueFn = LSTATUS(WINAPI*)(HKEY, LPCWSTR, LPCWSTR, DWORD*, void*, DWORD*);
    static const ShGetValueFn shGetValue = []() -> ShGetValueFn {
        HMODULE m = LoadLibraryW(L"shlwapi.dll");
        if (!m) {
            return nullptr;
        }
        return reinterpret_cast<ShGetValueFn>(
            reinterpret_cast<void*>(GetProcAddress(m, "SHGetValueW")));
    }();
    if (shGetValue) {
        DWORD type = 0;
        DWORD value = 1;
        DWORD size = sizeof(value);
        const LSTATUS st = shGetValue(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", &type, &value, &size);
        if (st == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(value)) {
            return value == 0;
        }
    }
    return true;   // 默认深色
#else
    return false;  // 非 Windows：默认浅色（后续可按平台补充）
#endif
}

// 由模式算出当前是否深色。
inline bool effectiveDark(ThemeMode mode) {
    switch (mode) {
        case ThemeMode::Dark:   return true;
        case ThemeMode::Light:  return false;
        case ThemeMode::System: return osDark();
    }
    return true;
}

} // namespace heibu
