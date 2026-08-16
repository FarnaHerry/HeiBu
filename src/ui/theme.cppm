// 黑簿 UI 主题与布局常量 — 风格取自 tinynext：岛卡片浮层 + 侧栏条目 + uiScale 原生缩放。
module;
#include "eui_ui.h"

export module heibu.ui.theme;
import std;
import heibu.app_state;

export namespace heibu::ui {

// eui-neo 0.5.6 原生全局缩放：所有尺寸按「设计逻辑像素」书写，窗口物理尺寸 = 设计 × kUI。
inline constexpr float kUI = 1.4f;
inline constexpr float kDesignW = 1200.0f;   // 设计逻辑窗口宽
inline constexpr float kDesignH = 760.0f;    // 设计逻辑窗口高

// 布局常量（设计逻辑像素）
inline constexpr float kSidebarW = 280.0f;   // 大侧边栏：标签 + 命籍/卷宗选择器
inline constexpr float kStatusH = 24.0f;
inline constexpr float kIslandGap = 8.0f;    // 岛卡片与窗口边缘的间距
inline constexpr float kIslandRadius = 10.0f;

inline components::theme::ThemeColorTokens currentTheme() {
    return S().darkTheme ? components::theme::dark() : components::theme::light();
}

// 岛卡片底色：background↔surface 的中间调，与纯 surface 内层卡片分层。
inline core::Color panelColor(const components::theme::ThemeColorTokens& t) {
    return core::mixColor(t.background, t.surface, 0.5f);
}

// 连接类型对应的图标颜色（侧边栏圆点 / 连接对话框卡片）。
inline core::Color connectionIconColor(const std::string& driver) {
    if (driver == "mysql" || driver == "mariadb") {
        return {0.95f, 0.55f, 0.15f, 1.0f};   // MySQL 橙
    }
    if (driver == "postgres" || driver == "postgresql" || driver == "pg") {
        return {0.20f, 0.45f, 0.85f, 1.0f};   // PostgreSQL 蓝
    }
    if (driver == "redis") {
        return {0.90f, 0.25f, 0.20f, 1.0f};   // Redis 红
    }
    return {0.35f, 0.65f, 0.55f, 1.0f};       // SQLite 青绿
}

// 岛卡片背景：圆角 + 细边框 + 柔和投影，浮在页面背景上。
inline void drawPanel(eui::Ui& ui, const std::string& id, float x, float y, float w, float h,
                      const components::theme::ThemeColorTokens& t) {
    const core::Color shadow = t.dark ? core::Color{0.0f, 0.0f, 0.0f, 0.25f}
                                      : core::Color{0.10f, 0.14f, 0.22f, 0.12f};
    ui.rect(id)
        .position(x, y)
        .size(w, h)
        .color(panelColor(t))
        .radius(kIslandRadius)
        .border(1.0f, components::theme::withOpacity(t.border, 0.6f))
        .shadow(14.0f, 3.0f, shadow)
        .build();
}

// 侧栏条目：前缀（箭头等）+ 文字，激活时主色高亮 + 左侧竖条，hover/按下反馈。
// iconColor 非透明时，在最左侧画一个彩色圆点（连接类型图标）。
inline void drawSidebarItem(eui::Ui& ui, const std::string& id, float x, float y, float w,
                            float h, const std::string& label, bool active,
                            const components::theme::ThemeColorTokens& t,
                            std::function<void()> onClick, const std::string& prefix = "",
                            std::function<void(const core::PointerEvent&, const core::Rect&)> onContextMenu = {},
                            core::Color iconColor = {}) {
    const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
    const core::Color activeFill =
        components::theme::withAlpha(t.primary, t.dark ? 0.22f : 0.14f);
    const core::Color textColor = active ? t.primary : t.text;
    const bool hasIcon = iconColor.a > 0.0f;
    const float iconW = hasIcon ? 14.0f : 0.0f;

    ui.rect(id + ".bg")
        .position(x, y).size(w, h)
        .color(active ? activeFill : idle)
        .radius(8.0f)
        .build();
    if (active) {
        ui.rect(id + ".bar")
            .position(x, y + 6.0f).size(3.0f, h - 12.0f)
            .color(t.primary).radius(1.5f)
            .build();
    }
    if (hasIcon) {
        ui.rect(id + ".icon")
            .position(x + 4.0f, y + (h - 8.0f) * 0.5f)
            .size(8.0f, 8.0f)
            .color(iconColor)
            .radius(4.0f)
            .build();
    }
    ui.rect(id + ".hit")
        .position(x, y).size(w, h)
        .states(idle, active ? idle : t.surfaceHover, t.surfaceActive)
        .radius(8.0f)
        .onClick(std::move(onClick))
        .onContextMenu(std::move(onContextMenu))
        .build();
    ui.text(id + ".label")
        .position(x + 10.0f + iconW, y).size(w - 20.0f - iconW, h)
        .text(prefix + label)
        .fontSize(12.0f)
        .lineHeight(h)
        .color(textColor)
        .verticalAlign(core::VerticalAlign::Center)
        .build();
}

} // namespace heibu::ui
