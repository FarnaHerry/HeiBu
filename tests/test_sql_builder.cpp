// 黑簿 — sql_builder 纯函数测试：方言标识符转义、ByKey CRUD 语句形态、NULL vs 空串绑定。
import heibu.db.dialect;
import heibu.db.sql_builder;
import heibu.db.types;

import std;

namespace {
int failures = 0;

void check(bool ok, const std::string& msg) {
    if (!ok) {
        std::println("FAIL: {}", msg);
        ++failures;
    }
}

void checkEq(const std::string& got, const std::string& want, const std::string& msg) {
    if (got != want) {
        std::println("FAIL: {} — got '{}', want '{}'", msg, got, want);
        ++failures;
    }
}
} // namespace

int main() {
    using namespace heibu;
    using namespace heibu::sql;
    const Dialect S = Dialect::Sqlite;
    const Dialect M = Dialect::MySql;
    const Dialect P = Dialect::Postgre;

    // 标识符引用（按方言）
    checkEq(quoteIdent(S, "abc"), "\"abc\"", "sqlite plain ident");
    checkEq(quoteIdent(S, "we\"ird"), "\"we\"\"ird\"", "sqlite embedded quote doubled");
    checkEq(quoteIdent(S, "命籍"), "\"命籍\"", "CJK ident");
    checkEq(quoteIdent(M, "tbl`name"), "`tbl``name`", "mysql backtick doubled");
    checkEq(quoteIdent(P, "abc"), "\"abc\"", "postgre double-quote");

    // buildSelect
    checkEq(buildSelect(S, "", "users", 50000, false),
            "SELECT * FROM \"users\" LIMIT 50001", "sqlite select cap+1");
    checkEq(buildSelect(S, "", "users", 50000, true),
            "SELECT rowid AS \"__heibu_key__\", * FROM \"users\" LIMIT 50001",
            "sqlite hidden-key select");
    checkEq(buildSelect(M, "", "users", 100, false), "SELECT * FROM `users` LIMIT 101",
            "mysql select");
    checkEq(buildSelect(P, "public", "users", 100, false),
            "SELECT * FROM \"public\".\"users\" LIMIT 101", "postgres schema-qualified select");

    // buildUpdateByKey：单键 / 复合键 / NULL 区分
    {
        auto upd = buildUpdateByKey(S, "", "t", {"id"}, "c", std::string("hello"),
                                    {CellValue{false, "7"}});
        checkEq(upd.sql, "UPDATE \"t\" SET \"c\" = ?1 WHERE \"id\" = ?2", "update single-key sql");
        check(upd.params.size() == 2 && upd.params[0].has_value() && *upd.params[0] == "hello" &&
                  upd.params[1].has_value() && *upd.params[1] == "7",
              "update params: value + key");
    }
    {
        auto upd = buildUpdateByKey(P, "public", "t", {"a", "b"}, "c", std::nullopt,
                                    {CellValue{false, "1"}, CellValue{true, ""}});
        checkEq(upd.sql, "UPDATE \"public\".\"t\" SET \"c\" = $1 WHERE \"a\" = $2 AND \"b\" = $3",
                "postgres composite-key update");
        check(upd.params.size() == 3 && !upd.params[0].has_value() && upd.params[1].has_value() &&
                  *upd.params[1] == "1" && !upd.params[2].has_value(),
              "postgres NULL + key values bound");
    }
    {
        auto upd = buildUpdateByKey(S, "", "t", {"id"}, "c", std::string(""),
                                    {CellValue{false, "7"}});
        check(upd.params[0].has_value() && upd.params[0]->empty(),
              "empty string kept distinct from NULL");
    }

    // buildInsert
    {
        auto ins = buildInsert(S, "", "t", {"a", "b"}, {std::string("x"), std::nullopt});
        checkEq(ins.sql, "INSERT INTO \"t\" (\"a\", \"b\") VALUES (?1, ?2)", "insert sql");
        check(ins.params.size() == 2 && ins.params[0].has_value() && !ins.params[1].has_value(),
              "insert NULL preserved");
    }
    {
        auto ins = buildInsert(M, "", "t", {"a"}, {std::string("x")});
        checkEq(ins.sql, "INSERT INTO `t` (`a`) VALUES (?)", "mysql insert placeholder");
    }

    // buildDeleteByKey
    {
        auto del = buildDeleteByKey(S, "", "t", {"id"}, {CellValue{false, "42"}});
        checkEq(del.sql, "DELETE FROM \"t\" WHERE \"id\" = ?1", "delete single-key sql");
        check(del.params.size() == 1 && del.params[0].has_value() && *del.params[0] == "42",
              "delete key bound");
    }
    {
        auto del = buildDeleteByKey(S, "", "t", {"a", "b"},
                                    {CellValue{false, "1"}, CellValue{false, "2"}});
        checkEq(del.sql, "DELETE FROM \"t\" WHERE \"a\" = ?1 AND \"b\" = ?2",
                "delete composite-key sql");
    }

    if (failures == 0) {
        std::println("test_sql_builder: ok");
        return 0;
    }
    std::println("test_sql_builder: {} failure(s)", failures);
    return 1;
}
