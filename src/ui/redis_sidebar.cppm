// 黑簿 Redis 二级侧边栏 — 主侧边栏与内容区之间，独立键树。
// 键按分隔符（: / . - _，可选）分组为树，中间段可展开/折叠，叶子点击开值编辑器。
module;
#include "eui_ui.h"

export module heibu.ui.redis_sidebar;
import std;
import heibu.app_actions;
import heibu.app_state;
import heibu.db.types;
import heibu.ui.theme;

export namespace heibu::ui {

// 键树行：folder=中间段，leaf=完整键。
struct RRow {
    bool isFolder;
    std::string path;      // folder: 前缀路径；leaf: 完整键
    std::string label;     // 末段显示
    std::string type;      // leaf 类型
    bool expanded;
    int depth;
};

inline void composeRedisSidebar(eui::Ui& ui, float x, float y, float w, float h,
                                const components::theme::ThemeColorTokens& t) {
    AppState& s = S();
    drawPanel(ui, "redis.sidebar.panel", x, y, w, h, t);

    // ── 头部：db 名 + 分隔符选择 + 关闭 ──
    ui.text("rsdb.title").position(x + 10.0f, y + 10.0f).size(w - 30.0f, 22.0f)
        .text("Redis · " + s.redisSidebarDb).fontSize(13.0f).color(t.text)
        .build();
    ui.text("rsdb.close")
        .position(x + w - 24.0f, y + 8.0f).size(20.0f, 20.0f)
        .text("×").fontSize(14.0f).color(t.border)
        .horizontalAlign(core::HorizontalAlign::Center)
        .verticalAlign(core::VerticalAlign::Center)
        .onClick([] { heibu::closeRedisSidebar(); })
        .build();

    // 分隔符选择
    ui.text("rsdb.sep_label").position(x + 10.0f, y + 34.0f)
        .text("分隔符").fontSize(11.0f).color(t.border)
        .build();
    const char* seps[] = {":", "/", ".", "-", "_"};
    for (int i = 0; i < 5; ++i) {
        const bool active = s.redisSeparator == seps[i];
        components::button(ui, "rsdb.sep." + std::to_string(i))
            .position(x + 10.0f + static_cast<float>(i) * 34.0f, y + 34.0f)
            .size(30.0f, 24.0f)
            .text(seps[i]).fontSize(13.0f).theme(t, active)
            .onClick([sep = std::string(seps[i])] { heibu::setRedisSeparator(sep); })
            .build();
    }

    // ── 键树 ──
    std::vector<TableInfo> objs;
    auto tit = s.tableLists.find(s.redisSidebarConnId);
    if (tit != s.tableLists.end()) {
        auto tdit = tit->second.find(s.redisSidebarDb);
        if (tdit != tit->second.end()) {
            objs = tdit->second;
        }
    }
    std::vector<RRow> rows;
    {
        const std::string sep = s.redisSeparator;
        std::function<void(const std::string&, int)> walk;
        walk = [&](const std::string& prefix, int depth) {
            std::map<std::string, bool> folders;
            std::map<std::string, std::string> leaves;
            for (const TableInfo& o : objs) {
                if (!prefix.empty() && o.name.rfind(prefix + sep, 0) != 0) {
                    continue;
                }
                const std::string rest =
                    prefix.empty() ? o.name : o.name.substr(prefix.size() + sep.size());
                const std::size_t pos = rest.find(sep);
                if (pos == std::string::npos) {
                    leaves[rest] = o.type;
                } else if (pos > 0) {   // 跳过空段
                    folders[rest.substr(0, pos)] = true;
                }
            }
            for (const auto& [seg, isF] : folders) {
                const std::string childPath = prefix.empty() ? seg : prefix + sep + seg;
                const std::string ek = s.redisSidebarConnId + "\n" + s.redisSidebarDb + "\n" +
                                       sep + "\n" + childPath;
                const bool expanded = s.expandedRedisPaths.count(ek) > 0;
                rows.push_back({true, childPath, seg, "", expanded, depth});
                if (expanded) {
                    walk(childPath, depth + 1);
                }
            }
            for (const auto& [seg, type] : leaves) {
                const std::string full = prefix.empty() ? seg : prefix + sep + seg;
                rows.push_back({false, full, seg, type, false, depth});
            }
        };
        walk("", 0);
    }

    const float listY = y + 66.0f;
    const float listH = h - 66.0f - 10.0f;
    const components::theme::ThemeColorTokens theme = t;
    const float rowH = 24.0f;

    components::virtualList(ui, "redis.sidebar.list")
        .position(x + 6.0f, listY).size(w - 12.0f, std::max(40.0f, listH))
        .itemCount(static_cast<std::int64_t>(rows.size()))
        .rowHeight(rowH)
        .offset(s.redisSidebarScrollY)
        .theme(t)
        .onChange([](float v) { S().redisSidebarScrollY = v; })
        .row([rows, theme](eui::Ui& ui, const std::string& slotId, std::int64_t idx, float w2, float) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= rows.size()) {
                return;
            }
            const RRow& r = rows[static_cast<std::size_t>(idx)];
            const float indent = 8.0f + static_cast<float>(r.depth) * 16.0f;
            if (r.isFolder) {
                drawSidebarItem(ui, slotId + ".f", indent, 0.0f, w2 - indent, 24.0f,
                                r.label, false, theme,
                                [path = r.path] { heibu::toggleRedisPath(S().redisSidebarConnId, S().redisSidebarDb, path); },
                                r.expanded ? "▾ " : "▸ ", nullptr);
            } else {
                drawSidebarItem(ui, slotId + ".l", indent, 0.0f, w2 - indent, 24.0f,
                                r.label, false, theme,
                                [key = r.path, type = r.type] {
                                    heibu::selectSidebar(SidebarSelection::Kind::Object,
                                                         S().redisSidebarConnId, S().redisSidebarDb, key, type);
                                    heibu::openRedisKey(S().redisSidebarConnId, S().redisSidebarDb, key);
                                },
                                "", nullptr);
            }
        })
        .build();
}

} // namespace heibu::ui
