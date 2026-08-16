// 黑簿 — SqliteDriver 测试：内存库 CRUD、物化（NULL/BLOB/UTF-8）、截断、键编辑、主键检测。
#include "db/driver_sqlite.h"
#include "db/grid_utils.h"
#include "db/sql_builder.h"

import std;

namespace {
int failures = 0;

void check(bool ok, const std::string& msg) {
    if (!ok) {
        std::println("FAIL: {}", msg);
        ++failures;
    }
}

heibu::SqliteDriver openMem(std::string& err) {
    heibu::DbConfig cfg;
    cfg.path = ":memory:";
    heibu::SqliteDriver d;
    d.open(cfg, err);
    return d;
}
} // namespace

int main() {
    using namespace heibu;

    std::string err;
    SqliteDriver db = openMem(err);
    check(db.isOpen() && err.empty(), "open :memory:");
    check(!db.engineVersion().empty(), "engine version non-empty");
    check(db.engineVersion().find("SQLite") == 0, "engine version branded");
    check(db.dialect() == Dialect::Sqlite, "dialect is sqlite");

    // 建表 + 数据（含 NULL、空串、UTF-8、BLOB）
    {
        ResultGrid g;
        check(db.run("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT, age INTEGER, note TEXT, blobcol BLOB)",
                     g, 100, err), "create table: " + err);
        check(db.run("INSERT INTO people (name, age, note, blobcol) VALUES ('张三', 82, NULL, x'0102')",
                     g, 100, err), "insert row 1: " + err);
        check(db.run("INSERT INTO people (name, age, note) VALUES ('李四', NULL, '')", g, 100, err),
              "insert row 2: " + err);
    }

    // listTables：排除 sqlite_ 内部表
    {
        std::vector<TableInfo> tables;
        check(db.listTables("main", tables, err), "listTables: " + err);
        bool found = false;
        for (const auto& t : tables) {
            if (t.name == "people") {
                found = true;
                check(t.type == "table", "people type is table");
            }
        }
        check(found, "people present in listTables");
    }

    // SELECT 物化
    {
        ResultGrid g;
        check(db.run("SELECT name, age, note, blobcol FROM people ORDER BY id", g, 100, err),
              "select: " + err);
        check(g.columns.size() == 4 && g.columns[0].name == "name", "columns");
        check(g.rows.size() == 2 && !g.truncated && g.affectedRows == -1, "select shape");
        const auto& r0 = g.rows[0];
        check(!r0[0].isNull && r0[0].text == "张三", "UTF-8 text");
        check(!r0[1].isNull && r0[1].text == "82", "integer as text");
        check(r0[2].isNull, "NULL cell");
        check(!r0[3].isNull && r0[3].text == "[BLOB 2 B]", "blob rendered");
        check(g.rows[1][1].isNull, "NULL integer");
        check(!g.rows[1][2].isNull && g.rows[1][2].text.empty(), "empty string not NULL");
    }

    // DML 影响行数
    {
        ResultGrid g;
        check(db.run("UPDATE people SET age = 90 WHERE name = '张三'", g, 100, err),
              "update: " + err);
        check(g.affectedRows == 1 && g.rows.empty() && g.columns.empty(), "DML affectedRows");
    }

    // 截断
    {
        ResultGrid g;
        check(db.run("SELECT id FROM people ORDER BY id", g, 1, err), "capped select: " + err);
        check(g.rows.size() == 1 && g.truncated, "truncated flag");

        check(db.run("CREATE TABLE exact (n INTEGER)", g, 100, err), "create exact: " + err);
        for (int i = 0; i < 3; ++i) {
            check(db.run("INSERT INTO exact (n) VALUES (" + std::to_string(i) + ")", g, 100, err),
                  "insert exact: " + err);
        }
        check(db.run("SELECT n FROM exact", g, 3, err), "exact-cap select: " + err);
        check(g.rows.size() == 3 && !g.truncated, "not truncated at exact boundary");
        check(db.run("SELECT n FROM exact", g, 2, err), "below-cap select: " + err);
        check(g.rows.size() == 2 && g.truncated, "truncated when rows > cap");
    }

    // tableKey：声明主键
    {
        TableInfo ti;
        ti.name = "people";
        TableKey key;
        check(db.tableKey(ti, key, err), "tableKey people: " + err);
        check(key.columns.size() == 1 && key.columns[0] == "id", "people PK = id");
        check(!key.hidden, "declared PK not hidden");
    }

    // 键编辑：buildSelect + postProcessGrid + buildUpdate/DeleteByKey 全链路
    {
        TableInfo ti;
        ti.name = "people";
        TableKey key;
        db.tableKey(ti, key, err);
        ResultGrid g;
        check(db.run(sql::buildSelect(Dialect::Sqlite, "", "people", 100, key.hidden), g, 100, err),
              "keyed select: " + err);
        postProcessGrid(g, key.columns);
        check(g.editable, "editable via declared PK");
        check(g.keyColumns == std::vector<std::string>{"id"}, "keyColumns id");
        check(g.keys.size() == 2, "2 keys");

        const std::string id0 = g.keys[0].values[0].text;
        auto upd = sql::buildUpdateByKey(Dialect::Sqlite, "", "people", g.keyColumns, "note",
                                         std::string("批改"), g.keys[0].values);
        ResultGrid dummy;
        check(db.run(upd, dummy, 100, err), "update by key: " + err);

        ResultGrid verify;
        check(db.run("SELECT note FROM people WHERE id = " + id0, verify, 100, err),
              "verify update: " + err);
        check(verify.rows.size() == 1 && verify.rows[0][0].text == "批改", "update applied");

        auto del = sql::buildDeleteByKey(Dialect::Sqlite, "", "people", g.keyColumns, g.keys[0].values);
        check(db.run(del, dummy, 100, err), "delete by key: " + err);
        check(dummy.affectedRows == 1, "delete affectedRows == 1");
    }

    // 无声明主键的表 → rowid 隐式键
    {
        ResultGrid g;
        check(db.run("CREATE TABLE nopk (a TEXT, b TEXT)", g, 100, err), "create nopk: " + err);
        check(db.run("INSERT INTO nopk (a,b) VALUES ('x','y')", g, 100, err), "insert nopk: " + err);
        TableInfo ti;
        ti.name = "nopk";
        TableKey key;
        check(db.tableKey(ti, key, err), "tableKey nopk: " + err);
        check(key.columns.size() == 1 && key.columns[0] == "rowid", "nopk uses rowid");
        check(key.hidden, "rowid is hidden");

        ResultGrid grid;
        check(db.run(sql::buildSelect(Dialect::Sqlite, "", "nopk", 100, key.hidden), grid, 100, err),
              "nopk hidden-key select: " + err);
        postProcessGrid(grid, key.columns);
        check(grid.editable, "nopk editable via rowid");
        check(grid.columns.size() == 2, "hidden key stripped -> 2 visible cols");
        check(grid.keys.size() == 1, "one key");
    }

    // WITHOUT ROWID 表：声明主键可用（比旧 rowid 方案更强——按主键而非 rowid）
    {
        ResultGrid g;
        check(db.run("CREATE TABLE norow (k TEXT PRIMARY KEY, v TEXT) WITHOUT ROWID", g, 100, err),
              "create WITHOUT ROWID: " + err);
        TableInfo ti;
        ti.name = "norow";
        TableKey key;
        check(db.tableKey(ti, key, err), "tableKey WITHOUT ROWID: " + err);
        check(key.columns.size() == 1 && key.columns[0] == "k", "WITHOUT ROWID uses declared PK");
        check(!key.hidden, "declared PK not hidden");
    }

    // 视图：只读
    {
        ResultGrid g;
        check(db.run("CREATE VIEW pview AS SELECT name FROM people", g, 100, err),
              "create view: " + err);
        TableInfo ti;
        ti.name = "pview";
        TableKey key;
        check(db.tableKey(ti, key, err), "tableKey view: " + err);
        check(key.columns.empty(), "view has no key");
        check(!key.hidden, "view not hidden");
    }

    // INSERT 往返（builder，NULL 走绑定）
    {
        auto ins = sql::buildInsert(Dialect::Sqlite, "", "people", {"name", "age", "note"},
                                    {std::string("赵六"), std::string("30"), std::nullopt});
        ResultGrid g;
        check(db.run(ins, g, 100, err), "insert via builder: " + err);
        check(g.affectedRows == 1, "insert affectedRows == 1");
        ResultGrid verify;
        check(db.run("SELECT name, note FROM people WHERE name = '赵六'", verify, 100, err),
              "verify insert: " + err);
        check(verify.rows.size() == 1 && verify.rows[0][0].text == "赵六", "inserted name present");
        check(verify.rows[0][1].isNull, "inserted NULL note");
    }

    // 错误 SQL
    {
        ResultGrid g;
        check(!db.run("SELECT * FROM nonexistent_table", g, 100, err), "bad sql fails");
        check(!err.empty(), "bad sql sets error");
    }

    if (failures == 0) {
        std::println("test_driver_sqlite: ok");
        return 0;
    }
    std::println("test_driver_sqlite: {} failure(s)", failures);
    return 1;
}
