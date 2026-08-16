// 黑簿 SQL 编辑器 — components::input 多行 + monospace。无语法高亮（MVP 取舍）。
module;
#include "eui_ui.h"

export module heibu.ui.editor;
import std;
import heibu.app_state;

export namespace heibu::ui {

inline void composeEditor(eui::Ui& ui, const Tab& tab, float x, float y, float w, float h,
                          const components::theme::ThemeColorTokens& t) {
    const std::string tabId = tab.id;
    components::input(ui, "editor." + tabId)
        .position(x, y)
        .size(w, h)
        .value(tab.sqlText)
        .multiline(true)
        .fontFamily("monospace")
        .fontSize(14.0f)
        .theme(t)
        .onChange([tabId](const std::string& v) {
            auto it = S().tabs.find(tabId);
            if (it != S().tabs.end()) {
                it->second.sqlText = v;
            }
        })
        .build();
}

} // namespace heibu::ui
