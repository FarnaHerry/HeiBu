#pragma once
// 黑簿 数据库驱动抽象 — v2：方言感知、主键编辑模型、服务器连接配置。
// 本头不包含任何具体客户端库头（sqlite3.h / libpq-fe.h 等），保持可无头测试、可被 UI 层与驱动实现共同依赖。
#include "db/dialect.h"
#include "db/types.h"

#include <functional>
#include <string>
#include <vector>

namespace heibu {

// 协作式取消检查：返回 true 表示应尽早中止（查询会周期性轮询）。
using CancelCheck = std::function<bool()>;

struct IDbDriver {
    virtual ~IDbDriver() = default;

    [[nodiscard]] virtual bool open(const DbConfig& cfg, std::string& error) = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    virtual void close() = 0;

    // 本驱动方言（决定标识符引用/占位符/LIMIT 语法）。
    [[nodiscard]] virtual Dialect dialect() const = 0;

    // 列出数据库/模式名（MySQL 的 database、PG 的 schema；SQLite 恒 {"main"}）。
    [[nodiscard]] virtual bool listDatabases(std::vector<std::string>& out, std::string& error) = 0;

    // 列出某数据库/模式下的表。
    [[nodiscard]] virtual bool listTables(const std::string& database, std::vector<TableInfo>& out,
                                          std::string& error) = 0;

    // 列出某数据库/模式下的触发器（type="trigger"，parent 为所属表）。
    [[nodiscard]] virtual bool listTriggers(const std::string& database, std::vector<TableInfo>& out,
                                            std::string& error) = 0;

    // 列出某数据库/模式下的存储过程与函数（type="procedure"/"function"）。
    [[nodiscard]] virtual bool listRoutines(const std::string& database, std::vector<TableInfo>& out,
                                            std::string& error) = 0;

    // 取对象（视图/触发器/表）的创建语句。
    [[nodiscard]] virtual bool objectDdl(const std::string& database, const std::string& name,
                                         const std::string& type, std::string& ddl,
                                         std::string& error) = 0;

    // 取非表对象的值视图（Redis 键等），物化进 out 网格。SQL 驱动默认不支持。
    [[nodiscard]] virtual bool keyValue(const std::string& database, const std::string& key,
                                        const std::string& type, ResultGrid& out,
                                        std::string& error) {
        (void)database;
        (void)key;
        (void)type;
        (void)out;
        error = "该驱动不支持键值查看";
        return false;
    }

    // 查单表主键；无主键时（SQLite 可用 rowid 时返回 {"rowid"} + hidden）返回空列 = 只读。
    [[nodiscard]] virtual bool tableKey(const TableInfo& t, TableKey& key, std::string& error) = 0;

    // 执行任意参数化语句。若语句返回列（SELECT/PRAGMA），物化进 out.columns/out.rows；
    // 否则为 DML，填充 out.affectedRows。行数达 rowCap 时置 truncated 并截断。
    [[nodiscard]] virtual bool run(const BoundStatement& stmt, ResultGrid& out, std::int64_t rowCap,
                                   std::string& error, CancelCheck canceled = {}) = 0;

    // 完整品牌串，如 "SQLite 3.45.3"。
    [[nodiscard]] virtual std::string engineVersion() const = 0;
};

} // namespace heibu
