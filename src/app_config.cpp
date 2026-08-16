// 黑簿 eui-neo 应用入口：dslAppConfig() + compose()。
// include 纪律：eui_neo.h 及标准头在前，import std 在后。
#include <eui_neo.h>
// 0.5.6 起 app::initialize/update/render/shutdown/requestUpdate 等实现 inline 在此头，
// own-main 模式须显式 include（eui_neo.h 只拉到声明）。
#include "eui/detail/dsl_app_impl.h"

import std;
import heibu.app_state;
import heibu.os_theme;
import heibu.store.connections;
import heibu.ui.shell;
import heibu.ui.theme;

namespace {

void loadConnectionsOnce() {
    static bool loaded = false;
    if (loaded) {
        return;
    }
    loaded = true;
    std::string err;
    heibu::S().connections = heibu::ConnectionStore::load(err);
}

// 后台轮询 OS 深色模式变化；变化时只唤醒 UI 一帧（compose 里读 osDark()），空闲零开销。
void startThemeWatchOnce() {
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    std::thread([] {
        bool last = heibu::osDark();
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const bool cur = heibu::osDark();
            if (cur != last) {
                last = cur;
                core::platform::requestUiUpdate();
            }
        }
    }).detach();
}

} // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("黑簿 Heibu")
        .pageId("heibu")
        .clearColor({0.075f, 0.085f, 0.105f, 1.0f})
        // 原生全局缩放（eui-neo 0.5.6）：uiScale 按 dpiScale*uiScale 放大整个逻辑坐标系。
        // 所有尺寸按设计逻辑像素书写；窗口物理尺寸 = 设计尺寸 × kUI。
        .uiScale(heibu::ui::kUI)
        .windowSize(static_cast<int>(heibu::ui::kDesignW * heibu::ui::kUI),
                    static_cast<int>(heibu::ui::kDesignH * heibu::ui::kUI))
        .fps(0.0)
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    loadConnectionsOnce();
    startThemeWatchOnce();
    heibu::ui::composeShell(ui, screen);
}

} // namespace app
