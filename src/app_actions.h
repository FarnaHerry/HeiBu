#pragma once
// 黑簿 业务动作 — 打开/关闭连接、开/关标签、异步加载表格与执行查询。
// 依赖 eui 的 core::async（查询在线程池、then 编回 UI 线程），故属于 UI 层而非 db 层。
#include <eui_neo.h>

#include "app_state.h"
#include "db/driver_factory.h"
#include "db/grid_utils.h"
#include "db/sql_builder.h"
#include "store/connections.h"

#include <algorithm>

namespace heibu {

inline constexpr std::int64_t kRowCap = 50000;

// 表所属的 SQL schema：SQLite 不限定（"main" 只是导航名），MySQL/PG 用数据库/模式名限定。
inline std::string tableSchema(Dialect dialect, const std::string& database) {
    return dialect == Dialect::Sqlite ? std::string() : database;
}

// 前向声明（编辑/删除流程在 cancelChanges 定义之前可能用到）。
inline void cancelChanges(const std::string& tabId);

// 设置侧边栏选中行并重排：点击谁，高亮谁。
inline void selectSidebar(SidebarSelection::Kind kind, std::string connId,
                          std::string database = "", std::string name = "",
                          std::string type = "") {
    S().sidebarSel.kind = kind;
    S().sidebarSel.connId = std::move(connId);
    S().sidebarSel.database = std::move(database);
    S().sidebarSel.name = std::move(name);
    S().sidebarSel.type = std::move(type);
    app::requestUpdate();
}

// 打开（或复用已打开的）连接，返回 session；失败置 statusMessage 并返回 nullptr。
[[nodiscard]] inline std::shared_ptr<IDbDriver> ensureSession(const std::string& connId) {
    auto it = S().sessions.find(connId);
    if (it != S().sessions.end() && it->second->isOpen()) {
        return it->second;
    }
    const ConnectionInfo* info = nullptr;
    for (const ConnectionInfo& c : S().connections) {
        if (c.id == connId) {
            info = &c;
            break;
        }
    }
    if (info == nullptr) {
        return nullptr;
    }
    auto driver = createDriver(info->driver);
    if (!driver) {
        S().statusMessage = std::string(L(StrId::Error)) + ": 不支持的数据库驱动 " + info->driver;
        return nullptr;
    }
    DbConfig cfg;
    cfg.driver = info->driver;
    cfg.path = info->path;
    cfg.host = info->host;
    cfg.port = info->port;
    cfg.user = info->user;
    cfg.password = info->password;
    cfg.database = info->database;
    std::string err;
    if (!driver->open(cfg, err)) {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
        return nullptr;
    }
    S().sessions[connId] = driver;
    return driver;
}

// 显式打开连接（未连则连上）；成功置提示。
inline void connectNow(const std::string& connId) {
    if (auto driver = ensureSession(connId)) {
        S().statusMessage = std::string(L(StrId::Connected));
    }
    S().ctxMenuOpen = false;
    app::requestUpdate();
}

// 切换连接的展开状态；首次展开时同步拉取表列表（sqlite_master 很小，很快）。
inline void toggleConnection(const std::string& connId) {
    AppState& s = S();
    s.activeConnectionId = connId;
    if (s.expandedConnections.count(connId)) {
        s.expandedConnections.erase(connId);
    } else {
        s.expandedConnections.insert(connId);
        if (auto driver = ensureSession(connId)) {
            std::vector<std::string> dbs;
            std::string err;
            if (driver->listDatabases(dbs, err)) {
                s.databaseLists[connId] = std::move(dbs);
            } else {
                s.statusMessage = err;
            }
        }
    }
    app::requestUpdate();
}

// 切换数据库的展开状态；首次展开时拉取该库的表列表。
inline void toggleDatabase(const std::string& connId, const std::string& database) {
    AppState& s = S();
    const std::string key = connId + "\n" + database;
    if (s.expandedDatabases.count(key)) {
        s.expandedDatabases.erase(key);
    } else {
        s.expandedDatabases.insert(key);
        if (auto driver = ensureSession(connId)) {
            std::vector<TableInfo> tables;
            std::string err;
            if (driver->listTables(database, tables, err)) {
                std::vector<TableInfo> extra;
                std::string err2;
                if (driver->listRoutines(database, extra, err2)) {
                    for (auto& o : extra) {
                        tables.push_back(std::move(o));
                    }
                }
                extra.clear();
                if (driver->listTriggers(database, extra, err2)) {
                    for (auto& o : extra) {
                        tables.push_back(std::move(o));
                    }
                }
                s.tableLists[connId][database] = std::move(tables);
            } else {
                s.statusMessage = err;
            }
        }
    }
    app::requestUpdate();
}

// 切换对象分类（表/视图/存储过程/函数/触发器）的展开状态。
inline void toggleCategory(const std::string& connId, const std::string& database,
                           const std::string& type) {
    const std::string key = connId + "\n" + database + "\n" + type;
    if (S().expandedCategories.count(key)) {
        S().expandedCategories.erase(key);
    } else {
        S().expandedCategories.insert(key);
    }
    app::requestUpdate();
}

// 异步执行一条 SQL，结果写回指定标签。queryKey 用于 restart 去重/取消。
inline void runQueryAsync(const std::string& tabId, const std::string& sql, const std::string& queryKey,
                          const std::shared_ptr<IDbDriver>& driver) {
    const std::string key = queryKey + "." + tabId;
    core::async::restart(
        key,
        [driver, sql](const core::async::CancelToken& token) -> core::async::Result<ResultGrid> {
            ResultGrid grid;
            std::string err;
            if (!driver->run(BoundStatement(sql), grid, kRowCap, err,
                             [&token] { return token.canceled(); })) {
                return core::async::failure<ResultGrid>(err);
            }
            return core::async::success(std::move(grid));
        },
        [tabId](const core::async::Result<ResultGrid>& result) {
            auto it = S().tabs.find(tabId);
            if (it == S().tabs.end()) {
                return;   // 标签已关闭，丢弃
            }
            Tab& tab = it->second;
            tab.running = false;
            if (result.ok) {
                tab.result = result.value;
                postProcessGrid(*tab.result, {});
                tab.lastError.clear();
            } else {
                tab.lastError = result.error;
            }
            app::requestUpdate();
        });
}

// 加载表标签：查单表主键（无主键则 SQLite 回退 rowid），一次 SELECT 物化并按键列剥离/提取键值。
inline void loadTableTab(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    const std::string tableName = it->second.tableName;
    const std::string connId = it->second.connectionId;
    const std::string database = it->second.database;
    const std::int64_t page = it->second.page;
    const std::int64_t pageSize = it->second.pageSize;
    auto driver = ensureSession(connId);
    if (!driver) {
        return;
    }

    const std::string key = "load." + tabId;
    const Dialect dialect = driver->dialect();
    const std::string schema = tableSchema(dialect, database);
    auto total = std::make_shared<std::int64_t>(-1);

    core::async::restart(
        key,
        [driver, dialect, tableName, schema, page, pageSize,
         total](const core::async::CancelToken& token) -> core::async::Result<ResultGrid> {
            TableInfo ti;
            ti.name = tableName;
            ti.schema = schema;
            TableKey tkey;
            std::string err;
            if (!driver->tableKey(ti, tkey, err)) {
                return core::async::failure<ResultGrid>(err);
            }
            // 总行数（供分页指示与末页导航）
            ResultGrid cnt;
            if (driver->run(BoundStatement(sql::buildCount(dialect, schema, tableName)), cnt, 1, err,
                            [&token] { return token.canceled(); })) {
                if (!cnt.rows.empty() && !cnt.rows[0].empty() && !cnt.rows[0][0].isNull) {
                    try {
                        *total = std::stoll(cnt.rows[0][0].text);
                    } catch (...) {
                    }
                }
            }
            const std::string sql = sql::buildSelectPaged(dialect, schema, tableName, pageSize,
                                                          page * pageSize, tkey.hidden);
            ResultGrid grid;
            if (!driver->run(BoundStatement(sql), grid, pageSize, err,
                             [&token] { return token.canceled(); })) {
                return core::async::failure<ResultGrid>(err);
            }
            postProcessGrid(grid, tkey.columns);
            return core::async::success(std::move(grid));
        },
        [tabId, total](const core::async::Result<ResultGrid>& result) {
            auto it = S().tabs.find(tabId);
            if (it == S().tabs.end()) {
                return;
            }
            Tab& tab = it->second;
            tab.running = false;
            if (result.ok) {
                tab.result = result.value;
                tab.totalRows = *total;
                tab.lastError.clear();
            } else {
                tab.lastError = result.error;
            }
            app::requestUpdate();
        });
}

// 跳转到指定页（0-based，越界自动夹紧）。
inline void goToPage(const std::string& tabId, std::int64_t page) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    Tab& tab = it->second;
    if (page < 0) {
        page = 0;
    }
    if (tab.totalRows > 0) {
        const std::int64_t totalPages = (tab.totalRows + tab.pageSize - 1) / tab.pageSize;
        if (page >= totalPages) {
            page = totalPages - 1;
        }
    }
    if (page == tab.page) {
        return;
    }
    tab.page = page;
    tab.pageInput = std::to_string(page + 1);
    loadTableTab(tabId);
    app::requestUpdate();
}

// 改每页行数并回到第一页。
inline void setPageSize(const std::string& tabId, std::int64_t size) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || size <= 0) {
        return;
    }
    it->second.pageSize = size;
    it->second.page = 0;
    it->second.pageInput = "1";
    loadTableTab(tabId);
    app::requestUpdate();
}

// 打开表浏览标签并异步加载网格。
inline void openTableTab(const std::string& connId, const std::string& database,
                         const std::string& tableName) {
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    S().activeConnectionId = connId;

    // 识别：若该连接该库该表的标签已存在，直接激活（唤醒）而非重复打开。
    for (const std::string& tabId : S().openTabIds) {
        auto it = S().tabs.find(tabId);
        if (it != S().tabs.end() && it->second.kind == TabKind::Table &&
            it->second.connectionId == connId && it->second.database == database &&
            it->second.tableName == tableName) {
            S().activeTabId = tabId;
            app::requestUpdate();
            return;
        }
    }

    Tab tab;
    tab.id = newId();
    tab.kind = TabKind::Table;
    tab.connectionId = connId;
    tab.database = database;
    tab.tableName = tableName;
    tab.title = std::string(L(StrId::Tables)) + ": " + tableName;
    tab.running = true;
    const std::string tabId = tab.id;
    S().tabs[tabId] = std::move(tab);
    S().openTabIds.push_back(tabId);
    S().activeTabId = tabId;
    loadTableTab(tabId);
    app::requestUpdate();
}

// 打开对象（触发器/存储过程/函数/视图）的 DDL 为查询标签（只读展示）。
inline void openObjectDdl(const std::string& connId, const std::string& database,
                          const std::string& name, const std::string& type) {
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    std::string ddl;
    std::string err;
    if (!driver->objectDdl(database, name, type, ddl, err)) {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
        app::requestUpdate();
        return;
    }
    Tab tab;
    tab.id = newId();
    tab.kind = TabKind::Query;
    tab.connectionId = connId;
    tab.title = name;
    tab.sqlText = ddl;
    S().tabs[tab.id] = std::move(tab);
    S().openTabIds.push_back(tab.id);
    S().activeTabId = tab.id;
    app::requestUpdate();
}

// 查看 Redis 键的值：按类型物化成网格并展示（只读）。
inline void openRedisKey(const std::string& connId, const std::string& database,
                         const std::string& key) {
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    ResultGrid grid;
    std::string err;
    if (!driver->keyValue(database, key, "", grid, err)) {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
        app::requestUpdate();
        return;
    }
    postProcessGrid(grid, {});
    Tab tab;
    tab.id = newId();
    tab.kind = TabKind::Query;
    tab.connectionId = connId;
    tab.title = key;
    tab.sqlText = "KEY " + key;
    tab.result = std::move(grid);
    tab.running = false;
    S().tabs[tab.id] = std::move(tab);
    S().openTabIds.push_back(tab.id);
    S().activeTabId = tab.id;
    app::requestUpdate();
}

// 查看表属性：查列元数据（名称/类型/可空/键/默认）并以只读结果展示。
inline void openTableProperties(const std::string& connId, const std::string& database,
                                const std::string& tableName) {
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    const Dialect d = driver->dialect();
    const std::string schema = tableSchema(d, database);
    std::string sql;
    if (d == Dialect::Sqlite) {
        sql = "PRAGMA table_info(" + sql::quoteIdent(d, tableName) + ")";
    } else if (d == Dialect::MySql) {
        sql = "SELECT COLUMN_NAME AS `name`, COLUMN_TYPE AS `type`, IS_NULLABLE AS `nullable`, "
              "COLUMN_KEY AS `key`, COLUMN_DEFAULT AS `default` "
              "FROM information_schema.COLUMNS "
              "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + tableName +
              "' ORDER BY ORDINAL_POSITION";
    } else {
        sql = "SELECT column_name AS name, data_type AS type, is_nullable AS nullable, "
              "column_default AS \"default\" FROM information_schema.columns "
              "WHERE table_schema = '" + schema + "' AND table_name = '" + tableName +
              "' ORDER BY ordinal_position";
    }
    Tab tab;
    tab.id = newId();
    tab.kind = TabKind::Query;
    tab.connectionId = connId;
    tab.title = tableName + " · 属性";
    tab.sqlText = sql;
    tab.running = true;
    const std::string tabId = tab.id;
    S().tabs[tabId] = std::move(tab);
    S().openTabIds.push_back(tabId);
    S().activeTabId = tabId;
    runQueryAsync(tabId, sql, "query", driver);
    app::requestUpdate();
}

// 打开查询标签。
inline void openQueryTab(const std::string& connId) {
    Tab tab;
    tab.id = newId();
    tab.kind = TabKind::Query;
    tab.connectionId = connId;
    tab.title = std::string(L(StrId::NewQuery)) + " " + std::to_string(S().openTabIds.size() + 1);
    tab.sqlText = "SELECT * FROM ";
    S().tabs[tab.id] = std::move(tab);
    S().openTabIds.push_back(tab.id);
    S().activeTabId = tab.id;
    S().activeConnectionId = connId;
    app::requestUpdate();
}

// 打开（或激活）设置标签——设置即普通标签，无需单独返回逻辑。
inline void openSettingsTab() {
    for (const std::string& tabId : S().openTabIds) {
        if (tabId == "settings") {
            S().activeTabId = tabId;
            app::requestUpdate();
            return;
        }
    }
    Tab tab;
    tab.id = "settings";
    tab.kind = TabKind::Settings;
    tab.title = std::string(L(StrId::Settings));
    S().tabs["settings"] = std::move(tab);
    S().openTabIds.push_back("settings");
    S().activeTabId = "settings";
    app::requestUpdate();
}

// 执行查询标签中的 SQL。
inline void runQueryTab(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    Tab& tab = it->second;
    auto driver = ensureSession(tab.connectionId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    const std::string sql = tab.sqlText;
    if (sql.empty()) {
        S().statusMessage = std::string(L(StrId::EmptyQuery));
        app::requestUpdate();
        return;
    }
    tab.running = true;
    tab.lastError.clear();
    tab.result.reset();
    runQueryAsync(tabId, sql, "query", driver);
    app::requestUpdate();
}

// ── 数据编辑（落笔即生效） ────────────────────────────────────────────────

// 校验一个 CellRef 是否落在可编辑网格内。
inline bool cellRefValid(const Tab& tab, const CellRef& ref) {
    return tab.result && !tab.result->keys.empty() &&
           ref.row >= 0 && static_cast<std::size_t>(ref.row) < tab.result->rows.size() &&
           ref.col >= 0 && static_cast<std::size_t>(ref.col) < tab.result->columns.size();
}

// 单元格点击：未选中则选中；再次点击同格进入编辑。
inline void clickCell(const std::string& tabId, std::int64_t row, int col) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    Tab& tab = it->second;
    const CellRef ref{row, col};
    if (tab.selected && tab.selected->row == row && tab.selected->col == col) {
        if (cellRefValid(tab, ref)) {
            const CellValue& cell = tab.result->rows[static_cast<std::size_t>(row)]
                                        [static_cast<std::size_t>(col)];
            tab.editing = ref;
            tab.editBuffer = cell.isNull ? "" : cell.text;
            tab.editIsNull = false;
        }
    } else {
        tab.selected = ref;
        tab.editing.reset();
    }
    app::requestUpdate();
}

// 确保修改事务已开启：首次改动时 BEGIN，后续改动复用。成功返回驱动（事务已开），失败返回 nullptr。
inline std::shared_ptr<IDbDriver> ensureModificationTransaction(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return nullptr;
    }
    Tab& tab = it->second;
    auto driver = ensureSession(tab.connectionId);
    if (!driver) {
        return nullptr;
    }
    if (!tab.dirty) {
        ResultGrid g;
        std::string err;
        if (!driver->run(BoundStatement("BEGIN"), g, kRowCap, err)) {
            S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
            return nullptr;
        }
        tab.dirty = true;
    }
    return driver;
}

// 提交编辑：UPDATE ... WHERE rowid；成功本地修补 + toast，失败回退。
inline void commitEdit(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    Tab& tab = it->second;
    if (!tab.editing || !cellRefValid(tab, *tab.editing)) {
        tab.editing.reset();
        return;
    }
    const CellRef ref = *tab.editing;
    const std::size_t r = static_cast<std::size_t>(ref.row);
    const std::size_t c = static_cast<std::size_t>(ref.col);

    // 草稿行（最后一行）：就地更新内存，不写库。
    if (tab.addingRow && r + 1 == tab.result->rows.size()) {
        CellValue& cell = tab.result->rows[r][c];
        cell.isNull = tab.editIsNull;
        cell.text = tab.editIsNull ? "" : tab.editBuffer;
        tab.editing.reset();
        tab.editBuffer.clear();
        tab.editIsNull = false;
        app::requestUpdate();
        return;
    }

    auto driver = ensureModificationTransaction(tabId);
    if (!driver) {
        tab.editing.reset();
        return;
    }
    const std::optional<std::string> value = tab.editIsNull
        ? std::nullopt
        : std::optional<std::string>(tab.editBuffer);
    auto stmt = sql::buildUpdateByKey(driver->dialect(), tableSchema(driver->dialect(), tab.database), tab.tableName,
                                      tab.result->keyColumns, tab.result->columns[c].name, value,
                                      tab.result->keys[r].values);
    ResultGrid g;
    std::string err;
    if (driver->run(stmt, g, kRowCap, err)) {
        CellValue& cell = tab.result->rows[r][c];
        if (tab.editIsNull) {
            cell.isNull = true;
            cell.text.clear();
        } else {
            cell.isNull = false;
            cell.text = tab.editBuffer;
        }
        S().statusMessage = std::string(L(StrId::Committed));
    } else {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    tab.editing.reset();
    tab.editBuffer.clear();
    tab.editIsNull = false;
    app::requestUpdate();
}

// 把选中单元格置为 NULL。
inline void setCellNull(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || !it->second.selected) {
        return;
    }
    Tab& tab = it->second;
    const CellRef ref = *tab.selected;
    if (!cellRefValid(tab, ref)) {
        return;
    }
    const std::size_t r = static_cast<std::size_t>(ref.row);
    const std::size_t c = static_cast<std::size_t>(ref.col);
    if (tab.addingRow && r + 1 == tab.result->rows.size()) {
        tab.result->rows[r][c].isNull = true;
        tab.result->rows[r][c].text.clear();
        app::requestUpdate();
        return;
    }
    auto driver = ensureModificationTransaction(tabId);
    if (!driver) {
        return;
    }
    auto stmt = sql::buildUpdateByKey(driver->dialect(), tableSchema(driver->dialect(), tab.database), tab.tableName,
                                      tab.result->keyColumns, tab.result->columns[c].name,
                                      std::nullopt, tab.result->keys[r].values);
    ResultGrid g;
    std::string err;
    if (driver->run(stmt, g, kRowCap, err)) {
        tab.result->rows[r][c].isNull = true;
        tab.result->rows[r][c].text.clear();
        S().statusMessage = std::string(L(StrId::Committed));
    } else {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    app::requestUpdate();
}

// 请求删除选中行（弹确认框）。
inline void requestDeleteRow(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || !it->second.selected) {
        return;
    }
    S().showDeleteConfirm = true;
    app::requestUpdate();
}

// 确认删除选中行：DELETE ... WHERE rowid。
inline void confirmDeleteRow(const std::string& tabId) {
    S().showDeleteConfirm = false;
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || !it->second.selected) {
        app::requestUpdate();
        return;
    }
    Tab& tab = it->second;
    const CellRef ref = *tab.selected;
    if (!cellRefValid(tab, ref)) {
        app::requestUpdate();
        return;
    }
    const std::size_t r = static_cast<std::size_t>(ref.row);
    if (tab.addingRow && r + 1 == tab.result->rows.size()) {
        // 删除草稿行：直接移除，不写库（其余未提交改动仍在事务里）。
        tab.result->rows.pop_back();
        tab.addingRow = false;
        tab.selected.reset();
        tab.editing.reset();
        app::requestUpdate();
        return;
    }
    auto driver = ensureModificationTransaction(tabId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    auto stmt = sql::buildDeleteByKey(driver->dialect(), tableSchema(driver->dialect(), tab.database), tab.tableName,
                                      tab.result->keyColumns, tab.result->keys[r].values);
    ResultGrid g;
    std::string err;
    if (driver->run(stmt, g, kRowCap, err)) {
        tab.result->rows.erase(tab.result->rows.begin() + static_cast<std::ptrdiff_t>(r));
        tab.result->keys.erase(tab.result->keys.begin() + static_cast<std::ptrdiff_t>(r));
        tab.selected.reset();
        S().statusMessage = std::string(L(StrId::Committed));
    } else {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    app::requestUpdate();
}

// 进入添一笔：在当前页尾部追加一条草稿行（全 NULL），并开启事务。
inline void startAddRow(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || !it->second.result || it->second.result->columns.empty()) {
        return;
    }
    Tab& tab = it->second;
    if (tab.dirty) {
        return;   // 已有未提交改动，先保存/取消
    }
    if (!ensureModificationTransaction(tabId)) {
        return;
    }
    tab.addingRow = true;
    tab.result->rows.push_back(
        std::vector<CellValue>(tab.result->columns.size(), CellValue{true, ""}));
    tab.selected.reset();
    tab.editing.reset();
    app::requestUpdate();
}

// 取消修改：回滚事务并重查表（还原所有编辑/删除/草稿）。
inline void cancelChanges(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end() || !it->second.dirty) {
        return;
    }
    Tab& tab = it->second;
    if (auto driver = ensureSession(tab.connectionId)) {
        ResultGrid g;
        std::string err;
        (void)driver->run(BoundStatement("ROLLBACK"), g, kRowCap, err);
    }
    tab.dirty = false;
    tab.addingRow = false;
    tab.editing.reset();
    tab.selected.reset();
    loadTableTab(tabId);
    app::requestUpdate();
}

// 提交修改：草稿行 INSERT（如有）+ COMMIT；失败回滚。之后重查表。
inline void commitChanges(const std::string& tabId) {
    auto it = S().tabs.find(tabId);
    if (it == S().tabs.end()) {
        return;
    }
    Tab& tab = it->second;
    if (!tab.dirty) {
        return;
    }
    auto driver = ensureSession(tab.connectionId);
    if (!driver) {
        tab.dirty = false;
        tab.addingRow = false;
        return;
    }
    bool ok = true;
    std::string err;
    if (tab.addingRow && tab.result && !tab.result->columns.empty()) {
        const std::vector<CellValue>& draft = tab.result->rows.back();
        std::vector<std::string> cols;
        std::vector<std::optional<std::string>> vals;
        for (std::size_t c = 0; c < tab.result->columns.size(); ++c) {
            cols.push_back(tab.result->columns[c].name);
            const CellValue& cell = c < draft.size() ? draft[c] : CellValue{true, ""};
            vals.emplace_back(cell.isNull ? std::nullopt : std::optional<std::string>(cell.text));
        }
        auto stmt = sql::buildInsert(driver->dialect(), tableSchema(driver->dialect(), tab.database),
                                     tab.tableName, cols, vals);
        ResultGrid g;
        ok = driver->run(stmt, g, kRowCap, err);
    }
    ResultGrid g2;
    std::string err2;
    (void)driver->run(BoundStatement(ok ? "COMMIT" : "ROLLBACK"), g2, kRowCap, err2);
    tab.dirty = false;
    tab.addingRow = false;
    tab.editing.reset();
    tab.selected.reset();
    if (ok) {
        S().statusMessage = std::string(L(StrId::Committed));
        loadTableTab(tabId);
    } else {
        S().statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    app::requestUpdate();
}

// 关闭标签；取消其进行中的异步任务。
inline void closeTab(const std::string& tabId) {
    core::async::cancel("load." + tabId);
    core::async::cancel("query." + tabId);
    AppState& s = S();
    auto tit = s.tabs.find(tabId);
    if (tit != s.tabs.end() && tit->second.dirty) {
        if (auto driver = ensureSession(tit->second.connectionId)) {
            ResultGrid g;
            std::string err;
            (void)driver->run(BoundStatement("ROLLBACK"), g, kRowCap, err);
        }
    }
    s.tabs.erase(tabId);
    if (auto it = std::ranges::find(s.openTabIds, tabId); it != s.openTabIds.end()) {
        s.openTabIds.erase(it);
    }
    if (s.activeTabId && *s.activeTabId == tabId) {
        s.activeTabId = s.openTabIds.empty() ? std::nullopt : std::optional<std::string>(s.openTabIds.back());
    }
    app::requestUpdate();
}

// 批量关闭标签；关闭后活动标签切到 keepTabId。
inline void closeTabIds(const std::vector<std::string>& ids, const std::string& keepTabId) {
    AppState& s = S();
    for (const std::string& id : ids) {
        core::async::cancel("load." + id);
        core::async::cancel("query." + id);
        auto tit = s.tabs.find(id);
        if (tit != s.tabs.end() && tit->second.dirty) {
            if (auto driver = ensureSession(tit->second.connectionId)) {
                ResultGrid g;
                std::string err;
                (void)driver->run(BoundStatement("ROLLBACK"), g, kRowCap, err);
            }
        }
        s.tabs.erase(id);
    }
    s.openTabIds.erase(std::remove_if(s.openTabIds.begin(), s.openTabIds.end(),
                                      [&](const std::string& id) {
                                          return std::find(ids.begin(), ids.end(), id) != ids.end();
                                      }),
                       s.openTabIds.end());
    if (s.tabs.count(keepTabId)) {
        s.activeTabId = keepTabId;
    } else if (!s.openTabIds.empty()) {
        s.activeTabId = s.openTabIds.back();
    } else {
        s.activeTabId.reset();
    }
    app::requestUpdate();
}

// 关闭左侧标签。
inline void closeTabsToLeft(const std::string& tabId) {
    std::vector<std::string> ids;
    for (const std::string& id : S().openTabIds) {
        if (id == tabId) {
            break;
        }
        ids.push_back(id);
    }
    closeTabIds(ids, tabId);
}

// 关闭右侧标签。
inline void closeTabsToRight(const std::string& tabId) {
    std::vector<std::string> ids;
    bool seen = false;
    for (const std::string& id : S().openTabIds) {
        if (seen) {
            ids.push_back(id);
        }
        if (id == tabId) {
            seen = true;
        }
    }
    closeTabIds(ids, tabId);
}

// 关闭其他标签（保留当前）。
inline void closeOtherTabs(const std::string& tabId) {
    std::vector<std::string> ids;
    for (const std::string& id : S().openTabIds) {
        if (id != tabId) {
            ids.push_back(id);
        }
    }
    closeTabIds(ids, tabId);
}

// 关闭所有标签。
inline void closeAllTabs() {
    std::vector<std::string> ids = S().openTabIds;   // 拷贝，避免与 erase 别名
    closeTabIds(ids, "");
}

// 保存连接对话框表单：按驱动分支填字段并持久化。
inline void saveConnectionFromDialog() {
    AppState& s = S();
    const bool isSqlite = s.connDriver == "sqlite";
    const bool isRedis = s.connDriver == "redis";
    if (s.connName.empty()) {
        s.connError = "名称不能为空";
        app::requestUpdate();
        return;
    }
    if (isSqlite && s.connPath.empty()) {
        s.connError = "路径不能为空";
        app::requestUpdate();
        return;
    }
    if (isRedis && s.connHost.empty()) {
        s.connError = "主机不能为空";
        app::requestUpdate();
        return;
    }
    if (!isSqlite && !isRedis && (s.connHost.empty() || s.connDatabase.empty())) {
        s.connError = "主机与数据库不能为空";
        app::requestUpdate();
        return;
    }
    ConnectionInfo info;
    info.id = newId();
    info.name = s.connName;
    info.driver = s.connDriver;
    if (isSqlite) {
        info.path = s.connPath;
    } else {
        info.host = s.connHost;
        info.user = s.connUser;
        info.password = s.connPassword;
        info.database = isRedis ? (s.connDatabase.empty() ? "0" : s.connDatabase) : s.connDatabase;
        try {
            info.port = std::stoi(s.connPort);
        } catch (...) {
            info.port = 0;
        }
    }
    s.connections.push_back(std::move(info));
    std::string err;
    if (!ConnectionStore::save(s.connections, err)) {
        s.connError = err;
        s.connections.pop_back();
        app::requestUpdate();
        return;
    }
    s.showConnDialog = false;
    s.connName.clear();
    s.connPath.clear();
    s.connHost.clear();
    s.connPort.clear();
    s.connUser.clear();
    s.connPassword.clear();
    s.connDatabase.clear();
    s.connError.clear();
    app::requestUpdate();
}

// 连接对话框第一步：选中类型 → 进入第二步填信息。
inline void chooseConnDriver(const std::string& driver) {
    S().connDriver = driver;
    S().connStep = 1;
    app::requestUpdate();
}

// 回到类型选择步。
inline void backToConnTypes() {
    S().connStep = 0;
    app::requestUpdate();
}

// 关闭连接对话框。
inline void closeConnDialog() {
    S().showConnDialog = false;
    S().connError.clear();
    app::requestUpdate();
}

// 打开连接对话框（重置表单）。
inline void openConnDialog() {
    AppState& s = S();
    s.showConnDialog = true;
    s.connStep = 0;
    s.connDriver = "sqlite";
    s.connName.clear();
    s.connPath.clear();
    s.connHost.clear();
    s.connPort.clear();
    s.connUser.clear();
    s.connPassword.clear();
    s.connDatabase.clear();
    s.connError.clear();
    app::requestUpdate();
}

// ── 侧边栏右键菜单的数据库操作 ────────────────────────────────────────────

// 刷新某连接的数据库列表（并清空表缓存，下次展开库时重拉）。
inline void refreshTables(const std::string& connId) {
    auto driver = ensureSession(connId);
    if (!driver) {
        return;
    }
    AppState& s = S();
    std::vector<std::string> dbs;
    std::string err;
    if (driver->listDatabases(dbs, err)) {
        s.databaseLists[connId] = std::move(dbs);
    } else {
        s.statusMessage = err;
    }
    s.tableLists.erase(connId);
    app::requestUpdate();
}

// 打开新建表对话框（预置一个 id 字段）。
inline void requestCreateTable() {
    S().ctxMenuOpen = false;
    S().showCreateTable = true;
    S().newTableName.clear();
    S().newTableColumns = {{"id", "INTEGER PRIMARY KEY"}};
    app::requestUpdate();
}

// 字段编辑操作。
inline void addColumnField() {
    S().newTableColumns.push_back(ColumnField{});
    app::requestUpdate();
}

inline void removeColumnField(int index) {
    auto& cols = S().newTableColumns;
    if (index >= 0 && static_cast<std::size_t>(index) < cols.size()) {
        cols.erase(cols.begin() + index);
    }
    app::requestUpdate();
}

inline void updateColumnFieldName(int index, const std::string& name) {
    auto& cols = S().newTableColumns;
    if (index >= 0 && static_cast<std::size_t>(index) < cols.size()) {
        cols[static_cast<std::size_t>(index)].name = name;
    }
}

inline void updateColumnFieldType(int index, const std::string& type) {
    auto& cols = S().newTableColumns;
    if (index >= 0 && static_cast<std::size_t>(index) < cols.size()) {
        cols[static_cast<std::size_t>(index)].type = type;
    }
}

// 确认新建表：按字段列表拼 CREATE TABLE。
inline void confirmCreateTable() {
    AppState& s = S();
    s.showCreateTable = false;
    const std::string connId = s.ctxConnId;
    const std::string name = s.newTableName;
    if (name.empty()) {
        s.statusMessage = "表名不能为空";
        app::requestUpdate();
        return;
    }
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    const Dialect d = driver->dialect();
    // 收集非空字段（字段名转义，类型原样拼接——本工具本就具备任意 SQL 权限）
    std::vector<std::string> defs;
    for (const ColumnField& f : s.newTableColumns) {
        if (f.name.empty()) {
            continue;
        }
        defs.push_back(sql::quoteIdent(d, f.name) + (f.type.empty() ? "" : " " + f.type));
    }
    if (defs.empty()) {
        s.statusMessage = "至少需要一个字段";
        app::requestUpdate();
        return;
    }
    std::string cols;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (i) {
            cols += ", ";
        }
        cols += defs[i];
    }
    const std::string sql = "CREATE TABLE " +
                            sql::qualifiedTable(d, tableSchema(d, s.ctxDatabase), name) +
                            " (" + cols + ")";
    ResultGrid g;
    std::string err;
    if (driver->run(BoundStatement(sql), g, kRowCap, err)) {
        s.statusMessage = std::string(L(StrId::Committed));
        refreshTables(connId);
    } else {
        s.statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    app::requestUpdate();
}

// 打开删除表确认框。
inline void requestDropTable() {
    S().showDropTable = true;
    S().dropTableName = S().ctxTableName;
    S().ctxMenuOpen = false;
    app::requestUpdate();
}

// 确认删除表：DROP TABLE。
inline void confirmDropTable() {
    AppState& s = S();
    s.showDropTable = false;
    const std::string connId = s.ctxConnId;
    const std::string name = s.dropTableName;
    if (name.empty()) {
        app::requestUpdate();
        return;
    }
    auto driver = ensureSession(connId);
    if (!driver) {
        app::requestUpdate();
        return;
    }
    const std::string sql = "DROP TABLE " +
                            sql::qualifiedTable(driver->dialect(),
                                               tableSchema(driver->dialect(), s.ctxDatabase), name);
    ResultGrid g;
    std::string err;
    if (driver->run(BoundStatement(sql), g, kRowCap, err)) {
        s.statusMessage = std::string(L(StrId::Committed));
        // 关闭该表的已开标签
        for (auto it = s.tabs.begin(); it != s.tabs.end();) {
            if (it->second.kind == TabKind::Table && it->second.connectionId == connId &&
                it->second.database == s.ctxDatabase && it->second.tableName == name) {
                it = s.tabs.erase(it);
            } else {
                ++it;
            }
        }
        s.openTabIds.erase(std::remove_if(s.openTabIds.begin(), s.openTabIds.end(),
                                          [&](const std::string& id) {
                                              return s.tabs.find(id) == s.tabs.end();
                                          }),
                           s.openTabIds.end());
        refreshTables(connId);
    } else {
        s.statusMessage = std::string(L(StrId::Error)) + ": " + err;
    }
    app::requestUpdate();
}

// 关闭连接：关闭 session、收起、关闭其所有标签。
inline void closeConnection(const std::string& connId) {
    AppState& s = S();
    auto sit = s.sessions.find(connId);
    if (sit != s.sessions.end()) {
        sit->second->close();
        s.sessions.erase(sit);
    }
    s.expandedConnections.erase(connId);
    s.tableLists.erase(connId);
    s.databaseLists.erase(connId);
    for (auto it = s.expandedDatabases.begin(); it != s.expandedDatabases.end();) {
        if (it->rfind(connId + "\n", 0) == 0) {
            it = s.expandedDatabases.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = s.tabs.begin(); it != s.tabs.end();) {
        if (it->second.connectionId == connId) {
            it = s.tabs.erase(it);
        } else {
            ++it;
        }
    }
    s.openTabIds.erase(std::remove_if(s.openTabIds.begin(), s.openTabIds.end(),
                                      [&](const std::string& id) {
                                          return s.tabs.find(id) == s.tabs.end();
                                      }),
                       s.openTabIds.end());
    if (s.activeConnectionId && *s.activeConnectionId == connId) {
        s.activeConnectionId.reset();
    }
    s.ctxMenuOpen = false;
    app::requestUpdate();
}

// 删除连接：关闭其会话/标签后从列表移除并持久化。
inline void removeConnection(const std::string& connId) {
    closeConnection(connId);
    auto& conns = S().connections;
    conns.erase(std::remove_if(conns.begin(), conns.end(),
                               [&](const ConnectionInfo& c) { return c.id == connId; }),
                conns.end());
    std::string err;
    ConnectionStore::save(conns, err);
    S().ctxMenuOpen = false;
    app::requestUpdate();
}

} // namespace heibu
