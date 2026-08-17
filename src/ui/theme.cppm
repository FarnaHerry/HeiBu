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
inline constexpr float kRedisSidebarW = 220.0f;   // Redis 键树窗格宽
inline constexpr float kStatusH = 24.0f;
inline constexpr float kIslandGap = 8.0f;    // 岛卡片与窗口边缘的间距
inline constexpr float kIslandRadius = 10.0f;

// 毛玻璃：岛卡半透明填充 + 背景模糊；背后渐变提供可模糊的层次。
inline constexpr float kFrostAlpha = 0.72f;   // 岛卡填充透明度
inline constexpr float kFrostBlur = 18.0f;    // 背景模糊半径

// 根背景渐变（深/浅主题自适应），毛玻璃背后有层次可模糊。
inline core::Gradient rootGradient(const components::theme::ThemeColorTokens& t) {
    if (t.dark) {
        // 极客黑：深灰顶 → 近黑底，纯单色层次。
        const core::Color top = core::mixColor(t.background, t.primary, 0.18f);
        return {true, top, t.background, core::GradientDirection::Vertical};
    }
    // 极客白：浅灰顶 → 近白底。
    const core::Color top = core::mixColor(t.background, t.primary, 0.05f);
    return {true, top, t.background, core::GradientDirection::Vertical};
}

// AI 极客风 · 黑：近纯黑底 + 白字 + 纯白 accent（主按钮反转为黑字）。
inline components::theme::ThemeColorTokens geekBlack() {
    components::theme::ThemeColorTokens t = components::theme::dark();
    t.background    = {0.04f, 0.04f, 0.05f, 1.0f};
    t.surface       = {0.10f, 0.10f, 0.11f, 1.0f};
    t.surfaceHover  = {0.18f, 0.18f, 0.19f, 1.0f};
    t.surfaceActive = {0.28f, 0.28f, 0.29f, 1.0f};
    t.text          = {0.95f, 0.95f, 0.96f, 1.0f};
    t.border        = {0.50f, 0.50f, 0.50f, 1.0f};   // 中灰：兼作次要文字，深色下可读
    t.primary       = {1.0f, 1.0f, 1.0f, 1.0f};
    t.dark          = true;
    return t;
}

// AI 极客风 · 白：近纯白底 + 黑字 + 纯黑 accent（主按钮白字）。
inline components::theme::ThemeColorTokens geekWhite() {
    components::theme::ThemeColorTokens t = components::theme::light();
    t.background    = {0.96f, 0.96f, 0.96f, 1.0f};
    t.surface       = {1.0f, 1.0f, 1.0f, 1.0f};
    t.surfaceHover  = {0.90f, 0.90f, 0.90f, 1.0f};
    t.surfaceActive = {0.82f, 0.82f, 0.82f, 1.0f};
    t.text          = {0.05f, 0.05f, 0.05f, 1.0f};
    t.border        = {0.55f, 0.55f, 0.55f, 1.0f};   // 中灰：兼作次要文字，浅色下可读
    t.primary       = {0.0f, 0.0f, 0.0f, 1.0f};
    t.dark          = false;
    return t;
}

inline components::theme::ThemeColorTokens currentTheme() {
    return S().darkTheme ? geekBlack() : geekWhite();
}

// 主按钮文字：深色主题黑字（白底反白），浅色主题白字（黑底）。
inline core::Color onPrimaryText(const components::theme::ThemeColorTokens& t) {
    return t.dark ? core::Color{0.03f, 0.03f, 0.03f, 1.0f}
                  : core::Color{0.95f, 0.95f, 0.95f, 1.0f};
}

// 按钮文字：primary（选中/主）反色保证可读；次按钮沿用默认（深=近白字、浅=黑字）。
inline core::Color buttonTextColor(const components::theme::ThemeColorTokens& t, bool primary) {
    if (primary) {
        return onPrimaryText(t);
    }
    return t.dark ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : t.text;
}

// 岛卡片底色：background↔surface 的中间调，与纯 surface 内层卡片分层。
inline core::Color panelColor(const components::theme::ThemeColorTokens& t) {
    return core::mixColor(t.background, t.surface, 0.5f);
}

// 连接类型对应的图标颜色（侧边栏圆点 / 连接对话框卡片）：保留各数据库品牌色。
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

// 岛卡片背景：半透明 + 背景模糊（毛玻璃）+ 圆角 + 细边框 + 柔和投影，浮在渐变背景上。
inline void drawPanel(eui::Ui& ui, const std::string& id, float x, float y, float w, float h,
                      const components::theme::ThemeColorTokens& t) {
    const core::Color shadow = t.dark ? core::Color{0.0f, 0.0f, 0.0f, 0.25f}
                                      : core::Color{0.10f, 0.14f, 0.22f, 0.12f};
    ui.rect(id)
        .position(x, y)
        .size(w, h)
        .color(components::theme::withOpacity(panelColor(t), kFrostAlpha))
        .blur(kFrostBlur)
        .radius(kIslandRadius)
        .border(1.0f, components::theme::withOpacity(t.border, 0.6f))
        .shadow(14.0f, 3.0f, shadow)
        .build();
}

// 侧栏条目：前缀（箭头等）+ 文字，激活时主色高亮 + 左侧竖条，hover/按下反馈。
// iconColor 非透明时画彩色圆点（连接类型图标）；faIcon 非 0 时画 Font Awesome 字形（表/视图等，随主题色）。
inline void drawSidebarItem(eui::Ui& ui, const std::string& id, float x, float y, float w,
                            float h, const std::string& label, bool active,
                            const components::theme::ThemeColorTokens& t,
                            std::function<void()> onClick, const std::string& prefix = "",
                            std::function<void(const core::PointerEvent&, const core::Rect&)> onContextMenu = {},
                            core::Color iconColor = {},
                            unsigned int faIcon = 0) {
    const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
    const core::Color activeFill =
        components::theme::withAlpha(t.primary, t.dark ? 0.22f : 0.14f);
    const core::Color textColor = active ? t.primary : t.text;
    const bool hasIcon = iconColor.a > 0.0f;
    const bool showIcon = hasIcon || faIcon != 0;
    const float iconW = showIcon ? 14.0f : 0.0f;

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
    if (faIcon != 0) {
        // Font Awesome 字形图标：随主题色的半透明文字，跟随深/浅模式。
        ui.text(id + ".faicon")
            .position(x + 4.0f, y).size(14.0f, h)
            .icon(faIcon)
            .fontSize(12.0f)
            .color(components::theme::withOpacity(t.text, 0.55f))
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();
    } else if (hasIcon) {
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

// 毛玻璃对话框样式：半透明面板 + 更贴合玻璃的圆角，透出背后已模糊的渐变。
inline components::DialogStyle dialogGlassStyle(const components::theme::ThemeColorTokens& t) {
    components::DialogStyle s(t);
    s.surface = components::theme::withOpacity(t.surface, 0.82f);
    s.radius = 14.0f;
    return s;
}

// 毛玻璃 toast：半透明背景（底下内容区本身已是毛玻璃，透出即玻璃观感）。
inline components::ToastStyle toastGlassStyle(const components::theme::ThemeColorTokens& t) {
    components::ToastStyle s(t);
    s.background = components::theme::withOpacity(s.background, 0.80f);
    return s;
}

// 毛玻璃菜单项。
struct GlassMenuItem {
    std::string text;
    bool danger = false;
};

// 毛玻璃浮层菜单（官方 demo blur 配方）：半透明 surface + 背景模糊 + 投影。
// onDismiss 负责关掉开关 + requestUpdate；onSelect(i) 处理选中第 i 项。
inline void composeGlassMenu(eui::Ui& ui, const std::string& id, float screenW, float screenH,
                             float x, float y, float itemW, float itemH, int zIndex,
                             const std::vector<GlassMenuItem>& items,
                             const components::theme::ThemeColorTokens& t,
                             std::function<void()> onDismiss,
                             std::function<void(int)> onSelect) {
    const float inset = 6.0f;
    const float sepGap = 5.0f;
    float menuH = inset * 2.0f;
    for (const GlassMenuItem& it : items) {
        menuH += it.danger ? sepGap + itemH : itemH;
    }
    x = std::clamp(x, 8.0f, std::max(8.0f, screenW - itemW - 8.0f));
    y = std::clamp(y, 8.0f, std::max(8.0f, screenH - menuH - 8.0f));
    const core::Color danger = {0.92f, 0.33f, 0.33f, 1.0f};
    const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
    const core::Color shadow = t.dark ? core::Color{0.0f, 0.0f, 0.0f, 0.40f}
                                      : core::Color{0.10f, 0.14f, 0.22f, 0.12f};

    ui.stack(id)
        .position(0.0f, 0.0f).size(screenW, screenH)
        .zIndex(zIndex)
        .content([&] {
            ui.rect(id + ".dismiss").size(screenW, screenH).color(idle)
                .onClick(onDismiss)
                .build();

            ui.stack(id + ".menu")
                .position(x, y).size(itemW, menuH)
                .content([&] {
                    ui.rect(id + ".bg").size(itemW, menuH)
                        .color(components::theme::withOpacity(t.surface, 0.72f))
                        .blur(kFrostBlur)
                        .radius(12.0f)
                        .border(1.0f, components::theme::withOpacity(t.border, 0.82f))
                        .shadow(14.0f, 3.0f, shadow)
                        .build();

                    float iy = inset;
                    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                        if (items[i].danger) {
                            ui.rect(id + ".sep")
                                .position(inset, iy).size(itemW - inset * 2.0f, 1.0f)
                                .color(components::theme::withOpacity(t.border, 0.5f))
                                .build();
                            iy += sepGap;
                        }
                        const core::Color tc = items[i].danger ? danger : t.text;
                        ui.rect(id + ".item." + std::to_string(i))
                            .position(inset, iy).size(itemW - inset * 2.0f, itemH)
                            .states(idle, t.surfaceHover, t.surfaceActive)
                            .radius(8.0f)
                            .onClick([i, onDismiss, onSelect] {
                                onDismiss();
                                onSelect(i);
                            })
                            .build();
                        ui.text(id + ".label." + std::to_string(i))
                            .position(inset + 10.0f, iy).size(itemW - inset * 2.0f - 20.0f, itemH)
                            .text(items[i].text)
                            .fontSize(13.0f)
                            .color(tc)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                        iy += itemH;
                    }
                })
                .build();
        })
        .build();
}

} // namespace heibu::ui
