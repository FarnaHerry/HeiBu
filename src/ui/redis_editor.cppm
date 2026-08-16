// 黑簿 Redis 值编辑器 — 查看/修改键值 + TTL。
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

inline void composeRedisEditor(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                               const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    const float headerH = 96.0f;
    const float footerH = 40.0f;
    const float bodyY = y + headerH;
    const float bodyH = h - headerH - footerH;

    // ── 头部：键名 + 类型 + TTL ──
    ui.rect("redis.head." + tabId).position(x, y).size(w, headerH).color(panelColor(t));
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
    ui.rect("redis.foot." + tabId).position(x, by).size(w, footerH).color(panelColor(t));
    components::button(ui, "rs.save." + tabId)
        .position(x + 12.0f, by + 5.0f).size(88.0f, 30.0f)
        .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
        .onClick([tabId] { heibu::saveRedisKey(tabId); })
        .build();
    components::button(ui, "rs.cancel." + tabId)
        .position(x + 108.0f, by + 5.0f).size(88.0f, 30.0f)
        .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
        .onClick([tabId] { heibu::closeTab(tabId); })
        .build();
}

} // namespace heibu::ui
