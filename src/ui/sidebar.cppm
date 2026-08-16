// 黑簿 大侧边栏 — 连接 → 数据库 → 对象分类（表/视图/存储过程/函数/触发器）→ 对象，四层树，虚拟化滚动。
module;
#include "eui_ui.h"

export module heibu.ui.sidebar;
import std;
import heibu.app_actions;
import heibu.app_state;
import heibu.db.dialect;
import heibu.db.types;
import heibu.i18n;
import heibu.ui.theme;

export namespace heibu::ui {

struct SidebarRow {
    enum class Kind { Conn, Database, Category, Object } kind;
    std::string connId;
    std::string database;   // Database / Category / Object 行专用
    std::string name;       // Object 行：对象名
    std::string label;      // 显示标签
    std::string type;       // Object 行：对象类型；Category 行：分类类型
    bool expanded;
    bool active;
    core::Color iconColor{0.0f, 0.0f, 0.0f, 0.0f};   // 连接行：类型图标颜色
};

// 对象分类：SQL 库与 Redis 各一套（按连接的驱动选取）。
struct Category {
    const char* type;
    const char* label;
};

inline constexpr Category kSqlCategories[] = {
    {"table", "表"},
    {"view", "视图"},
    {"procedure", "存储过程"},
    {"function", "函数"},
    {"trigger", "触发器"},
};

inline constexpr Category kRedisCategories[] = {
    {"string", "字符串"},
    {"hash", "哈希"},
    {"list", "列表"},
    {"set", "集合"},
    {"zset", "有序集合"},
};

// 某方言是否支持该对象分类（SQLite 无存储过程/函数，不渲染这两类）。
inline bool typeSupported(Dialect dialect, const std::string& type) {
    if (dialect != Dialect::Sqlite) {
        return true;
    }
    return type == "table" || type == "view" || type == "trigger";
}

inline void composeSidebar(eui::Ui& ui, float w, float h, const components::theme::ThemeColorTokens& t) {
    const float x = kIslandGap;
    const float y = kIslandGap;
    const float sw = kSidebarW;
    const float sh = h - kIslandGap * 2.0f;
    const float pad = 10.0f;
    const float innerW = sw - pad * 2.0f;
    const float rowH = 26.0f;

    drawPanel(ui, "sidebar.panel", x, y, sw, sh, t);

    ui.text("sidebar.brand")
        .position(x + pad, y + 11.0f)
        .text("黑簿 Heibu")
        .fontSize(15.0f)
        .color(t.text)
        .build();

    ui.text("sidebar.conn_title")
        .position(x + pad, y + 44.0f).size(innerW, 18.0f)
        .text(std::string(L(StrId::Connections)))
        .fontSize(11.0f)
        .color(t.border)
        .build();

    // 扁平化四层树：连接 → 数据库 → 分类（固定）→ 对象
    std::vector<SidebarRow> rows;
    {
        AppState& s = S();
        for (const ConnectionInfo& conn : s.connections) {
            const bool expanded = s.expandedConnections.count(conn.id) > 0;
            const Dialect dialect = dialectFromDriver(conn.driver);
            const bool connActive =
                s.sidebarSel.kind == SidebarSelection::Kind::Conn &&
                s.sidebarSel.connId == conn.id;
            std::string label = conn.name.empty() ? conn.path : conn.name;
            if (conn.driver == "mysql" || conn.driver == "mariadb") {
                label += " · MySQL";
            } else if (conn.driver == "postgres" || conn.driver == "postgresql" || conn.driver == "pg") {
                label += " · PostgreSQL";
            } else if (conn.driver == "redis") {
                label += " · Redis";
            } else if (!conn.driver.empty() && conn.driver != "sqlite") {
                label += " · " + conn.driver;
            }
            rows.push_back(SidebarRow{SidebarRow::Kind::Conn, conn.id, "", "", label, "",
                                      expanded, connActive, connectionIconColor(conn.driver)});
            if (expanded) {
                auto dit = s.databaseLists.find(conn.id);
                if (dit != s.databaseLists.end()) {
                    for (const std::string& db : dit->second) {
                        const std::string dbKey = conn.id + "\n" + db;
                        const bool dbExpanded = s.expandedDatabases.count(dbKey) > 0;
                        const bool dbActive =
                            s.sidebarSel.kind == SidebarSelection::Kind::Database &&
                            s.sidebarSel.connId == conn.id && s.sidebarSel.database == db;
                        rows.push_back(SidebarRow{SidebarRow::Kind::Database, conn.id, db, db, db, "",
                                                  dbExpanded, dbActive});
                        if (dbExpanded) {
                            std::vector<TableInfo> objs;
                            auto tit = s.tableLists.find(conn.id);
                            if (tit != s.tableLists.end()) {
                                auto tdit = tit->second.find(db);
                                if (tdit != tit->second.end()) {
                                    objs = tdit->second;
                                }
                            }
                            // Redis 键树在二级侧边栏，主侧边栏只列 db，不在此展开。
                            if (conn.driver != "redis") {
                                const std::size_t ncats =
                                    sizeof(kSqlCategories) / sizeof(kSqlCategories[0]);
                                for (std::size_t ci = 0; ci < ncats; ++ci) {
                                    const Category& cat = kSqlCategories[ci];
                                    if (!typeSupported(dialect, cat.type)) {
                                        continue;
                                    }
                                    const std::string catKey = conn.id + "\n" + db + "\n" + cat.type;
                                    const bool catExpanded = s.expandedCategories.count(catKey) > 0;
                                    const bool catActive =
                                        s.sidebarSel.kind == SidebarSelection::Kind::Category &&
                                        s.sidebarSel.connId == conn.id &&
                                        s.sidebarSel.database == db && s.sidebarSel.type == cat.type;
                                    rows.push_back(SidebarRow{SidebarRow::Kind::Category, conn.id, db,
                                                              "", cat.label, cat.type, catExpanded,
                                                              catActive});
                                    if (catExpanded) {
                                        for (const auto& o : objs) {
                                            if (o.type != cat.type) {
                                                continue;
                                            }
                                            const bool objActive =
                                                s.sidebarSel.kind == SidebarSelection::Kind::Object &&
                                                s.sidebarSel.connId == conn.id &&
                                                s.sidebarSel.database == db &&
                                                s.sidebarSel.name == o.name &&
                                                s.sidebarSel.type == o.type;
                                            rows.push_back(SidebarRow{SidebarRow::Kind::Object,
                                                                      conn.id, db, o.name, o.name,
                                                                      o.type, false, objActive});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    const float listX = x + pad;
    const float listY = y + 66.0f;
    const float footerTop = y + sh - 72.0f;
    const float listH = footerTop - listY - 6.0f;
    const components::theme::ThemeColorTokens theme = t;

    components::virtualList(ui, "sidebar.list")
        .position(listX, listY).size(innerW, std::max(40.0f, listH))
        .itemCount(static_cast<std::int64_t>(rows.size()))
        .rowHeight(rowH)
        .offset(S().sidebarScrollY)
        .theme(t)
        .onChange([](float value) { S().sidebarScrollY = value; })
        .row([rows, theme](eui::Ui& ui, const std::string&, std::int64_t idx, float w2, float h2) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= rows.size()) {
                return;
            }
            const SidebarRow& r = rows[static_cast<std::size_t>(idx)];
            if (r.kind == SidebarRow::Kind::Conn) {
                drawSidebarItem(ui, "conn." + r.connId, 0.0f, 0.0f, w2, h2, r.label, r.active, theme,
                                [connId = r.connId] {
                                    heibu::selectSidebar(SidebarSelection::Kind::Conn, connId);
                                    heibu::toggleConnection(connId);
                                },
                                r.expanded ? "▾ " : "▸ ",
                                [connId = r.connId](const core::PointerEvent& e, const core::Rect&) {
                                    S().ctxConnId = connId;
                                    S().ctxDatabase.clear();
                                    S().ctxTableName.clear();
                                    S().ctxX = static_cast<float>(e.x);
                                    S().ctxY = static_cast<float>(e.y);
                                    S().ctxMenuOpen = true;
                                    app::requestUpdate();
                                },
                                r.iconColor);
            } else if (r.kind == SidebarRow::Kind::Database) {
                drawSidebarItem(ui, "db." + r.connId + "." + r.database, 16.0f, 0.0f, w2 - 16.0f, h2,
                                r.label, false, theme,
                                [connId = r.connId, db = r.database] {
                                    heibu::selectSidebar(SidebarSelection::Kind::Database, connId, db);
                                    bool isRedis = false;
                                    for (const ConnectionInfo& c : S().connections) {
                                        if (c.id == connId) {
                                            isRedis = c.driver == "redis";
                                            break;
                                        }
                                    }
                                    if (isRedis) {
                                        heibu::openRedisDb(connId, db);   // 打开二级侧边栏键树
                                    } else {
                                        heibu::toggleDatabase(connId, db);
                                    }
                                },
                                r.expanded ? "▾ " : "▸ ",
                                [connId = r.connId, db = r.database](const core::PointerEvent& e,
                                                                     const core::Rect&) {
                                    S().ctxConnId = connId;
                                    S().ctxDatabase = db;
                                    S().ctxTableName.clear();
                                    S().ctxX = static_cast<float>(e.x);
                                    S().ctxY = static_cast<float>(e.y);
                                    S().ctxMenuOpen = true;
                                    app::requestUpdate();
                                });
            } else if (r.kind == SidebarRow::Kind::Category) {
                drawSidebarItem(ui, "cat." + r.connId + "." + r.database + "." + r.type,
                                32.0f, 0.0f, w2 - 32.0f, h2, r.label, false, theme,
                                [connId = r.connId, db = r.database, type = r.type] {
                                    heibu::selectSidebar(SidebarSelection::Kind::Category, connId, db, "", type);
                                    heibu::toggleCategory(connId, db, type);
                                },
                                r.expanded ? "▾ " : "▸ ", nullptr);
            } else {
                drawSidebarItem(ui, "obj." + r.connId + "." + r.database + "." + r.name,
                                48.0f, 0.0f, w2 - 48.0f, h2, r.label, false, theme,
                                [connId = r.connId, db = r.database, name = r.name, type = r.type] {
                                    heibu::selectSidebar(SidebarSelection::Kind::Object, connId, db, name, type);
                                    if (type == "table" || type == "view") {
                                        heibu::openTableTab(connId, db, name);
                                    } else if (type == "string" || type == "hash" || type == "list" ||
                                               type == "set" || type == "zset") {
                                        heibu::openRedisKey(connId, db, name);
                                    } else {
                                        // 触发器/存储过程/函数（SQL）走定义视图
                                        heibu::openObjectDdl(connId, db, name, type);
                                    }
                                },
                                "",
                                [connId = r.connId, db = r.database,
                                 name = r.name](const core::PointerEvent& e, const core::Rect&) {
                                    S().ctxConnId = connId;
                                    S().ctxDatabase = db;
                                    S().ctxTableName = name;
                                    S().ctxX = static_cast<float>(e.x);
                                    S().ctxY = static_cast<float>(e.y);
                                    S().ctxMenuOpen = true;
                                    app::requestUpdate();
                                });
            }
        })
        .build();

    components::button(ui, "settings_btn")
        .position(x + pad, footerTop).size(innerW, 26.0f)
        .text(std::string(L(StrId::Settings)))
        .fontSize(12.0f).theme(t, false).radius(8.0f)
        .onClick([] { heibu::openSettingsTab(); })
        .build();
    components::button(ui, "new_conn_btn")
        .position(x + pad, y + sh - 40.0f).size(innerW, 30.0f)
        .text("＋ " + std::string(L(StrId::NewConnection)))
        .fontSize(12.0f).theme(t, true).radius(8.0f)
        .onClick([] { heibu::openConnDialog(); })
        .build();
}

} // namespace heibu::ui
