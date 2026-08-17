// 黑簿 对话框 — 新命籍（连接）表单。components::dialog 的 content 会替换默认标题/按钮，故表单里自绘按钮。
module;
#include "eui_ui.h"

export module heibu.ui.dialogs;
import std;
import heibu.app_actions;
import heibu.app_state;
import heibu.i18n;
import heibu.ui.theme;

namespace {

struct ConnTypeDef {
    const char* id;
    const char* name;
};

inline constexpr ConnTypeDef kConnTypes[] = {
    {"sqlite", "SQLite"},
    {"mysql", "MySQL"},
    {"postgres", "PostgreSQL"},
    {"redis", "Redis"},
};

inline const char* driverDisplayName(const std::string& d) {
    for (const ConnTypeDef& t : kConnTypes) {
        if (d == t.id) {
            return t.name;
        }
    }
    if (d == "mariadb") {
        return "MySQL";
    }
    if (d == "postgresql" || d == "pg") {
        return "PostgreSQL";
    }
    return "SQLite";
}

} // namespace

export namespace heibu::ui {

inline void composeConnDialog(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    AppState& s = S();

    // ── 第一步：选择连接类型（正方形圆角卡片） ──
    if (s.connStep == 0) {
        const float cardW = 128.0f;
        const float cardH = 128.0f;
        const float gap = 16.0f;
        const std::size_t n = sizeof(kConnTypes) / sizeof(kConnTypes[0]);
        const float dlgW = 24.0f * 2.0f + cardW * static_cast<float>(n) +
                           gap * (static_cast<float>(n) - 1.0f);
        const float dlgH = 250.0f;
        components::dialog(ui, "conn_dialog")
            .open(s.showConnDialog)
            .screen(w, h)
            .size(dlgW, dlgH)
            .theme(t)
            .zIndex(100)
            .content([&] {
                ui.text("conn_dialog.title")
                    .position(24.0f, 22.0f)
                    .text("选择连接类型")
                    .fontSize(17.0f)
                    .color(t.text);
                components::button(ui, "conn_dialog.close")
                    .position(dlgW - 32.0f, 16.0f).size(24.0f, 24.0f)
                    .text("×").fontSize(15.0f).theme(t, false)
                    .onClick([] { heibu::closeConnDialog(); })
                    .build();
                for (std::size_t i = 0; i < n; ++i) {
                    const float cx = 24.0f + static_cast<float>(i) * (cardW + gap);
                    const float cy = 62.0f;
                    const core::Color color = connectionIconColor(kConnTypes[i].id);
                    const std::string cid = "conn_dialog.type." + std::to_string(i);
                    ui.rect(cid + ".card")
                        .position(cx, cy).size(cardW, cardH)
                        .color(t.surface)
                        .radius(14.0f)
                        .border(1.0f, components::theme::withOpacity(t.border, 0.6f))
                        .build();
                    ui.rect(cid + ".icon")
                        .position(cx + (cardW - 40.0f) * 0.5f, cy + 24.0f)
                        .size(40.0f, 40.0f)
                        .color(color)
                        .radius(20.0f)
                        .build();
                    ui.text(cid + ".name")
                        .position(cx, cy + 78.0f).size(cardW, 24.0f)
                        .text(kConnTypes[i].name)
                        .fontSize(13.0f)
                        .color(t.text)
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();
                    ui.rect(cid + ".hit")
                        .position(cx, cy).size(cardW, cardH)
                        .states({0.0f, 0.0f, 0.0f, 0.0f},
                                components::theme::withOpacity(color, 0.10f),
                                components::theme::withOpacity(color, 0.18f))
                        .radius(14.0f)
                        .onClick([driver = std::string(kConnTypes[i].id)] {
                            heibu::chooseConnDriver(driver);
                        })
                        .build();
                }
            })
            .build();
        return;
    }

    // ── 第二步：填写连接信息 ──
    const bool isSqlite = s.connDriver == "sqlite";
    const bool isRedis = s.connDriver == "redis";
    const float dlgW = 520.0f;
    const float dlgH = isSqlite ? 300.0f : (isRedis ? 390.0f : 440.0f);
    components::dialog(ui, "conn_dialog")
        .open(s.showConnDialog)
        .screen(w, h)
        .size(dlgW, dlgH)
        .theme(t)
        .style(heibu::ui::dialogGlassStyle(t))
        .zIndex(100)
        .content([&] {
            ui.text("conn_dialog.title")
                .position(24.0f, 19.0f)
                .text(std::string(L(StrId::NewConnection)) + " · " + driverDisplayName(s.connDriver))
                .fontSize(17.0f)
                .color(t.text);
            components::button(ui, "conn_dialog.close")
                .position(dlgW - 32.0f, 16.0f).size(24.0f, 24.0f)
                .text("×").fontSize(15.0f).theme(t, false)
                .onClick([] { heibu::closeConnDialog(); })
                .build();

            if (isSqlite) {
                ui.text("conn_dialog.name_label").position(20.0f, 112.0f)
                    .text(std::string(L(StrId::Name))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.name")
                    .position(20.0f, 130.0f).size(400.0f, 30.0f)
                    .value(s.connName).placeholder(std::string(L(StrId::Name)))
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connName = v; })
                    .build();

                ui.text("conn_dialog.path_label").position(20.0f, 168.0f)
                    .text(std::string(L(StrId::Path))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.path")
                    .position(20.0f, 186.0f).size(320.0f, 30.0f)
                    .value(s.connPath).placeholder("C:\\…\\账册.db  或  :memory:")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connPath = v; })
                    .build();
                components::button(ui, "conn_dialog.browse")
                    .position(350.0f, 186.0f).size(70.0f, 30.0f)
                    .text(std::string(L(StrId::Browse))).fontSize(12.0f).theme(t, false)
                    .onClick([] {
                        core::platform::FileDialogOptions opts;
                        opts.prompt = "打开 SQLite 账册";
                        opts.filterName = "SQLite 账册";
                        opts.allowedExtensions = {"sqlite", "db", "sqlite3"};
                        const std::string chosen = core::platform::chooseFile(opts);
                        if (!chosen.empty()) {
                            S().connPath = chosen;
                            app::requestUpdate();
                        }
                    })
                    .build();

                if (!s.connError.empty()) {
                    ui.text("conn_dialog.error").position(20.0f, 224.0f)
                        .text(s.connError).fontSize(12.0f)
                        .color({0.92f, 0.33f, 0.33f, 1.0f});
                }
                components::button(ui, "conn_dialog.save")
                    .position(250.0f, 256.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
                    .textColor(onPrimaryText(t))
                    .onClick([] { heibu::saveConnectionFromDialog(); })
                    .build();
                components::button(ui, "conn_dialog.cancel")
                    .position(346.0f, 256.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
                    .onClick([] { heibu::closeConnDialog(); })
                    .build();
            } else if (isRedis) {
                // Redis 表单：名称 / 主机+端口 / 密码 / 数据库索引
                ui.text("conn_dialog.name_label").position(20.0f, 112.0f)
                    .text(std::string(L(StrId::Name))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.name")
                    .position(20.0f, 130.0f).size(480.0f, 30.0f)
                    .value(s.connName).placeholder(std::string(L(StrId::Name)))
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connName = v; })
                    .build();

                ui.text("conn_dialog.host_label").position(20.0f, 168.0f)
                    .text(std::string(L(StrId::Host))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.host")
                    .position(20.0f, 186.0f).size(320.0f, 30.0f)
                    .value(s.connHost).placeholder("127.0.0.1")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connHost = v; })
                    .build();
                ui.text("conn_dialog.port_label").position(360.0f, 168.0f)
                    .text(std::string(L(StrId::Port))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.port")
                    .position(360.0f, 186.0f).size(140.0f, 30.0f)
                    .value(s.connPort).placeholder("6379")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connPort = v; })
                    .build();

                ui.text("conn_dialog.password_label").position(20.0f, 224.0f)
                    .text(std::string(L(StrId::Password))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.password")
                    .position(20.0f, 242.0f).size(480.0f, 30.0f)
                    .value(s.connPassword).fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connPassword = v; })
                    .build();

                ui.text("conn_dialog.database_label").position(20.0f, 280.0f)
                    .text("数据库索引").fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.database")
                    .position(20.0f, 298.0f).size(480.0f, 30.0f)
                    .value(s.connDatabase).placeholder("0")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connDatabase = v; })
                    .build();

                if (!s.connError.empty()) {
                    ui.text("conn_dialog.error").position(20.0f, 336.0f)
                        .text(s.connError).fontSize(12.0f)
                        .color({0.92f, 0.33f, 0.33f, 1.0f});
                }
                components::button(ui, "conn_dialog.save")
                    .position(250.0f, 350.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
                    .textColor(onPrimaryText(t))
                    .onClick([] { heibu::saveConnectionFromDialog(); })
                    .build();
                components::button(ui, "conn_dialog.cancel")
                    .position(346.0f, 350.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
                    .onClick([] { heibu::closeConnDialog(); })
                    .build();
            } else {
                // 服务器库表单：名称 / 主机+端口 / 用户 / 密码 / 数据库
                ui.text("conn_dialog.name_label").position(20.0f, 112.0f)
                    .text(std::string(L(StrId::Name))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.name")
                    .position(20.0f, 130.0f).size(400.0f, 30.0f)
                    .value(s.connName).placeholder(std::string(L(StrId::Name)))
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connName = v; })
                    .build();

                ui.text("conn_dialog.host_label").position(20.0f, 168.0f)
                    .text(std::string(L(StrId::Host))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.host")
                    .position(20.0f, 186.0f).size(260.0f, 30.0f)
                    .value(s.connHost).placeholder("127.0.0.1")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connHost = v; })
                    .build();
                ui.text("conn_dialog.port_label").position(300.0f, 168.0f)
                    .text(std::string(L(StrId::Port))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.port")
                    .position(300.0f, 186.0f).size(120.0f, 30.0f)
                    .value(s.connPort).placeholder("3306")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connPort = v; })
                    .build();

                ui.text("conn_dialog.user_label").position(20.0f, 224.0f)
                    .text(std::string(L(StrId::User))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.user")
                    .position(20.0f, 242.0f).size(400.0f, 30.0f)
                    .value(s.connUser).placeholder("root")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connUser = v; })
                    .build();

                ui.text("conn_dialog.password_label").position(20.0f, 280.0f)
                    .text(std::string(L(StrId::Password))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.password")
                    .position(20.0f, 298.0f).size(400.0f, 30.0f)
                    .value(s.connPassword).fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connPassword = v; })
                    .build();

                ui.text("conn_dialog.database_label").position(20.0f, 336.0f)
                    .text(std::string(L(StrId::Database))).fontSize(12.0f).color(t.border);
                components::input(ui, "conn_dialog.database")
                    .position(20.0f, 354.0f).size(400.0f, 30.0f)
                    .value(s.connDatabase).placeholder("heibu")
                    .fontSize(13.0f).theme(t)
                    .onChange([](const std::string& v) { S().connDatabase = v; })
                    .build();

                if (!s.connError.empty()) {
                    ui.text("conn_dialog.error").position(20.0f, 392.0f)
                        .text(s.connError).fontSize(12.0f)
                        .color({0.92f, 0.33f, 0.33f, 1.0f});
                }
                components::button(ui, "conn_dialog.save")
                    .position(250.0f, 402.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
                    .textColor(onPrimaryText(t))
                    .onClick([] { heibu::saveConnectionFromDialog(); })
                    .build();
                components::button(ui, "conn_dialog.cancel")
                    .position(346.0f, 402.0f).size(88.0f, 32.0f)
                    .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
                    .onClick([] { heibu::closeConnDialog(); })
                    .build();
            }

            // 底部左侧：上一步
            components::button(ui, "conn_dialog.prev")
                .position(20.0f, dlgH - 44.0f).size(88.0f, 32.0f)
                .text("上一步").fontSize(13.0f).theme(t, false)
                .onClick([] { heibu::backToConnTypes(); })
                .build();
        })
        .build();
}

// 新建表对话框：表名 + 字段（名称/类型）列表 → CREATE TABLE。
inline void composeCreateTableDialog(eui::Ui& ui, float w, float h,
                                     const components::theme::ThemeColorTokens& t) {
    AppState& s = S();
    components::dialog(ui, "create_table_dialog")
        .open(s.showCreateTable)
        .screen(w, h)
        .size(460.0f, 460.0f)
        .theme(t)
        .style(heibu::ui::dialogGlassStyle(t))
        .zIndex(102)
        .content([&] {
            ui.text("ct.title")
                .position(20.0f, 18.0f)
                .text(std::string(L(StrId::NewTable)))
                .fontSize(17.0f)
                .color(t.text);

            ui.text("ct.name_label")
                .position(20.0f, 54.0f)
                .text(std::string(L(StrId::Name)))
                .fontSize(12.0f)
                .color(t.border);
            components::input(ui, "ct.name")
                .position(20.0f, 70.0f).size(420.0f, 28.0f)
                .value(s.newTableName)
                .placeholder("表名")
                .fontSize(13.0f)
                .theme(t)
                .onChange([](const std::string& v) { S().newTableName = v; })
                .build();

            // 字段区
            ui.text("ct.fields_label")
                .position(20.0f, 108.0f)
                .text("字段")
                .fontSize(12.0f)
                .color(t.border);
            ui.text("ct.col_name_header")
                .position(20.0f, 128.0f)
                .text(std::string(L(StrId::Name)))
                .fontSize(11.0f)
                .color(t.border);
            ui.text("ct.col_type_header")
                .position(203.0f, 128.0f)
                .text("类型")
                .fontSize(11.0f)
                .color(t.border);

            float cy = 146.0f;
            for (std::size_t i = 0; i < s.newTableColumns.size(); ++i) {
                components::input(ui, "ct.col_name." + std::to_string(i))
                    .position(20.0f, cy).size(175.0f, 26.0f)
                    .value(s.newTableColumns[i].name)
                    .placeholder("字段名")
                    .fontSize(12.0f)
                    .theme(t)
                    .onChange([i](const std::string& v) { heibu::updateColumnFieldName(static_cast<int>(i), v); })
                    .build();
                components::input(ui, "ct.col_type." + std::to_string(i))
                    .position(203.0f, cy).size(180.0f, 26.0f)
                    .value(s.newTableColumns[i].type)
                    .placeholder("TEXT")
                    .fontSize(12.0f)
                    .theme(t)
                    .onChange([i](const std::string& v) { heibu::updateColumnFieldType(static_cast<int>(i), v); })
                    .build();
                components::button(ui, "ct.col_del." + std::to_string(i))
                    .position(391.0f, cy).size(26.0f, 26.0f)
                    .text("×").fontSize(13.0f).theme(t, false)
                    .onClick([i] { heibu::removeColumnField(static_cast<int>(i)); })
                    .build();
                cy += 32.0f;
            }

            // ＋ 添加字段
            components::button(ui, "ct.add_field")
                .position(20.0f, cy + 2.0f).size(100.0f, 26.0f)
                .text("＋ 添加字段").fontSize(12.0f).theme(t, false)
                .onClick([] { heibu::addColumnField(); })
                .build();

            components::button(ui, "ct.save")
                .position(300.0f, 412.0f).size(80.0f, 30.0f)
                .text(std::string(L(StrId::Save))).fontSize(13.0f).theme(t, true)
                .onClick([] { heibu::confirmCreateTable(); })
                .build();
            components::button(ui, "ct.cancel")
                .position(388.0f, 412.0f).size(52.0f, 30.0f)
                .text(std::string(L(StrId::Cancel))).fontSize(13.0f).theme(t, false)
                .onClick([] {
                    S().showCreateTable = false;
                    app::requestUpdate();
                })
                .build();
        })
        .build();
}

// 删除表确认框。
inline void composeDropTableDialog(eui::Ui& ui, float w, float h,
                                   const components::theme::ThemeColorTokens& t) {
    components::dialog(ui, "drop_table_dialog")
        .open(S().showDropTable)
        .screen(w, h)
        .size(400.0f, 180.0f)
        .theme(t)
        .style(heibu::ui::dialogGlassStyle(t))
        .zIndex(102)
        .title(std::string(L(StrId::DropTable)))
        .message(std::string(L(StrId::DropTableMsg)))
        .primaryText(std::string(L(StrId::Delete)))
        .secondaryText(std::string(L(StrId::Cancel)))
        .onPrimary([] { heibu::confirmDropTable(); })
        .onSecondary([] {
            S().showDropTable = false;
            app::requestUpdate();
        })
        .build();
}

} // namespace heibu::ui
