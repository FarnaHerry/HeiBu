#pragma once
// 黑簿 数据网格 — 表头 + virtualList 数据行；支持选中/单元格编辑/NULL 显示。
// 单元格编辑：单击选中、再点进入编辑、Enter/失焦提交（落笔即生效）。
#include <eui_neo.h>

#include <algorithm>

#include "app_actions.h"
#include "ui/theme.h"

namespace heibu::ui {

namespace {

// UTF-8 码点数（按字符边界安全截断用）。
inline std::size_t utf8Length(const std::string& s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > s.size()) len = s.size() - i;
        i += len;
        ++n;
    }
    return n;
}

// 取前 maxCp 个码点的 UTF-8 前缀。
inline std::string utf8Prefix(const std::string& s, std::size_t maxCp) {
    std::string out;
    std::size_t cp = 0;
    for (std::size_t i = 0; i < s.size() && cp < maxCp;) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > s.size()) len = s.size() - i;
        out.append(s, i, len);
        i += len;
        ++cp;
    }
    return out;
}

// 超宽文本截断为「前缀…」：按真实字形宽度测量，CJK/ASCII 均正确。
inline std::string ellipsize(const std::string& text, float maxWidth, float fontSize) {
    if (text.empty()) {
        return text;
    }
    if (maxWidth <= 0.0f) {
        return std::string();
    }
    const float fullW = core::TextPrimitive::measureTextWidth(text, {}, fontSize, 400);
    if (fullW <= maxWidth) {
        return text;
    }
    const std::string ell = "\xE2\x80\xA6";   // "…"
    const float ellW = core::TextPrimitive::measureTextWidth(ell, {}, fontSize, 400);
    const float avail = maxWidth - ellW;
    if (avail <= 0.0f) {
        return ell;
    }
    const std::size_t n = utf8Length(text);
    if (n == 0) {
        return ell;
    }
    const float avgW = fullW / static_cast<float>(n);
    std::size_t k = static_cast<std::size_t>(std::max(0.0f, avail / avgW));
    if (k >= n) {
        k = n - 1;
    }
    while (k > 0) {
        std::string candidate = utf8Prefix(text, k) + ell;
        if (core::TextPrimitive::measureTextWidth(candidate, {}, fontSize, 400) <= maxWidth) {
            return candidate;
        }
        --k;
    }
    return ell;
}

} // namespace

inline void composeGrid(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                        const components::theme::ThemeColorTokens& t) {
    const ResultGrid& grid = *tab.result;
    const std::string tabId = tab.id;
    const float headerH = 26.0f;
    const float rowH = 24.0f;
    const float hScrollH = 12.0f;   // 横向滚动条条高（含上下留白）

    // 横向滚动：所有列宽之和超出视口才需要滚动条。
    float totalW = 0.0f;
    for (const Column& col : grid.columns) {
        totalW += col.width;
    }
    const float maxScrollX = std::max(0.0f, totalW - w);
    const bool needHScroll = maxScrollX > 0.0f;
    const float listH = h - headerH - (needHScroll ? hScrollH : 0.0f);
    const core::Color gridLineColor = components::theme::withOpacity(t.border, t.dark ? 0.22f : 0.30f);

    // 表头
    ui.stack("grid." + tab.id + ".header")
        .position(x, y)
        .size(w, headerH)
        .clip()
        .content([&] {
            float cx = 0.0f;
            for (std::size_t c = 0; c < grid.columns.size(); ++c) {
                const float px = cx - tab.scrollX;
                const float cw = grid.columns[c].width;
                ui.rect("grid." + tab.id + ".h." + std::to_string(c))
                    .position(px, 0.0f)
                    .size(cw, headerH)
                    .color(t.surface);
                ui.text("grid." + tab.id + ".ht." + std::to_string(c))
                    .position(px + 4.0f, 6.0f)
                    .text(ellipsize(grid.columns[c].name, cw - 10.0f, 12.0f))
                    .fontSize(12.0f)
                    .color(t.text);
                // 列分隔竖线
                ui.rect("grid." + tab.id + ".hv." + std::to_string(c))
                    .position(px + cw - 1.0f, 0.0f)
                    .size(1.0f, headerH)
                    .color(gridLineColor);
                // 列宽拖动把手：在列右缘拖动调整宽度。
                ui.rect("grid." + tab.id + ".rh." + std::to_string(c))
                    .position(px + cw - 3.0f, 0.0f)
                    .size(6.0f, headerH)
                    .color({0.0f, 0.0f, 0.0f, 0.0f})
                    .cursor(core::CursorShape::Hand)
                    .interactive()
                    .onDrag([tabId, c](const core::dsl::DragEvent& e) {
                        auto it = S().tabs.find(tabId);
                        if (it == S().tabs.end() || !it->second.result) {
                            return;
                        }
                        auto& cols = it->second.result->columns;
                        if (c >= cols.size()) {
                            return;
                        }
                        // e.deltaX 为物理像素；除以 uiScale（kUI）回到设计逻辑像素。
                        const float delta = static_cast<float>(e.deltaX) / kUI;
                        cols[c].width = std::clamp(cols[c].width + delta, 60.0f, 800.0f);
                        app::requestUpdate();
                    })
                    .build();
                cx += cw;
            }
        })
        .build();

    // 数据行（virtualList）
    const core::Color textColor = t.text;
    const core::Color nullColor = t.border;
    const core::Color selectColor = t.surfaceHover;
    const core::Color editBorder = t.primary;
    const components::theme::ThemeColorTokens theme = t;

    components::virtualList(ui, "grid." + tab.id + ".list")
        .position(x, y + headerH)
        .size(w, listH)
        .itemCount(static_cast<std::int64_t>(grid.rows.size()))
        .rowHeight(rowH)
        .offset(tab.scrollY)
        .theme(t)
        .onChange([tabId](float value) {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                it->second.scrollY = value;
            }
        })
        .row([tabId, textColor, nullColor, selectColor, editBorder, gridLineColor, theme](
                 eui::Ui& ui, const std::string& slotId, std::int64_t rowIdx, float, float rh) {
            auto it = S().tabs.find(tabId);
            if (it == S().tabs.end() || !it->second.result) {
                return;
            }
            const ResultGrid& grid = *it->second.result;
            if (rowIdx < 0 || static_cast<std::size_t>(rowIdx) >= grid.rows.size()) {
                return;
            }
            const std::vector<CellValue>& row = grid.rows[static_cast<std::size_t>(rowIdx)];
            const float scrollX = it->second.scrollX;
            const bool editable = grid.editable && !grid.keys.empty();
            const Tab& tabState = it->second;

            float cx = 0.0f;
            for (std::size_t c = 0; c < row.size() && c < grid.columns.size(); ++c) {
                const float cellX = cx - scrollX;
                const float cellW = grid.columns[c].width;
                const CellValue& cell = row[c];
                const bool isSelected = tabState.selected && tabState.selected->row == rowIdx &&
                                        tabState.selected->col == static_cast<int>(c);
                const bool isEditing = tabState.editing && tabState.editing->row == rowIdx &&
                                       tabState.editing->col == static_cast<int>(c);

                if (isEditing) {
                    components::input(ui, slotId + ".edit")
                        .position(cellX, 0.0f)
                        .size(cellW - 2.0f, rh - 2.0f)
                        .value(tabState.editBuffer)
                        .fontSize(12.0f)
                        .theme(theme)
                        .onChange([tabId](const std::string& v) {
                            auto t = S().tabs.find(tabId);
                            if (t != S().tabs.end()) {
                                t->second.editBuffer = v;
                            }
                        })
                        .onEnter([tabId] { heibu::commitEdit(tabId); })
                        .onFocus([tabId](bool focused) {
                            if (!focused) {
                                heibu::commitEdit(tabId);
                            }
                        })
                        .build();
                } else {
                    if (isSelected && editable) {
                        ui.rect(slotId + ".sel." + std::to_string(c))
                            .position(cellX, 0.0f)
                            .size(cellW, rh)
                            .color(selectColor);
                    }
                    ui.text(slotId + ".c" + std::to_string(c))
                        .position(cellX + 4.0f, 4.0f)
                        .text(cell.isNull ? "NULL" : ellipsize(cell.text, cellW - 8.0f, 12.0f))
                        .fontSize(12.0f)
                        .color(cell.isNull ? nullColor : textColor);
                    if (editable) {
                        ui.rect(slotId + ".hit." + std::to_string(c))
                            .position(cellX, 0.0f)
                            .size(cellW, rh)
                            .color({0.0f, 0.0f, 0.0f, 0.0f})
                            .onClick([tabId, rowIdx, c] { heibu::clickCell(tabId, rowIdx, static_cast<int>(c)); });
                    }
                }
                // 列分隔竖线
                ui.rect(slotId + ".v." + std::to_string(c))
                    .position(cellX + cellW - 1.0f, 0.0f)
                    .size(1.0f, rh)
                    .color(gridLineColor);
                cx += cellW;
            }
        })
        .build();

    // 横向滚动条（自建）：拖动让整行各列在视口内平移。
    if (needHScroll) {
        const std::string sid = "grid." + tab.id + ".hscroll";
        const float thumbH = 4.0f;   // 滚动条粗细
        const float thumbRatio = std::clamp(w / (w + maxScrollX), 0.08f, 1.0f);
        const float thumbW = std::max(24.0f, w * thumbRatio);
        const float value = std::clamp(tab.scrollX / maxScrollX, 0.0f, 1.0f);
        const float thumbY = (hScrollH - thumbH) * 0.5f;
        const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
        const core::Color track = components::theme::withOpacity(t.surfaceHover, t.dark ? 0.34f : 0.46f);
        const core::Color thumb = components::theme::withOpacity(t.text, t.dark ? 0.34f : 0.28f);
        const core::Color thumbHover = components::theme::withOpacity(t.text, t.dark ? 0.46f : 0.38f);
        const core::Color thumbPressed = components::theme::withOpacity(t.primary, 0.76f);

        ui.stack(sid)
            .position(x, y + headerH + listH)
            .size(w, hScrollH)
            .sliderState(sid, value, w, thumbW, [tabId, maxScrollX](float v) {
                auto it = S().tabs.find(tabId);
                if (it != S().tabs.end()) {
                    it->second.scrollX = v * maxScrollX;
                }
                app::requestUpdate();
            })
            .content([&] {
                ui.rect(sid + ".track")
                    .position(0.0f, thumbY)
                    .size(w, thumbH)
                    .color(track)
                    .radius(thumbH * 0.5f)
                    .build();
                ui.rect(sid + ".thumb")
                    .position(0.0f, thumbY)
                    .size(thumbW, thumbH)
                    .states(thumb, thumbHover, thumbPressed)
                    .radius(thumbH * 0.5f)
                    .cursor(core::CursorShape::Hand)
                    .sliderKnobFrom(sid)
                    .build();
                ui.rect(sid + ".hit")
                    .size(w, hScrollH)
                    .states(idle, idle, idle)
                    .interactive()
                    .sliderInputFrom(sid)
                    .build();
            })
            .build();
    }
}

} // namespace heibu::ui
