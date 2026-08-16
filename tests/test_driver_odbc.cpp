// 黑簿 — DriverOdbc 端到端：连本机 MySQL，跑 SELECT VERSION() 与 listTables。
// 本机无 MySQL ODBC 驱动 / 服务时优雅 SKIP（不判失败）。
#include "db/driver_odbc.h"

import std;

namespace {
int failures = 0;

void check(bool ok, const std::string& msg) {
    if (!ok) {
        std::println("FAIL: {}", msg);
        ++failures;
    }
}
} // namespace

int main() {
    using namespace heibu;

    DbConfig cfg;
    cfg.driver = "mysql";
    cfg.host = "127.0.0.1";
    cfg.port = 3306;
    cfg.user = "root";
    cfg.password = "root";
    cfg.database = "mysql";

    DriverOdbc db(Dialect::MySql);
    std::string err;
    if (!db.open(cfg, err)) {
        std::println("test_driver_odbc: SKIP (no MySQL reachable) — {}", err);
        return 0;
    }

    check(db.dialect() == Dialect::MySql, "dialect is mysql");
    check(!db.engineVersion().empty(), "engine version non-empty");

    // listTables：mysql 系统库表很多
    std::vector<TableInfo> tables;
    check(db.listTables("mysql", tables, err), "listTables: " + err);
    check(!tables.empty(), "mysql has tables");

    // run：SELECT VERSION()
    ResultGrid g;
    check(db.run(BoundStatement("SELECT VERSION()"), g, 100, err), "select version: " + err);
    check(g.columns.size() == 1 && g.rows.size() == 1 && !g.rows[0][0].isNull, "version row");
    if (g.rows.size() == 1 && !g.rows[0][0].isNull) {
        std::println("  MySQL VERSION = {}", g.rows[0][0].text);
    }

    if (failures == 0) {
        std::println("test_driver_odbc: ok");
        return 0;
    }
    std::println("test_driver_odbc: {} failure(s)", failures);
    return 1;
}
