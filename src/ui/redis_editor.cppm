// 黑簿 Redis 库标签 — 左侧键树 + 右侧值编辑器，一个标签内完成浏览与修改。
// 键按分隔符（: / . - _，可选）分组为树；叶子点击在右侧编辑。
// string 单输入；hash 字段/值行（可加可删）；list/set/zset 只读。TTL 空=保持当前，数字=设秒，-1=持久。
module;
#include "eui_ui.h"

export module heibu.ui.redis_editor;
import std;
import heibu.app_actions;
import heibu.app_state;
import heibu.db.types;
import heibu.i18n;
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

// ── 右侧值编辑器（模块内）──
inline void composeRedisEditor(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                               const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    const float headerH = 96.0f;
    const float footerH = 40.0f;
    const float bodyY = y + headerH;
    const float bodyH = h - headerH - footerH;
    const core::Color bandColor =
        components::theme::withOpacity(t.surface, t.dark ? 0.50f : 0.72f);

    // ── 头部：键名 + 类型 + TTL ──
    ui.rect("redis.head." + tabId).position(x, y).size(w, headerH)
        .color(bandColor).blur(kFrostBlur);
    // 头 / 值 分割线
    ui.rect("redis.hv." + tabId)
        .position(x, y + headerH - 1.0f).size(w, 1.0f)
        .color(components::theme::withOpacity(t.border, t.dark ? 0.22f : 0.40f))
        .build();
    ui.text("redis.key." + tabId)
        .position(x + 12.0f, y + 12.0f).size(w - 200.0f, 24.0f)
        .text(tab.redisKey)
        .fontSize(15.0f).color(t.text)
        .build();
    ui.text("redis.type." + tabId)
        .position(x + 12.0f, y + 40.0f)
        .text("类型: " + tab.redisType).fontSize(12.0f).color(t.border)
        .build();

    ui.text("redis.ttl_label." + tabId)
        .position(x + w - 220.0f, y + 14.0f)
        .text("TTL (秒)").fontSize(12.0f).color(t.border)
        .build();
    const std::string ttlHint =
        tab.redisTtlCurrent < 0 ? "当前 持久 · 空=保持"
                                : ("当前 " + std::to_string(tab.redisTtlCurrent) + "s · 空=保持");
    components::input(ui, "redis.ttl." + tabId)
        .position(x + w - 220.0f, y + 32.0f).size(200.0f, 26.0f)
        .value(tab.redisTtlInput)
        .placeholder(ttlHint)
        .fontSize(12.0f).theme(t)
        .onChange([tabId](const std::string& v) { heibu::updateRedisTtlInput(tabId, v); })
        .build();
    ui.text("redis.ttl_hint." + tabId)
        .position(x + w - 220.0f, y + 62.0f)
        .text("空=保持 · 数字=设秒 · -1=持久").fontSize(10.0f).color(t.border)
        .build();

    // ── 值区 ──
    if (tab.redisType == "string") {
        components::input(ui, "redis.str." + tabId)
            .position(x + 8.0f, bodyY).size(w - 16.0f, bodyH)
            .value(tab.redisEntries.empty() ? "" : tab.redisEntries[0].value)
            .multiline(true).fontFamily("monospace").fontSize(13.0f).theme(t)
            .onChange([tabId](const std::string& v) { heibu::updateRedisStringValue(tabId, v); })
            .build();
    } else if (tab.redisType == "hash") {
        // 表头
        ui.text("redis.hash_nh." + tabId).position(x + 12.0f, bodyY).size(40.0f, 20.0f)
            .text("字段").fontSize(11.0f).color(t.border).build();
        ui.text("redis.hash_vh." + tabId).position(x + w * 0.42f + 12.0f, bodyY).size(60.0f, 20.0f)
            .text("值").fontSize(11.0f).color(t.border).build();

        components::virtualList(ui, "redis.hash_list." + tabId)
            .position(x + 8.0f, bodyY + 22.0f)
            .size(w - 16.0f, bodyH - 26.0f)
            .itemCount(static_cast<std::int64_t>(tab.redisEntries.size()))
            .rowHeight(32.0f)
            .theme(t)
            .row([tabId, x, w, theme = t](eui::Ui& ui, const std::string& slotId,
                                          std::int64_t idx, float, float rh) {
                auto it = S().tabs.find(tabId);
                if (it == S().tabs.end() || idx < 0 ||
                    static_cast<std::size_t>(idx) >= it->second.redisEntries.size()) {
                    return;
                }
                const RedisEntry& e = it->second.redisEntries[static_cast<std::size_t>(idx)];
                const std::size_t i = static_cast<std::size_t>(idx);
                const float nameW = w * 0.40f;
                const float valW = w * 0.60f - 40.0f;
                components::input(ui, slotId + ".n")
                    .position(0.0f, 3.0f).size(nameW, 26.0f)
                    .value(e.name).fontSize(12.0f).theme(theme)
                    .onChange([tabId, i](const std::string& v) { heibu::updateRedisField(tabId, i, true, v); })
                    .build();
                components::input(ui, slotId + ".v")
                    .position(nameW + 8.0f, 3.0f).size(valW, 26.0f)
                    .value(e.value).fontSize(12.0f).theme(theme)
                    .onChange([tabId, i](const std::string& v) { heibu::updateRedisField(tabId, i, false, v); })
                    .build();
                components::button(ui, slotId + ".del")
                    .position(w - 28.0f, 3.0f).size(22.0f, 26.0f)
                    .text("×").fontSize(13.0f).theme(theme, false)
                    .onClick([tabId, i] { heibu::removeRedisField(tabId, i); })
                    .build();
            })
            .build();
        components::button(ui, "redis.hash_add." + tabId)
            .position(x + 12.0f, bodyY + bodyH - 30.0f).size(110.0f, 26.0f)
            .text("＋ 添加字段").fontSize(12.0f).theme(t, false)
            .onClick([tabId] { heibu::addRedisField(tabId); })
            .build();
    } else {
        // list/set/zset 只读展示
        components::virtualList(ui, "redis.list." + tabId)
            .position(x + 8.0f, bodyY)
            .size(w - 16.0f, bodyH)
            .itemCount(static_cast<std::int64_t>(tab.redisEntries.size()))
            .rowHeight(24.0f)
            .theme(t)
            .row([tabId, textColor = t.text](eui::Ui& ui, const std::string& slotId,
                                             std::int64_t idx, float w2, float) {
                auto it = S().tabs.find(tabId);
                if (it == S().tabs.end() || idx < 0 ||
                    static_cast<std::size_t>(idx) >= it->second.redisEntries.size()) {
                    return;
                }
                const RedisEntry& e = it->second.redisEntries[static_cast<std::size_t>(idx)];
                const std::string line = e.name.empty() ? e.value : (e.name + "  " + e.value);
                ui.text(slotId + ".t")
                    .position(4.0f, 0.0f).size(w2 - 8.0f, 24.0f)
                    .text(line).fontSize(12.0f).color(textColor)
                    .verticalAlign(core::VerticalAlign::Center)
                    .build();
            })
            .build();
    }

    // ── 底部：保存 / 取消 ──
    const float by = y + h - footerH;
    // 值 / 底部 分割线
    ui.rect("redis.fv." + tabId)
        .position(x, by - 1.0f).size(w, 1.0f)
        .color(components::theme::withOpacity(t.border, t.dark ? 0.22f : 0.40f))
        .build();
    ui.rect("redis.foot." + tabId).position(x, by).size(w, footerH)
        .color(bandColor).blur(kFrostBlur);
    components::button(ui, "rs.save." + tabId)
        .position(x + 12.0f, by + 5.0f).size(88.0f, 30.0f)
        .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
        .textColor(onPrimaryText(t))
        .onClick([tabId] { heibu::saveRedisKey(tabId); })
        .build();
    components::button(ui, "rs.cancel." + tabId)
        .position(x + 108.0f, by + 5.0f).size(88.0f, 30.0f)
        .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
        .onClick([tabId] { heibu::selectRedisKey(tabId, ""); })   // 取消选择，标签不关
        .build();
}

// ── 左侧键树（模块内）：分隔符选择 + 冒号分组树 ──
inline void composeRedisTree(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                             const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    AppState& s = S();

    ui.text("rt." + tabId + ".title")
        .position(x + 10.0f, y + 10.0f).size(w - 30.0f, 22.0f)
        .text("Redis · " + tab.database).fontSize(13.0f).color(t.text)
        .build();

    // 分隔符选择
    ui.text("rt." + tabId + ".sep_label")
        .position(x + 10.0f, y + 34.0f)
        .text("分隔符").fontSize(11.0f).color(t.border)
        .build();
    const char* seps[] = {":", "/", ".", "-", "_"};
    for (int i = 0; i < 5; ++i) {
        const bool active = tab.redisSeparator == seps[i];
        components::button(ui, "rt." + tabId + ".sep." + std::to_string(i))
            .position(x + 10.0f + static_cast<float>(i) * 34.0f, y + 34.0f)
            .size(30.0f, 24.0f)
            .text(seps[i]).fontSize(13.0f).theme(t, active)
            .textColor(buttonTextColor(t, active))
            .onClick([tabId, sep = std::string(seps[i])] { heibu::setRedisSeparator(tabId, sep); })
            .build();
    }

    // ── 键树 ──
    std::vector<TableInfo> objs;
    auto tit = s.tableLists.find(tab.connectionId);
    if (tit != s.tableLists.end()) {
        auto tdit = tit->second.find(tab.database);
        if (tdit != tit->second.end()) {
            objs = tdit->second;
        }
    }
    std::vector<RRow> rows;
    {
        const std::string sep = tab.redisSeparator;
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
                const std::string ek = tab.connectionId + "\n" + tab.database + "\n" +
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
    const float rowH = 24.0f;
    const std::string selectedKey = tab.redisKey;

    components::virtualList(ui, "rt." + tabId + ".list")
        .position(x + 6.0f, listY).size(w - 12.0f, std::max(40.0f, listH))
        .itemCount(static_cast<std::int64_t>(rows.size()))
        .rowHeight(rowH)
        .offset(tab.redisTreeScrollY)
        .theme(t)
        .onChange([tabId](float v) {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                it->second.redisTreeScrollY = v;
            }
        })
        .row([tabId, rows, selectedKey, theme = t](eui::Ui& ui, const std::string& slotId,
                                                   std::int64_t idx, float w2, float) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= rows.size()) {
                return;
            }
            const RRow& r = rows[static_cast<std::size_t>(idx)];
            const float indent = 8.0f + static_cast<float>(r.depth) * 16.0f;
            if (r.isFolder) {
                drawSidebarItem(ui, slotId + ".f", indent, 0.0f, w2 - indent, 24.0f,
                                r.label, false, theme,
                                [tabId, path = r.path] {
                                    auto it = S().tabs.find(tabId);
                                    if (it == S().tabs.end()) {
                                        return;
                                    }
                                    heibu::toggleRedisPath(it->second.connectionId, it->second.database,
                                                           it->second.redisSeparator, path);
                                },
                                r.expanded ? "▾ " : "▸ ", nullptr);
            } else {
                drawSidebarItem(ui, slotId + ".l", indent, 0.0f, w2 - indent, 24.0f,
                                r.label, r.path == selectedKey, theme,
                                [tabId, key = r.path] { heibu::selectRedisKey(tabId, key); },
                                "", nullptr);
            }
        })
        .build();
}

// Redis 库标签内容：直接画在内容岛卡片上，内部用线分割（树 | 值编辑器，头/值/底）。
inline void composeRedisTab(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                            const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    const float treeW = kRedisSidebarW;
    const float lineW = 1.0f;

    // 左侧键树区：微妙的底色块，读作独立窗格。
    ui.rect("rt." + tabId + ".treebg")
        .position(x, y).size(treeW, h)
        .color(components::theme::withOpacity(t.surface, t.dark ? 0.40f : 0.62f))
        .blur(kFrostBlur)
        .build();
    composeRedisTree(ui, tab, x, y, treeW, h, t);   // 标题/分隔符/键树

    // 树与值编辑器之间竖分割线（满高，紧贴右侧）。
    ui.rect("rt." + tabId + ".vline")
        .position(x + treeW, y).size(lineW, h)
        .color(components::theme::withOpacity(t.border, t.dark ? 0.25f : 0.42f))
        .build();

    const float ex = x + treeW + lineW;
    const float ew = w - treeW - lineW;
    if (ew <= 0.0f) {
        return;
    }
    if (tab.redisKey.empty()) {
        ui.text("rt." + tabId + ".hint")
            .position(ex + 12.0f, y + 16.0f).size(ew - 24.0f, 22.0f)
            .text("点击左侧键查看 / 修改").fontSize(12.0f).color(t.border)
            .build();
        return;
    }
    composeRedisEditor(ui, tab, ex, y, ew, h, t);
}

} // namespace heibu::ui
