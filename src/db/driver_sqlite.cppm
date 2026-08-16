// 黑簿 SQLite 驱动 — 唯一 include <sqlite3.h> 的地方。
// 线程模型：SQLITE_OPEN_FULLMUTEX + 每连接 std::mutex 串行化；同连接的并发查询排队。
module;
#include <sqlite3.h>

export module heibu.db.driver_sqlite;
import std;
import heibu.db.dialect;
import heibu.db.driver;
import heibu.db.types;

export namespace heibu {

class SqliteDriver : public IDbDriver {
public:
    SqliteDriver() = default;
    ~SqliteDriver() override { close(); }

    // 含 std::mutex，不可拷贝；可移动（生产里以 shared_ptr 持有，这里仅为测试便利）。
    SqliteDriver(const SqliteDriver&) = delete;
    SqliteDriver& operator=(const SqliteDriver&) = delete;
    SqliteDriver(SqliteDriver&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    SqliteDriver& operator=(SqliteDriver&& other) noexcept {
        if (this != &other) {
            close();
            std::lock_guard<std::mutex> lock(other.mutex_);
            db_ = other.db_;
            other.db_ = nullptr;
        }
        return *this;
    }

    bool open(const DbConfig& cfg, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
        if (sqlite3_open_v2(cfg.path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
            error = db_ ? sqlite3_errmsg(db_) : "sqlite open failed";
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            return false;
        }
        sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        return true;
    }

    bool isOpen() const override { return db_ != nullptr; }

    Dialect dialect() const override { return Dialect::Sqlite; }

    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    // 单表主键：先 PRAGMA table_info 取声明主键；无声明主键时（普通表）回退 rowid；
    // 视图 / WITHOUT ROWID 表（必含主键，故走声明主键路径）→ 无键则只读。
    bool tableKey(const TableInfo& t, TableKey& key, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        key = TableKey{};
        if (!db_) {
            error = "not connected";
            return false;
        }
        const std::string qname = quoteName(t.name);

        // 1) 声明主键（pk>0 的列，按 pk 序号升序）。
        sqlite3_stmt* stmt = nullptr;
        const std::string pragma = "PRAGMA table_info(" + qname + ")";
        if (sqlite3_prepare_v2(db_, pragma.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db_);
            return false;
        }
        std::vector<std::pair<int, std::string>> pks;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const int pk = sqlite3_column_int(stmt, 5);
            if (pk > 0) {
                const unsigned char* n = sqlite3_column_text(stmt, 1);
                pks.emplace_back(pk, n ? reinterpret_cast<const char*>(n) : "");
            }
        }
        sqlite3_finalize(stmt);
        if (!pks.empty()) {
            std::sort(pks.begin(), pks.end());
            for (auto& [pk, name] : pks) {
                key.columns.push_back(name);
            }
            return true;
        }

        // 2) 无声明主键：能否用 rowid（普通表可用，视图不可用）。
        const std::string probe = "SELECT rowid FROM " + qname + " LIMIT 1";
        if (sqlite3_prepare_v2(db_, probe.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_finalize(stmt);
            key.columns = {"rowid"};
            key.hidden = true;
            return true;
        }
        key.columns.clear();
        key.hidden = false;
        return true;
    }

    bool listDatabases(std::vector<std::string>& out, std::string& error) override {
        out = {"main"};
        return true;
    }

    bool listTables(const std::string& /*database*/, std::vector<TableInfo>& out,
                    std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            error = "not connected";
            return false;
        }
        out.clear();
        const char* sql = "SELECT name, type FROM sqlite_master "
                          "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                          "ORDER BY name";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db_);
            return false;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TableInfo t;
            const unsigned char* n = sqlite3_column_text(stmt, 0);
            const unsigned char* ty = sqlite3_column_text(stmt, 1);
            t.name = n ? reinterpret_cast<const char*>(n) : "";
            t.type = ty ? reinterpret_cast<const char*>(ty) : "";
            out.push_back(std::move(t));
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool listTriggers(const std::string& /*database*/, std::vector<TableInfo>& out,
                      std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            error = "not connected";
            return false;
        }
        out.clear();
        const char* sql = "SELECT name, tbl_name FROM sqlite_master WHERE type='trigger' ORDER BY name";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db_);
            return false;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TableInfo t;
            const unsigned char* n = sqlite3_column_text(stmt, 0);
            const unsigned char* tbl = sqlite3_column_text(stmt, 1);
            t.name = n ? reinterpret_cast<const char*>(n) : "";
            t.type = "trigger";
            t.parent = tbl ? reinterpret_cast<const char*>(tbl) : "";
            out.push_back(std::move(t));
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool listRoutines(const std::string& /*database*/, std::vector<TableInfo>& out,
                      std::string& error) override {
        out.clear();   // SQLite 无存储过程/函数
        return true;
    }

    bool objectDdl(const std::string& /*database*/, const std::string& name, const std::string& type,
                   std::string& ddl, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            error = "not connected";
            return false;
        }
        const char* sql = "SELECT sql FROM sqlite_master WHERE type = ? AND name = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db_);
            return false;
        }
        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        ddl.clear();
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* s = sqlite3_column_text(stmt, 0);
            ddl = s ? reinterpret_cast<const char*>(s) : "";
        }
        sqlite3_finalize(stmt);
        if (ddl.empty()) {
            error = "未找到该对象的定义";
            return false;
        }
        return true;
    }

    bool run(const BoundStatement& statement, ResultGrid& out, std::int64_t rowCap,
             std::string& error, CancelCheck canceled = {}) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            error = "not connected";
            return false;
        }
        out = ResultGrid{};
        const auto t0 = std::chrono::steady_clock::now();

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, statement.sql.c_str(), static_cast<int>(statement.sql.size()),
                               &stmt, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db_);
            return false;
        }

        // 绑定参数：nullopt => NULL，否则文本（SQLITE_TRANSIENT 让 sqlite 自行拷贝）。
        for (std::size_t i = 0; i < statement.params.size(); ++i) {
            const int index = static_cast<int>(i) + 1;
            const std::optional<std::string>& p = statement.params[i];
            if (p.has_value()) {
                sqlite3_bind_text(stmt, index, p->c_str(), static_cast<int>(p->size()), SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt, index);
            }
        }

        const int colCount = sqlite3_column_count(stmt);
        if (colCount > 0) {
            for (int c = 0; c < colCount; ++c) {
                Column col;
                const char* name = sqlite3_column_name(stmt, c);
                col.name = name ? name : "";
                const char* decl = sqlite3_column_decltype(stmt, c);
                col.declType = decl ? decl : "";
                out.columns.push_back(std::move(col));
            }

            std::int64_t fetched = 0;
            while (true) {
                if (canceled && canceled()) {
                    sqlite3_finalize(stmt);
                    error = "canceled";
                    return false;
                }
                const int rc = sqlite3_step(stmt);
                if (rc == SQLITE_ROW) {
                    if (fetched >= rowCap) {
                        out.truncated = true;   // 多取到第 rowCap+1 行证明截断，不存储
                        break;
                    }
                    std::vector<CellValue> row;
                    row.reserve(static_cast<std::size_t>(colCount));
                    for (int c = 0; c < colCount; ++c) {
                        row.push_back(readCell(stmt, c));
                    }
                    out.rows.push_back(std::move(row));
                    ++fetched;
                } else if (rc == SQLITE_DONE) {
                    break;
                } else {
                    error = sqlite3_errmsg(db_);
                    sqlite3_finalize(stmt);
                    return false;
                }
            }
            out.affectedRows = -1;
        } else {
            const int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                error = sqlite3_errmsg(db_);
                sqlite3_finalize(stmt);
                return false;
            }
            out.affectedRows = sqlite3_changes(db_);
        }

        sqlite3_finalize(stmt);
        const auto t1 = std::chrono::steady_clock::now();
        out.elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return true;
    }

    std::string engineVersion() const override {
        return "SQLite " + std::string(sqlite3_libversion());
    }

private:
    // 双引号引用标识符（内嵌双引号翻倍），供 PRAGMA / 探测语句拼接。
    static std::string quoteName(const std::string& name) {
        std::string out = "\"";
        for (char c : name) {
            if (c == '"') {
                out += '"';
            }
            out += c;
        }
        out += '"';
        return out;
    }

    static CellValue readCell(sqlite3_stmt* stmt, int col) {
        CellValue cell;
        const int type = sqlite3_column_type(stmt, col);
        if (type == SQLITE_NULL) {
            cell.isNull = true;
        } else if (type == SQLITE_BLOB) {
            cell.isNull = false;
            cell.text = "[BLOB " + std::to_string(sqlite3_column_bytes(stmt, col)) + " B]";
        } else {
            cell.isNull = false;
            const unsigned char* text = sqlite3_column_text(stmt, col);
            cell.text = text ? reinterpret_cast<const char*>(text) : "";
        }
        return cell;
    }

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};

} // namespace heibu
