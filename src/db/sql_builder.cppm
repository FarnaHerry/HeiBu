// 黑簿 SQL 生成器 — 纯函数，返回 BoundStatement{sql, 绑定参数}。所有值走参数绑定，
// 永不拼接字符串字面量，从而免疫注入，也让 CRUD 逻辑可无头单测。方言由首参 Dialect 决定。
export module heibu.db.sql_builder;
import std;
import heibu.db.dialect;
import heibu.db.types;

export namespace heibu::sql {

// 引用标识符（按方言）。
inline std::string quoteIdent(Dialect d, const std::string& ident) {
    return dialectTraits(d).quoteIdent(ident);
}

// 限定表名：schema 非空时 "schema"."table"，否则 "table"。
inline std::string qualifiedTable(Dialect d, const std::string& schema, const std::string& table) {
    std::string q = dialectTraits(d).quoteIdent(table);
    if (!schema.empty()) {
        q = dialectTraits(d).quoteIdent(schema) + "." + q;
    }
    return q;
}

// 表浏览：多取 1 行判「截断」。hidden=true（SQLite 无声明主键）时带 rowid 隐藏键列。
inline std::string buildSelect(Dialect d, const std::string& schema, const std::string& table,
                               std::int64_t rowCap, bool hidden) {
    std::string sql = "SELECT ";
    if (hidden) {
        sql += "rowid AS \"" + std::string(kHiddenKeyColumn) + "\", ";
    }
    sql += "* FROM " + qualifiedTable(d, schema, table) + " " +
           dialectTraits(d).limitClause(static_cast<int>(rowCap + 1));
    return sql;
}

// 分页表浏览：LIMIT limit OFFSET offset（三库通用语法）。
inline std::string buildSelectPaged(Dialect d, const std::string& schema, const std::string& table,
                                    std::int64_t limit, std::int64_t offset, bool hidden) {
    std::string sql = "SELECT ";
    if (hidden) {
        sql += "rowid AS \"" + std::string(kHiddenKeyColumn) + "\", ";
    }
    sql += "* FROM " + qualifiedTable(d, schema, table) + " LIMIT " + std::to_string(limit) +
           " OFFSET " + std::to_string(offset);
    return sql;
}

// 表总行数。
inline std::string buildCount(Dialect d, const std::string& schema, const std::string& table) {
    return "SELECT COUNT(*) FROM " + qualifiedTable(d, schema, table);
}

// UPDATE "t" SET "c" = ?1 WHERE "k1" = ?2 [AND "k2" = ?3 ...]
// 键值按序跟在 set 值后绑定（keyValues 与 keyColumns 一一对应）。
inline BoundStatement buildUpdateByKey(Dialect d, const std::string& schema,
                                       const std::string& table,
                                       const std::vector<std::string>& keyColumns,
                                       const std::string& column,
                                       const std::optional<std::string>& value,
                                       const std::vector<CellValue>& keyValues) {
    BoundStatement s;
    const DialectTraits& tr = dialectTraits(d);
    s.sql = "UPDATE " + qualifiedTable(d, schema, table) + " SET " +
            tr.quoteIdent(column) + " = " + tr.placeholder(1) + " WHERE ";
    for (std::size_t i = 0; i < keyColumns.size(); ++i) {
        if (i) {
            s.sql += " AND ";
        }
        s.sql += tr.quoteIdent(keyColumns[i]) + " = " + tr.placeholder(static_cast<int>(i + 2));
    }
    s.params.push_back(value);
    for (const CellValue& kv : keyValues) {
        s.params.push_back(kv.isNull ? std::nullopt : std::optional<std::string>(kv.text));
    }
    return s;
}

// INSERT INTO "t" ("a","b") VALUES (?1, ?2)
inline BoundStatement buildInsert(Dialect d, const std::string& schema,
                                  const std::string& table,
                                  const std::vector<std::string>& columns,
                                  const std::vector<std::optional<std::string>>& values) {
    BoundStatement s;
    const DialectTraits& tr = dialectTraits(d);
    std::string colList;
    std::string valList;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i) {
            colList += ", ";
            valList += ", ";
        }
        colList += tr.quoteIdent(columns[i]);
        valList += tr.placeholder(static_cast<int>(i + 1));
    }
    s.sql = "INSERT INTO " + qualifiedTable(d, schema, table) + " (" + colList + ") VALUES (" +
            valList + ")";
    s.params = values;
    return s;
}

// DELETE FROM "t" WHERE "k1" = ?1 [AND "k2" = ?2 ...]
inline BoundStatement buildDeleteByKey(Dialect d, const std::string& schema,
                                       const std::string& table,
                                       const std::vector<std::string>& keyColumns,
                                       const std::vector<CellValue>& keyValues) {
    BoundStatement s;
    const DialectTraits& tr = dialectTraits(d);
    s.sql = "DELETE FROM " + qualifiedTable(d, schema, table) + " WHERE ";
    for (std::size_t i = 0; i < keyColumns.size(); ++i) {
        if (i) {
            s.sql += " AND ";
        }
        s.sql += tr.quoteIdent(keyColumns[i]) + " = " + tr.placeholder(static_cast<int>(i + 1));
    }
    for (const CellValue& kv : keyValues) {
        s.params.push_back(kv.isNull ? std::nullopt : std::optional<std::string>(kv.text));
    }
    return s;
}

} // namespace heibu::sql
