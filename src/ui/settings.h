#pragma once
// 黑簿 设置页 — 主题模式（跟随系统/深色/浅色）+ 语言（中文/English）。
#include <eui_neo.h>

#include "app_state.h"
#include "ui/theme.h"

namespace heibu::ui {

inline void composeSettings(eui::Ui& ui, float x, float y, float w, float h,
                            const components::theme::ThemeColorTokens& t) {
    const float btnW = 96.0f;
    const float btnH = 28.0f;

    ui.text("settings.title")
        .position(x + 8.0f, y + 8.0f)
        .text(std::string(L(StrId::Settings)))
        .fontSize(17.0f)
        .color(t.text);

    // ── 主题模式 ──
    ui.text("settings.theme_label")
        .position(x + 8.0f, y + 48.0f)
        .text(std::string(L(StrId::ThemeModeLabel)))
        .fontSize(12.0f)
        .color(t.border);

    const struct {
        StrId label;
        ThemeMode mode;
    } themes[] = {
        {StrId::FollowSystem, ThemeMode::System},
        {StrId::ThemeDark, ThemeMode::Dark},
        {StrId::ThemeLight, ThemeMode::Light},
    };
    for (int i = 0; i < 3; ++i) {
        const bool active = S().themeMode == themes[i].mode;
        components::button(ui, "settings.theme." + std::to_string(i))
            .position(x + 8.0f + static_cast<float>(i) * (btnW + 6.0f), y + 68.0f)
            .size(btnW, btnH)
            .text(std::string(L(themes[i].label)))
            .fontSize(12.0f)
            .theme(t, active)
            .radius(7.0f)
            .onClick([mode = themes[i].mode] {
                heibu::setThemeMode(mode);
                app::requestUpdate();
            })
            .build();
    }

    // ── 语言 ──
    ui.text("settings.lang_label")
        .position(x + 8.0f, y + 116.0f)
        .text(std::string(L(StrId::LanguageLabel)))
        .fontSize(12.0f)
        .color(t.border);

    const struct {
        StrId label;
        Locale loc;
    } langs[] = {
        {StrId::LanguageZh, Locale::Zh},
        {StrId::LanguageEn, Locale::En},
    };
    for (int i = 0; i < 2; ++i) {
        const bool active = S().locale == langs[i].loc;
        components::button(ui, "settings.lang." + std::to_string(i))
            .position(x + 8.0f + static_cast<float>(i) * (btnW + 6.0f), y + 136.0f)
            .size(btnW, btnH)
            .text(std::string(L(langs[i].label)))
            .fontSize(12.0f)
            .theme(t, active)
            .radius(7.0f)
            .onClick([loc = langs[i].loc] {
                S().locale = loc;
                app::requestUpdate();
            })
            .build();
    }
}

} // namespace heibu::ui
