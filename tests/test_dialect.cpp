// 黑簿 — 方言层纯函数测试：driver 字符串映射、占位符、LIMIT、标识符引用。
import heibu.db.dialect;

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

    // driver 字符串 -> 方言
    check(dialectFromDriver("sqlite") == Dialect::Sqlite, "sqlite");
    check(dialectFromDriver("") == Dialect::Sqlite, "empty -> sqlite");
    check(dialectFromDriver("mysql") == Dialect::MySql, "mysql");
    check(dialectFromDriver("mariadb") == Dialect::MySql, "mariadb -> mysql");
    check(dialectFromDriver("postgres") == Dialect::Postgre, "postgres");
    check(dialectFromDriver("postgresql") == Dialect::Postgre, "postgresql");
    check(dialectFromDriver("pg") == Dialect::Postgre, "pg");
    check(dialectFromDriver("bogus") == Dialect::Sqlite, "unknown -> sqlite");

    // 占位符 + LIMIT
    checkEq(dialectTraits(Dialect::Sqlite).placeholder(2), "?2", "sqlite placeholder");
    checkEq(dialectTraits(Dialect::MySql).placeholder(2), "?", "mysql placeholder");
    checkEq(dialectTraits(Dialect::Postgre).placeholder(2), "$2", "postgres placeholder");
    checkEq(dialectTraits(Dialect::Sqlite).limitClause(10), "LIMIT 10", "limit clause");

    // 标识符引用
    checkEq(dialectTraits(Dialect::Sqlite).quoteIdent("a\"b"), "\"a\"\"b\"", "sqlite quote");
    checkEq(dialectTraits(Dialect::MySql).quoteIdent("a`b"), "`a``b`", "mysql quote");
    checkEq(dialectTraits(Dialect::Postgre).quoteIdent("a\"b"), "\"a\"\"b\"", "postgres quote");

    if (failures == 0) {
        std::println("test_dialect: ok");
        return 0;
    }
    std::println("test_dialect: {} failure(s)", failures);
    return 1;
}
