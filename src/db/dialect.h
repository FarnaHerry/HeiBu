#pragma once
// 黑簿 方言层 — 各数据库的标识符引用 / 占位符 / LIMIT 语法差异。纯函数，可无头测试。
#include <string>
#include <string_view>

namespace heibu {

enum class Dialect { Sqlite, MySql, Postgre, Redis };

// 双引号引用（SQLite / PostgreSQL）：内嵌双引号翻倍。
inline std::string quoteDqIdent(std::string_view ident) {
    std::string out;
    out.reserve(ident.size() + 2);
    out.push_back('"');
    for (char c : ident) {
        if (c == '"') {
            out.push_back('"');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

// 反引号引用（MySQL）：内嵌反引号翻倍。
inline std::string quoteBacktickIdent(std::string_view ident) {
    std::string out;
    out.reserve(ident.size() + 2);
    out.push_back('`');
    for (char c : ident) {
        if (c == '`') {
            out.push_back('`');
        }
        out.push_back(c);
    }
    out.push_back('`');
    return out;
}

inline std::string phSqlite(int i) { return "?" + std::to_string(i); }
inline std::string phMySql(int) { return "?"; }
inline std::string phPostgre(int i) { return "$" + std::to_string(i); }

inline std::string limitClause(int n) { return "LIMIT " + std::to_string(n); }

struct DialectTraits {
    std::string (*quoteIdent)(std::string_view);
    std::string (*placeholder)(int);
    std::string (*limitClause)(int);
};

inline const DialectTraits& dialectTraits(Dialect d) {
    static const DialectTraits sqlite{quoteDqIdent, phSqlite, limitClause};
    static const DialectTraits mysql{quoteBacktickIdent, phMySql, limitClause};
    static const DialectTraits postgre{quoteDqIdent, phPostgre, limitClause};
    switch (d) {
        case Dialect::MySql: return mysql;
        case Dialect::Postgre: return postgre;
        default: return sqlite;
    }
}

// 驱动字符串 -> 方言。未知驱动回退 SQLite（具体分派由 driver_factory 把关）。
inline Dialect dialectFromDriver(const std::string& driver) {
    if (driver == "mysql" || driver == "mariadb") {
        return Dialect::MySql;
    }
    if (driver == "postgres" || driver == "postgresql" || driver == "pg") {
        return Dialect::Postgre;
    }
    if (driver == "redis") {
        return Dialect::Redis;
    }
    return Dialect::Sqlite;
}

} // namespace heibu
