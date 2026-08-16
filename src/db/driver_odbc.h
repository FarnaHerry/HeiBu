#pragma once
// 黑簿 ODBC 驱动 — 通过 heibu.odbc 模块（包 nanodbc）连任意有 ODBC 驱动的库（MySQL/PG/SQL Server…）。
// 键检测走 nanodbc::catalog::primary_keys；无主键的表只读。
#include "db/driver.h"
import heibu.odbc;

#include <memory>
#include <string>
#include <vector>

namespace heibu {

class DriverOdbc : public IDbDriver {
public:
    explicit DriverOdbc(Dialect d) : dialect_(d) {}
    ~DriverOdbc() override { close(); }

    bool open(const DbConfig& cfg, std::string& error) override {
        const std::string name = findDriver(cfg.driver);
        if (name.empty()) {
            error = "未找到 ODBC 驱动: " + cfg.driver;
            return false;
        }
        try {
            conn_ = std::make_unique<odbc::connection>(buildConnectionString(cfg, name));
            if (conn_->connected()) {
                return true;
            }
            error = "ODBC 连接失败";
        } catch (const std::exception& e) {
            error = e.what();
        }
        conn_.reset();
        return false;
    }

    bool isOpen() const override { return conn_ && conn_->connected(); }

    Dialect dialect() const override { return dialect_; }

    void close() override { conn_.reset(); }

    bool listDatabases(std::vector<std::string>& out, std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        try {
            out.clear();
            odbc::catalog cat(*conn_);
            if (dialect_ == Dialect::MySql) {
                auto cats = cat.list_catalogs();
                out.assign(cats.begin(), cats.end());
            } else if (dialect_ == Dialect::Postgre) {
                auto schemas = cat.list_schemas();
                out.assign(schemas.begin(), schemas.end());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool listTables(const std::string& database, std::vector<TableInfo>& out,
                    std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        try {
            out.clear();
            if (dialect_ == Dialect::MySql) {
                // MySQL：SQLTables 不返回视图，改用 information_schema.TABLES。
                const std::string sql =
                    "SELECT TABLE_NAME, TABLE_TYPE FROM information_schema.TABLES "
                    "WHERE TABLE_SCHEMA = '" + database + "' ORDER BY TABLE_NAME";
                odbc::statement s(*conn_, sql);
                odbc::result r = s.execute();
                while (r.next()) {
                    TableInfo info;
                    info.name = r.get<std::string>(0);
                    const std::string tt = r.get<std::string>(1);
                    info.type = (tt == "VIEW" || tt == "SYSTEM VIEW") ? "view" : "table";
                    info.schema = database;
                    out.push_back(std::move(info));
                }
            } else {
                odbc::catalog cat(*conn_);
                odbc::catalog::tables t = findTables(cat, database);
                while (t.next()) {
                    TableInfo info;
                    info.name = t.table_name();
                    info.schema = database;
                    const std::string raw = t.table_type();
                    info.type = (raw == "VIEW" || raw == "SYSTEM VIEW") ? "view" : "table";
                    out.push_back(std::move(info));
                }
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool tableKey(const TableInfo& t, TableKey& key, std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        try {
            key = TableKey{};
            odbc::catalog cat(*conn_);
            auto pk = [&]() {
                if (dialect_ == Dialect::MySql) {
                    return cat.find_primary_keys(t.name, "", t.schema);
                }
                if (dialect_ == Dialect::Postgre) {
                    return cat.find_primary_keys(t.name, t.schema, "");
                }
                return cat.find_primary_keys(t.name);
            }();
            while (pk.next()) {
                key.columns.push_back(pk.column_name());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool listTriggers(const std::string& database, std::vector<TableInfo>& out,
                      std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        out.clear();
        try {
            const std::string sql =
                "SHOW TRIGGERS FROM " + dialectTraits(dialect_).quoteIdent(database);
            odbc::statement s(*conn_, sql);
            odbc::result r = s.execute();
            while (r.next()) {
                TableInfo t;
                t.name = r.get<std::string>(0);    // Trigger
                t.type = "trigger";
                t.parent = r.get<std::string>(2);  // Table
                t.schema = database;
                out.push_back(std::move(t));
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool listRoutines(const std::string& database, std::vector<TableInfo>& out,
                      std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        out.clear();
        try {
            const std::string sql =
                "SELECT ROUTINE_NAME, ROUTINE_TYPE FROM information_schema.ROUTINES "
                "WHERE ROUTINE_SCHEMA = '" + database + "' ORDER BY ROUTINE_NAME";
            odbc::statement s(*conn_, sql);
            odbc::result r = s.execute();
            while (r.next()) {
                TableInfo t;
                t.name = r.get<std::string>(0);
                t.type = (r.get<std::string>(1) == "FUNCTION") ? "function" : "procedure";
                t.schema = database;
                out.push_back(std::move(t));
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool objectDdl(const std::string& /*database*/, const std::string& name, const std::string& type,
                   std::string& ddl, std::string& error) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        try {
            std::string sql;
            if (type == "trigger") {
                sql = "SHOW CREATE TRIGGER " + dialectTraits(dialect_).quoteIdent(name);
            } else if (type == "procedure") {
                sql = "SHOW CREATE PROCEDURE " + dialectTraits(dialect_).quoteIdent(name);
            } else if (type == "function") {
                sql = "SHOW CREATE FUNCTION " + dialectTraits(dialect_).quoteIdent(name);
            } else if (type == "view") {
                sql = "SHOW CREATE VIEW " + dialectTraits(dialect_).quoteIdent(name);
            } else {
                sql = "SHOW CREATE TABLE " + dialectTraits(dialect_).quoteIdent(name);
            }
            odbc::statement s(*conn_, sql);
            odbc::result r = s.execute();
            ddl.clear();
            const int col = (type == "trigger" || type == "procedure" || type == "function") ? 2 : 1;
            if (r.next() && r.columns() > col) {
                ddl = r.get<std::string>(static_cast<short>(col));
            }
            if (ddl.empty()) {
                error = "未找到该对象的定义";
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    bool run(const BoundStatement& stmt, ResultGrid& out, std::int64_t rowCap,
             std::string& error, CancelCheck canceled = {}) override {
        if (!isOpen()) {
            error = "not connected";
            return false;
        }
        try {
            out = ResultGrid{};
            odbc::statement s(*conn_, stmt.sql);
            for (std::size_t i = 0; i < stmt.params.size(); ++i) {
                const short idx = static_cast<short>(i);
                if (stmt.params[i]) {
                    s.bind(idx, stmt.params[i]->c_str());
                } else {
                    s.bind_null(idx);
                }
            }
            odbc::result r = s.execute();

            const short colCount = r.columns();
            if (colCount > 0) {
                for (short c = 0; c < colCount; ++c) {
                    Column col;
                    col.name = r.column_name(c);
                    col.declType = r.column_datatype_name(c);
                    out.columns.push_back(std::move(col));
                }
                std::int64_t fetched = 0;
                while (r.next()) {
                    if (canceled && canceled()) {
                        error = "canceled";
                        return false;
                    }
                    if (fetched >= rowCap) {
                        out.truncated = true;
                        break;
                    }
                    std::vector<CellValue> row;
                    row.reserve(static_cast<std::size_t>(colCount));
                    for (short c = 0; c < colCount; ++c) {
                        CellValue cell;
                        cell.isNull = r.is_null(c);
                        if (!cell.isNull) {
                            cell.text = r.get<std::string>(c);
                        }
                        row.push_back(std::move(cell));
                    }
                    out.rows.push_back(std::move(row));
                    ++fetched;
                }
                out.affectedRows = -1;
            } else {
                out.affectedRows = r.has_affected_rows() ? r.affected_rows() : 0;
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    std::string engineVersion() const override {
        if (!conn_) {
            return "ODBC";
        }
        try {
            return conn_->dbms_name() + " " + conn_->dbms_version();
        } catch (...) {
            return "ODBC";
        }
    }

private:
    // 从系统已装 ODBC 驱动里，按关键字找驱动名（MySQL/PG 偏好 Unicode 版）。
    // 好处是驱动名随版本变化（8.0 / 8.4 / 26.7 …）也能自动命中，不用写死。
    static std::string findDriver(const std::string& driver) {
        std::string keyword;
        if (driver == "mysql" || driver == "mariadb") {
            keyword = "MySQL ODBC";
        } else if (driver == "postgres" || driver == "postgresql" || driver == "pg") {
            keyword = "PostgreSQL";
        } else {
            return driver;   // 直接把 driver 当 ODBC 驱动名（如自定义驱动）
        }
        std::string fallback;
        try {
            for (const auto& d : odbc::list_drivers()) {
                if (d.name.find(keyword) == std::string::npos) {
                    continue;
                }
                if (d.name.find("Unicode") != std::string::npos) {
                    return d.name;   // 优先 Unicode 版（UTF-8 数据）
                }
                if (fallback.empty()) {
                    fallback = d.name;
                }
            }
        } catch (...) {
        }
        return fallback;
    }

    // 组 DSN-less 连接串。
    static std::string buildConnectionString(const DbConfig& cfg, const std::string& driverName) {
        std::string s = "DRIVER={" + driverName + "};";
        if (!cfg.host.empty()) {
            s += "SERVER=" + cfg.host + ";";
        }
        if (cfg.port != 0) {
            s += "PORT=" + std::to_string(cfg.port) + ";";
        }
        if (!cfg.database.empty()) {
            s += "DATABASE=" + cfg.database + ";";
        }
        if (!cfg.user.empty()) {
            s += "UID=" + cfg.user + ";";
        }
        if (!cfg.password.empty()) {
            s += "PWD=" + cfg.password + ";";
        }
        return s;
    }

    // MySQL 的 database 在 catalog 位置、PG 在 schema 位置。
    odbc::catalog::tables findTables(odbc::catalog& cat, const std::string& database) {
        if (dialect_ == Dialect::MySql) {
            return cat.find_tables("", "", "", database);
        }
        if (dialect_ == Dialect::Postgre) {
            return cat.find_tables("", "", database, "");
        }
        return cat.find_tables();
    }

    Dialect dialect_;
    std::unique_ptr<odbc::connection> conn_;
};

} // namespace heibu
