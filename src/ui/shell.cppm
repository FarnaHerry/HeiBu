// 黑簿 根布局 — 大侧边栏（标签 + 命籍/卷宗选择器）+ 内容岛卡片 + 状态栏 + 对话框。
module;
#include "eui_ui.h"

export module heibu.ui.shell;
import std;
import heibu.app_actions;
import heibu.app_state;
import heibu.db.types;
import heibu.i18n;
import heibu.ui.dialogs;
import heibu.ui.editor;
import heibu.ui.grid;
import heibu.ui.settings;
import heibu.ui.sidebar;
import heibu.ui.theme;

export namespace heibu::ui {

// 可编辑表格的操作条（置为 NULL / 勾销此名 / 添一笔）。
inline void composeTableActions(eui::Ui& ui, const Tab& tab, float x, float y, float w,
                                const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    const bool hasSel = tab.selected.has_value();
    ui.rect("act." + tabId).position(x, y).size(w, 30.0f).color(panelColor(t));
    components::button(ui, "act.null." + tabId)
        .position(x + 8.0f, y + 3.0f).size(84.0f, 24.0f)
        .text(std::string(L(StrId::SetNull))).fontSize(12.0f).theme(t, false)
        .disabled(!hasSel)
        .onClick([tabId] { heibu::setCellNull(tabId); })
        .build();
    components::button(ui, "act.del." + tabId)
        .position(x + 96.0f, y + 3.0f).size(84.0f, 24.0f)
        .text(std::string(L(StrId::ConfirmDelete))).fontSize(12.0f).theme(t, false)
        .disabled(!hasSel)
        .onClick([tabId] { heibu::requestDeleteRow(tabId); })
        .build();
    components::button(ui, "act.add." + tabId)
        .position(x + 184.0f, y + 3.0f).size(84.0f, 24.0f)
        .text("＋ " + std::string(L(StrId::AddRow))).fontSize(12.0f).theme(t, true)
        .disabled(tab.dirty)
        .onClick([tabId] { heibu::startAddRow(tabId); })
        .build();
    if (tab.dirty) {
        components::button(ui, "act.commit." + tabId)
            .position(x + 272.0f, y + 3.0f).size(70.0f, 24.0f)
            .text(std::string(L(StrId::Save))).fontSize(12.0f).theme(t, true)
            .onClick([tabId] { heibu::commitChanges(tabId); })
            .build();
        components::button(ui, "act.cancel." + tabId)
            .position(x + 346.0f, y + 3.0f).size(70.0f, 24.0f)
            .text(std::string(L(StrId::Cancel))).fontSize(12.0f).theme(t, false)
            .onClick([tabId] { heibu::cancelChanges(tabId); })
            .build();
    }
}

// 勾销（删除行）确认框。
inline void composeDeleteConfirm(eui::Ui& ui, float w, float h,
                                 const components::theme::ThemeColorTokens& t) {
    const std::string tabId = S().activeTabId ? *S().activeTabId : std::string();
    components::dialog(ui, "delete_confirm")
        .open(S().showDeleteConfirm)
        .screen(w, h)
        .size(400.0f, 190.0f)
        .theme(t)
        .zIndex(101)
        .title(std::string(L(StrId::ConfirmDelete)))
        .message(std::string(L(StrId::ConfirmDeleteMsg)))
        .primaryText(std::string(L(StrId::Delete)))
        .secondaryText(std::string(L(StrId::Cancel)))
        .onPrimary([tabId] { heibu::confirmDeleteRow(tabId); })
        .onSecondary([] {
            S().showDeleteConfirm = false;
            app::requestUpdate();
        })
        .build();
}

// 分页栏：首页/上一页/下一页/末页 + 页码指示 + 每页行数选择 + 导出 CSV。
inline void composePaginationBar(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                                 const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    const std::int64_t totalPages =
        tab.totalRows > 0 ? (tab.totalRows + tab.pageSize - 1) / tab.pageSize : 0;
    const bool hasPrev = tab.page > 0;
    const bool hasNext = tab.totalRows > 0 ? (tab.page + 1 < totalPages) : true;

    const float btnW = 38.0f;
    const float gap = 4.0f;
    const float jumpW = 44.0f;   // 固定宽度
    const float firstX = x;
    const float prevX = firstX + btnW + gap;
    const float jumpX = prevX + btnW + gap;
    const float nextX = jumpX + jumpW + gap;
    const float lastX = nextX + btnW + gap;
    const float infoX = lastX + btnW + 8.0f;

    components::button(ui, "page.first." + tabId)
        .position(firstX, y + 4.0f).size(btnW, 24.0f).text("⏮").fontSize(12.0f).theme(t, false)
        .disabled(!hasPrev).onClick([tabId] { heibu::goToPage(tabId, 0); }).build();
    components::button(ui, "page.prev." + tabId)
        .position(prevX, y + 4.0f).size(btnW, 24.0f).text("◀").fontSize(12.0f).theme(t, false)
        .disabled(!hasPrev)
        .onClick([tabId] {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                heibu::goToPage(tabId, it->second.page - 1);
            }
        })
        .build();
    components::input(ui, "page.jump." + tabId)
        .position(jumpX, y + 4.0f).size(jumpW, 24.0f)
        .value(tab.pageInput)
        .fontSize(11.0f)
        .theme(t)
        .onChange([tabId](const std::string& v) {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                it->second.pageInput = v;
            }
        })
        .onEnter([tabId] {
            auto it = S().tabs.find(tabId);
            if (it == S().tabs.end()) {
                return;
            }
            int p = 0;
            try {
                p = std::stoi(it->second.pageInput);
            } catch (...) {
            }
            if (p >= 1) {
                heibu::goToPage(tabId, p - 1);
            } else {
                it->second.pageInput = std::to_string(it->second.page + 1);
                app::requestUpdate();
            }
        })
        .build();
    components::button(ui, "page.next." + tabId)
        .position(nextX, y + 4.0f).size(btnW, 24.0f).text("▶").fontSize(12.0f).theme(t, false)
        .disabled(!hasNext)
        .onClick([tabId] {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                heibu::goToPage(tabId, it->second.page + 1);
            }
        })
        .build();
    components::button(ui, "page.last." + tabId)
        .position(lastX, y + 4.0f).size(btnW, 24.0f).text("⏭").fontSize(12.0f).theme(t, false)
        .disabled(!hasNext)
        .onClick([tabId] {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end() && it->second.totalRows > 0) {
                heibu::goToPage(tabId, (it->second.totalRows - 1) / it->second.pageSize);
            }
        })
        .build();

    if (tab.totalRows >= 0) {
        std::string info = "共 " + std::to_string(tab.totalRows) + " 行";
        if (totalPages > 0) {
            info += " · 共 " + std::to_string(totalPages) + " 页";
        }
        ui.text("page.info." + tabId).position(infoX, y + 8.0f)
            .text(info).fontSize(12.0f).color(t.text);
    }

    // 每页行数下拉（让位给右侧导出按钮）
    const float sx = x + w - 100.0f - 88.0f - 4.0f;
    components::button(ui, "page.size." + tabId)
        .position(sx, y + 4.0f).size(96.0f, 24.0f)
        .text("每页 " + std::to_string(tab.pageSize) + " 行 ▾").fontSize(11.0f).theme(t, false)
        .onClick([tabId, sx, y] {
            S().pageSizeMenuOpen = true;
            S().pageSizeMenuTabId = tabId;
            S().pageSizeMenuX = sx;
            S().pageSizeMenuY = y + 4.0f;   // 附着在按钮顶，实际位置由菜单组件按 item 数计算
            app::requestUpdate();
        })
        .build();

    // 导出按钮：页数选择后面（分页栏最右下角），上拉格式菜单。
    const float ex = x + w - 88.0f;
    components::button(ui, "page.export." + tabId)
        .position(ex, y + 4.0f).size(84.0f, 24.0f)
        .text("导出 ▴").fontSize(11.0f).theme(t, false)
        .onClick([ex, y] {
            S().exportMenuOpen = true;
            S().exportMenuX = ex;
            S().exportMenuY = y + 4.0f;   // 附着在按钮顶，上拉
            S().exportMenuUp = true;
            app::requestUpdate();
        })
        .build();
}

// 结果区：running / 错误 / DML 影响行数 / 空 / 网格。
inline void composeResultArea(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                              const components::theme::ThemeColorTokens& t) {
    if (tab.running) {
        ui.text("result." + tab.id + ".running").position(x + 16.0f, y + 16.0f)
            .text(std::string(L(StrId::Running))).fontSize(14.0f).color(t.text);
        return;
    }
    if (!tab.lastError.empty()) {
        ui.text("result." + tab.id + ".error").position(x + 16.0f, y + 16.0f)
            .text(tab.lastError).fontSize(13.0f).color({0.92f, 0.33f, 0.33f, 1.0f});
        return;
    }
    if (!tab.result) {
        return;
    }
    const ResultGrid& grid = *tab.result;
    if (grid.columns.empty() && grid.affectedRows >= 0) {
        ui.text("result." + tab.id + ".affected").position(x + 16.0f, y + 16.0f)
            .text(heibu::fillFirst(L(StrId::AffectedRows), std::to_string(grid.affectedRows)))
            .fontSize(14.0f).color(t.text);
        return;
    }
    if (grid.columns.empty()) {
        ui.text("result." + tab.id + ".empty").position(x + 16.0f, y + 16.0f)
            .text("0 " + std::string(L(StrId::Rows))).fontSize(14.0f).color(t.border);
        return;
    }
    const bool editable = grid.editable && !grid.keys.empty();
    const bool paginated = tab.kind == TabKind::Table;
    const float pageH = paginated ? 32.0f : 0.0f;
    float gridY = y;
    float gridH = h;
    if (editable) {
        const float barH = 30.0f;
        composeTableActions(ui, tab, x, y, w, t);
        gridY = y + barH;
        gridH = h - barH - pageH;
    } else {
        gridH = h - pageH;
    }
    composeGrid(ui, tab, x, gridY, w, gridH, t);
    if (paginated) {
        composePaginationBar(ui, tab, x, y + h - pageH, w, pageH, t);
    }
}

// 查卷标签：工具栏（批阅 + 导出 CSV）+ 编辑器 + 结果区。
inline void composeQueryTab(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                            const components::theme::ThemeColorTokens& t) {
    const float toolbarH = 36.0f;
    const float editorH = std::max(80.0f, (h - toolbarH) * 0.4f);

    ui.rect("qtool." + tab.id).position(x, y).size(w, toolbarH).color(panelColor(t));
    components::button(ui, "run." + tab.id)
        .position(x + 12.0f, y + 5.0f).size(72.0f, 26.0f)
        .text(std::string(L(StrId::Run))).fontSize(13.0f).theme(t, true)
        .onClick([tabId = tab.id] { heibu::runQueryTab(tabId); })
        .build();
    // 查询页无分页栏，导出按钮放工具栏（菜单下拉）
    components::button(ui, "export." + tab.id)
        .position(x + 92.0f, y + 5.0f).size(72.0f, 26.0f)
        .text("导出 ▾").fontSize(12.0f).theme(t, false)
        .onClick([ex = x + 92.0f, ey = y + 5.0f + 26.0f] {
            S().exportMenuOpen = true;
            S().exportMenuX = ex;
            S().exportMenuY = ey;   // 附着在按钮底，下拉
            S().exportMenuUp = false;
            app::requestUpdate();
        })
        .build();

    composeEditor(ui, tab, x, y + toolbarH, w, editorH, t);
    composeResultArea(ui, tab, x, y + toolbarH + editorH, w, h - toolbarH - editorH, t);
}

// 标签条（内容卡片顶部）：打开标签 + 关闭 + 溢出时顶部横向滚动条。返回占用高度（含下方间距）。
inline float composeTabstrip(eui::Ui& ui, float x, float y, float w,
                             const components::theme::ThemeColorTokens& t) {
    const float tabH = 26.0f;
    const float tabW = 130.0f;
    const float gap = 4.0f;
    const float scrollH = 12.0f;   // 横向滚动条条高（含留白）
    AppState& s = S();

    // 溢出检测：所有标签总宽超出视口才需要滚动条。
    float totalW = 0.0f;
    for (const std::string& tabId : s.openTabIds) {
        if (s.tabs.count(tabId)) {
            totalW += tabW + gap;
        }
    }
    if (totalW > 0.0f) {
        totalW -= gap;   // 去掉最后一个间隙
    }
    const float maxScrollX = std::max(0.0f, totalW - w);
    const bool needScroll = maxScrollX > 0.0f;
    const float scrollX = std::clamp(s.tabScrollX, 0.0f, maxScrollX);
    const float tabsY = y + (needScroll ? scrollH + 4.0f : 0.0f);

    // 标签行：裁剪到可视区，溢出标签被裁掉不可见/不可点。
    ui.stack("tabstrip.clip")
        .position(x, tabsY)
        .size(w, tabH)
        .clip()
        .content([&] {
            float tabX = -scrollX;
            for (const std::string& tabId : s.openTabIds) {
                auto it = s.tabs.find(tabId);
                if (it == s.tabs.end()) {
                    continue;
                }
                const bool active = s.activeTabId && *s.activeTabId == tabId;
                ui.rect("tab." + tabId + ".bg")
                    .position(tabX, 0.0f).size(tabW, tabH)
                    .color(active ? t.surfaceActive : t.surface)
                    .radius(6.0f);
                ui.rect("tab." + tabId + ".hit")
                    .position(tabX, 0.0f).size(tabW, tabH)
                    .color({0.0f, 0.0f, 0.0f, 0.0f})
                    .onClick([tabId] {
                        S().activeTabId = tabId;
                        app::requestUpdate();
                    })
                    .onContextMenu([tabId](const core::PointerEvent& e, const core::Rect&) {
                        S().tabCtxId = tabId;
                        S().tabCtxX = static_cast<float>(e.x);
                        S().tabCtxY = static_cast<float>(e.y);
                        S().tabCtxOpen = true;
                        app::requestUpdate();
                    });
                ui.text("tab." + tabId + ".title")
                    .position(tabX + 8.0f, 5.0f)
                    .text(it->second.title)
                    .fontSize(12.0f)
                    .color(t.text);
                // 圆形关闭按钮：气泡底在 hover 时显示
                ui.rect("tab." + tabId + ".close.bg")
                    .position(tabX + tabW - 24.0f, 5.0f)
                    .size(16.0f, 16.0f)
                    .states({0.0f, 0.0f, 0.0f, 0.0f}, t.surfaceHover, t.surfaceActive)
                    .radius(8.0f)
                    .onClick([tabId] { heibu::closeTab(tabId); })
                    .build();
                ui.text("tab." + tabId + ".close")
                    .position(tabX + tabW - 24.0f, 5.0f)
                    .size(16.0f, 16.0f)
                    .text("×")
                    .fontSize(13.0f)
                    .color(t.border)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .build();
                tabX += tabW + gap;
            }
        })
        .build();

    // 顶部横向滚动条
    if (needScroll) {
        const std::string sid = "tabstrip.hscroll";
        const float thumbH = 4.0f;
        const float thumbRatio = std::clamp(w / (w + maxScrollX), 0.08f, 1.0f);
        const float thumbW = std::max(24.0f, w * thumbRatio);
        const float value = std::clamp(scrollX / maxScrollX, 0.0f, 1.0f);
        const float thumbY = (scrollH - thumbH) * 0.5f;
        const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
        const core::Color track = components::theme::withOpacity(t.surfaceHover, t.dark ? 0.34f : 0.46f);
        const core::Color thumb = components::theme::withOpacity(t.text, t.dark ? 0.34f : 0.28f);
        const core::Color thumbHover = components::theme::withOpacity(t.text, t.dark ? 0.46f : 0.38f);
        const core::Color thumbPressed = components::theme::withOpacity(t.primary, 0.76f);

        ui.stack(sid)
            .position(x, y)
            .size(w, scrollH)
            .sliderState(sid, value, w, thumbW, [maxScrollX](float v) {
                S().tabScrollX = v * maxScrollX;
                app::requestUpdate();
            })
            .content([&] {
                ui.rect(sid + ".track")
                    .position(0.0f, thumbY).size(w, thumbH)
                    .color(track).radius(thumbH * 0.5f)
                    .build();
                ui.rect(sid + ".thumb")
                    .position(0.0f, thumbY).size(thumbW, thumbH)
                    .states(thumb, thumbHover, thumbPressed)
                    .radius(thumbH * 0.5f)
                    .cursor(core::CursorShape::Hand)
                    .sliderKnobFrom(sid)
                    .build();
                ui.rect(sid + ".hit")
                    .size(w, scrollH)
                    .states(idle, idle, idle)
                    .interactive()
                    .sliderInputFrom(sid)
                    .build();
            })
            .build();
    }

    return tabH + (needScroll ? scrollH + 4.0f : 0.0f) + 8.0f;
}

inline void composeContent(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    const float cx = kIslandGap + kSidebarW + kIslandGap;
    const float cy = kIslandGap;
    const float cw = w - cx - kIslandGap;
    const float ch = h - kIslandGap * 2.0f;
    const float pad = 10.0f;

    drawPanel(ui, "content.panel", cx, cy, cw, ch, t);

    const float ix = cx + pad;
    const float iy = cy + pad;
    const float iw = cw - pad * 2.0f;
    const float ih = ch - pad * 2.0f;

    const float tabH = composeTabstrip(ui, ix, iy, iw, t);
    const float ry = iy + tabH;
    const float rh = ih - tabH;

    AppState& s = S();
    if (!s.activeTabId) {
        ui.text("content.none").position(ix + 8.0f, ry + 8.0f)
            .text(std::string(L(StrId::Ready))).fontSize(15.0f).color(t.text);
        return;
    }
    auto it = s.tabs.find(*s.activeTabId);
    if (it == s.tabs.end()) {
        ui.text("content.none").position(ix + 8.0f, ry + 8.0f)
            .text(std::string(L(StrId::Ready))).fontSize(15.0f).color(t.text);
        return;
    }
    const Tab& tab = it->second;
    if (tab.kind == TabKind::Settings) {
        composeSettings(ui, ix, ry, iw, rh, t);
    } else if (tab.kind == TabKind::Query) {
        composeQueryTab(ui, tab, ix, ry, iw, rh, t);
    } else {
        composeResultArea(ui, tab, ix, ry, iw, rh, t);
    }
}

// 侧边栏右键菜单：连接级（新建表/新建查询/刷新/关闭命籍）或表级（删除表/刷新）。
inline void composeContextMenu(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    if (!S().ctxMenuOpen) {
        return;
    }
    // 当前右键命中的连接是否为 Redis（决定「表级」菜单是「表」还是「键」）。
    bool redisCtx = false;
    for (const ConnectionInfo& c : S().connections) {
        if (c.id == S().ctxConnId) {
            redisCtx = c.driver == "redis";
            break;
        }
    }
    const bool tableLevel = !S().ctxTableName.empty();
    const bool dbLevel = !tableLevel && !S().ctxDatabase.empty();
    std::vector<std::string> items;
    if (tableLevel && redisCtx) {
        items = {"查看值", std::string(L(StrId::Refresh))};
    } else if (tableLevel) {
        items = {"查看属性", "查看DDL", std::string(L(StrId::DropTable)),
                 std::string(L(StrId::Refresh))};
    } else if (dbLevel) {
        items = {std::string(L(StrId::NewTable)), std::string(L(StrId::NewQueryTab)),
                 std::string(L(StrId::Refresh))};
    } else {
        items = {std::string(L(StrId::NewQueryTab)), "打开命籍", std::string(L(StrId::Refresh)),
                 std::string(L(StrId::CloseConnection)), "删除命籍"};
    }
    components::contextMenu(ui, "sidebar.ctx")
        .open(true)
        .screen(w, h)
        .position(S().ctxX, S().ctxY)
        .size(120.0f, 28.0f)
        .items(items)
        .theme(t)
        .zIndex(110)
        .onSelect([tableLevel, dbLevel, redisCtx](int idx) {
            S().ctxMenuOpen = false;
            if (tableLevel && redisCtx) {
                switch (idx) {
                    case 0: heibu::openRedisKey(S().ctxConnId, S().ctxDatabase, S().ctxTableName); break;
                    case 1: heibu::refreshTables(S().ctxConnId); break;
                }
            } else if (tableLevel) {
                switch (idx) {
                    case 0: heibu::openTableProperties(S().ctxConnId, S().ctxDatabase, S().ctxTableName); break;
                    case 1: heibu::openObjectDdl(S().ctxConnId, S().ctxDatabase, S().ctxTableName, "table"); break;
                    case 2: heibu::requestDropTable(); break;
                    case 3: heibu::refreshTables(S().ctxConnId); break;
                }
            } else if (dbLevel) {
                switch (idx) {
                    case 0: heibu::requestCreateTable(); break;
                    case 1: heibu::openQueryTab(S().ctxConnId); break;
                    case 2: heibu::refreshTables(S().ctxConnId); break;
                }
            } else {
                switch (idx) {
                    case 0: heibu::openQueryTab(S().ctxConnId); break;
                    case 1: heibu::connectNow(S().ctxConnId); break;
                    case 2: heibu::refreshTables(S().ctxConnId); break;
                    case 3: heibu::closeConnection(S().ctxConnId); break;
                    case 4: heibu::removeConnection(S().ctxConnId); break;
                }
            }
        })
        .onOpenChange([](bool open) { S().ctxMenuOpen = open; })
        .build();
}

// 分页大小上拉菜单：高度按 item 数动态算，底边贴住按钮顶。
inline void composePageSizeMenu(eui::Ui& ui, float w, float h,
                                const components::theme::ThemeColorTokens& t) {
    if (!S().pageSizeMenuOpen) {
        return;
    }
    std::vector<std::string> items = {"50", "100", "200", "500"};
    const float itemH = 28.0f;
    const float inset = t.metrics.spacing.small;
    const float menuH = itemH * static_cast<float>(items.size()) + inset * 2.0f;
    components::contextMenu(ui, "page.size.menu")
        .open(true)
        .screen(w, h)
        .position(S().pageSizeMenuX, S().pageSizeMenuY - menuH)
        .size(120.0f, itemH)
        .items(items)
        .theme(t)
        .zIndex(111)
        .onSelect([](int idx) {
            const std::int64_t sizes[] = {50, 100, 200, 500};
            S().pageSizeMenuOpen = false;
            if (idx >= 0 && idx < 4) {
                heibu::setPageSize(S().pageSizeMenuTabId, sizes[idx]);
            }
        })
        .onOpenChange([](bool open) { S().pageSizeMenuOpen = open; })
        .build();
}

// 标签右键菜单（自定义）：含红色危险项「关闭所有标签」。
inline void composeTabContextMenu(eui::Ui& ui, float w, float h,
                                  const components::theme::ThemeColorTokens& t) {
    if (!S().tabCtxOpen) {
        return;
    }
    const std::string id = "tab.ctx";
    const float itemH = 28.0f;
    const float menuW = 150.0f;
    const float inset = 6.0f;
    const float sepGap = 5.0f;   // 分隔线占位
    const float menuH = inset * 2.0f + itemH * 5.0f + sepGap;
    const float x = std::clamp(S().tabCtxX, 8.0f, std::max(8.0f, w - menuW - 8.0f));
    const float y = std::clamp(S().tabCtxY, 8.0f, std::max(8.0f, h - menuH - 8.0f));
    const core::Color danger = {0.92f, 0.33f, 0.33f, 1.0f};
    const core::Color idle{0.0f, 0.0f, 0.0f, 0.0f};
    const core::Color shadow = t.dark ? core::Color{0.0f, 0.0f, 0.0f, 0.4f}
                                      : core::Color{0.10f, 0.14f, 0.22f, 0.12f};

    ui.stack(id)
        .position(0.0f, 0.0f).size(w, h)
        .zIndex(112)
        .content([&] {
            // 点击外部关闭
            ui.rect(id + ".dismiss").size(w, h).color(idle)
                .onClick([] {
                    S().tabCtxOpen = false;
                    app::requestUpdate();
                })
                .build();

            ui.stack(id + ".menu")
                .position(x, y).size(menuW, menuH)
                .content([&] {
                    ui.rect(id + ".bg").size(menuW, menuH)
                        .color(t.surface).radius(12.0f)
                        .border(1.0f, components::theme::withOpacity(t.border, 0.82f))
                        .shadow(14.0f, 3.0f, shadow)
                        .build();

                    const struct {
                        const char* text;
                        bool danger;
                    } items[] = {
                        {"关闭标签", false},
                        {"关闭其他标签", false},
                        {"关闭左侧标签", false},
                        {"关闭右侧标签", false},
                        {"关闭所有标签", true},
                    };
                    float iy = inset;
                    for (int i = 0; i < 5; ++i) {
                        if (items[i].danger) {
                            ui.rect(id + ".sep")
                                .position(inset, iy).size(menuW - inset * 2.0f, 1.0f)
                                .color(components::theme::withOpacity(t.border, 0.5f))
                                .build();
                            iy += sepGap;
                        }
                        const core::Color textColor = items[i].danger ? danger : t.text;
                        ui.rect(id + ".item." + std::to_string(i))
                            .position(inset, iy).size(menuW - inset * 2.0f, itemH)
                            .states(idle, t.surfaceHover, t.surfaceActive)
                            .radius(8.0f)
                            .onClick([i] {
                                const std::string tabId = S().tabCtxId;
                                S().tabCtxOpen = false;
                                switch (i) {
                                    case 0: heibu::closeTab(tabId); break;
                                    case 1: heibu::closeOtherTabs(tabId); break;
                                    case 2: heibu::closeTabsToLeft(tabId); break;
                                    case 3: heibu::closeTabsToRight(tabId); break;
                                    case 4: heibu::closeAllTabs(); break;
                                }
                            })
                            .build();
                        ui.text(id + ".label." + std::to_string(i))
                            .position(inset + 10.0f, iy).size(menuW - inset * 2.0f - 20.0f, itemH)
                            .text(items[i].text)
                            .fontSize(13.0f)
                            .color(textColor)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                        iy += itemH;
                    }
                })
                .build();
        })
        .build();
}

// 导出格式菜单：高度按 item 数动态算；上拉贴按钮顶、下拉贴按钮底。
inline void composeExportMenu(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    if (!S().exportMenuOpen) {
        return;
    }
    std::vector<std::string> items = {"CSV (.csv)", "TSV (.tsv)", "JSON (.json)", "SQL INSERT (.sql)"};
    const float itemH = 30.0f;
    const float inset = t.metrics.spacing.small;
    const float menuH = itemH * static_cast<float>(items.size()) + inset * 2.0f;
    const float my = S().exportMenuUp ? (S().exportMenuY - menuH) : S().exportMenuY;
    components::contextMenu(ui, "export.menu")
        .open(true)
        .screen(w, h)
        .position(S().exportMenuX, my)
        .size(140.0f, itemH)
        .items(items)
        .theme(t)
        .zIndex(113)
        .onSelect([](int idx) {
            const char* fmts[] = {"csv", "tsv", "json", "sql"};
            S().exportMenuOpen = false;
            if (idx >= 0 && idx < 4) {
                heibu::exportAs(fmts[idx]);
            }
        })
        .onOpenChange([](bool open) { S().exportMenuOpen = open; })
        .build();
}

// 全局 toast：右下角弹出，3 秒自动消失（导出成功/失败等反馈）。
inline void composeToast(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    components::toast(ui, "app.toast")
        .visible(S().toastVisible)
        .screen(w, h)
        .size(360.0f, 64.0f)
        .title(S().toastTitle)
        .message(S().toastMessage)
        .theme(t)
        .zIndex(120)
        .duration(3.0f)
        .onDismiss([] { S().toastVisible = false; })
        .build();
}

inline void composeShell(eui::Ui& ui, const eui::Screen& screen) {
    syncTheme();   // 跟随系统时同步 OS 深色（轮询线程唤醒后触发重排再读）
    const components::theme::ThemeColorTokens t = currentTheme();
    const float w = screen.width;
    const float h = screen.height;

    // 根背景（clearColor 初始化固化，运行时主题切换需 compose 重绘）。
    ui.rect("root.bg").position(0.0f, 0.0f).size(w, h).color(t.background);

    composeSidebar(ui, w, h, t);
    composeContent(ui, w, h, t);
    composeConnDialog(ui, w, h, t);
    composeDeleteConfirm(ui, w, h, t);
    composeCreateTableDialog(ui, w, h, t);
    composeDropTableDialog(ui, w, h, t);
    composeContextMenu(ui, w, h, t);
    composePageSizeMenu(ui, w, h, t);
    composeTabContextMenu(ui, w, h, t);
    composeExportMenu(ui, w, h, t);
    composeToast(ui, w, h, t);
}

} // namespace heibu::ui
