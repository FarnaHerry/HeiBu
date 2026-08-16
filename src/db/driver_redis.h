#pragma once
// 黑簿 Redis 驱动 — 包 hiredis（compat.hiredis 1.2.0），把键值库映射进树：
//   连接 → db0..db15 → 键类型（string/hash/list/set/zset）→ 键。
// 键值只读展示（GET/HGETALL/LRANGE/SMEMBERS/ZRANGE）；查询标签里可直接跑任意 Redis 命令。
#include "db/driver.h"

// hiredis.h 只前置声明了 struct timeval，完整定义来自平台头；先引入再包 hiredis。
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif
#include <hiredis/hiredis.h>

#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace heibu {

class RedisDriver : public IDbDriver {
public:
    RedisDriver() = default;
    ~RedisDriver() override { close(); }

    RedisDriver(const RedisDriver&) = delete;
    RedisDriver& operator=(const RedisDriver&) = delete;

    bool open(const DbConfig& cfg, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
        const char* host = cfg.host.empty() ? "127.0.0.1" : cfg.host.c_str();
        const int port = cfg.port != 0 ? cfg.port : 6379;
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        ctx_ = redisConnectWithTimeout(host, port, tv);
        if (!ctx_ || ctx_->err) {
            error = ctx_ ? ctx_->errstr : "redis 连接失败";
            if (ctx_) {
                redisFree(ctx_);
                ctx_ = nullptr;
            }
            return false;
        }
        redisSetTimeout(ctx_, tv);
        if (!cfg.password.empty()) {
            redisReply* r = commandArgv({"AUTH", cfg.password}, error);
            if (!r) {
                redisFree(ctx_);
                ctx_ = nullptr;
                return false;
            }
            freeReplyObject(r);
        }
        return true;
    }

    bool isOpen() const override { return ctx_ != nullptr; }

    Dialect dialect() const override { return Dialect::Redis; }

    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
    }

    bool listDatabases(std::vector<std::string>& out, std::string&) override {
        out.clear();
        for (int i = 0; i < 16; ++i) {
            out.push_back("db" + std::to_string(i));
        }
        return true;
    }

    bool listTables(const std::string& database, std::vector<TableInfo>& out,
                    std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ctx_) {
            error = "not connected";
            return false;
        }
        if (!selectDb(parseDb(database), error)) {
            return false;
        }
        out.clear();
        std::vector<std::string> keys = scanAllKeys(error);
        if (!error.empty()) {
            return false;
        }
        for (const std::string& key : keys) {
            std::string type = keyType(key);
            if (type.empty()) {
                continue;
            }
            TableInfo t;
            t.name = key;
            t.type = type;
            out.push_back(std::move(t));
        }
        return true;
    }

    bool listTriggers(const std::string&, std::vector<TableInfo>& out, std::string&) override {
        out.clear();
        return true;
    }

    bool listRoutines(const std::string&, std::vector<TableInfo>& out, std::string&) override {
        out.clear();
        return true;
    }

    // Redis「对象定义」即键值：按 TYPE 自动判定类型并格式化。
    bool objectDdl(const std::string& database, const std::string& name, const std::string&,
                   std::string& ddl, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ctx_) {
            error = "not connected";
            return false;
        }
        if (!selectDb(parseDb(database), error)) {
            return false;
        }
        std::string type = keyType(name);
        if (type.empty() || type == "none") {
            error = "键不存在: " + name;
            return false;
        }
        std::string v;
        if (!keyValueAsText(name, type, v, error)) {
            return false;
        }
        ddl = std::move(v);
        return true;
    }

    // 键值 → 网格：按类型物化（string 单行、hash/zset 两列、list 带索引、set 单列）。
    bool keyValue(const std::string& database, const std::string& key, const std::string& type,
                  ResultGrid& out, std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ctx_) {
            error = "not connected";
            return false;
        }
        if (!selectDb(parseDb(database), error)) {
            return false;
        }
        const std::string t = type.empty() ? keyType(key) : type;
        if (t.empty() || t == "none") {
            error = "键不存在: " + key;
            return false;
        }
        std::vector<std::string> args;
        if (t == "string") {
            args = {"GET", key};
        } else if (t == "hash") {
            args = {"HGETALL", key};
        } else if (t == "list") {
            args = {"LRANGE", key, "0", "-1"};
        } else if (t == "set") {
            args = {"SMEMBERS", key};
        } else if (t == "zset") {
            args = {"ZRANGE", key, "0", "-1", "WITHSCORES"};
        } else {
            error = "暂不支持的键类型: " + t;
            return false;
        }
        redisReply* r = commandArgv(args, error);
        if (!r) {
            return false;
        }
        out = ResultGrid{};
        if (t == "string") {
            out.columns = {Column{"键", "", 200.0f}, Column{"值", "", 480.0f}};
            out.rows.push_back({CellValue{false, key}, cellOf(r)});
        } else if (t == "hash") {
            out.columns = {Column{"字段", "", 200.0f}, Column{"值", "", 480.0f}};
            for (std::size_t i = 0; i + 1 < r->elements; i += 2) {
                out.rows.push_back({cellOf(r->element[i]), cellOf(r->element[i + 1])});
            }
        } else if (t == "zset") {
            out.columns = {Column{"成员", "", 240.0f}, Column{"分值", "", 200.0f}};
            for (std::size_t i = 0; i + 1 < r->elements; i += 2) {
                out.rows.push_back({cellOf(r->element[i]), cellOf(r->element[i + 1])});
            }
        } else if (t == "list") {
            out.columns = {Column{"索引", "", 80.0f}, Column{"值", "", 480.0f}};
            for (std::size_t i = 0; i < r->elements; ++i) {
                out.rows.push_back({CellValue{false, std::to_string(i)}, cellOf(r->element[i])});
            }
        } else {   // set
            out.columns = {Column{"成员", "", 480.0f}};
            for (std::size_t i = 0; i < r->elements; ++i) {
                out.rows.push_back({cellOf(r->element[i])});
            }
        }
        out.affectedRows = -1;
        freeReplyObject(r);
        return true;
    }

    bool tableKey(const TableInfo&, TableKey& key, std::string&) override {
        key = TableKey{};   // 键值只读展示，不参与行编辑
        return true;
    }

    bool run(const BoundStatement& stmt, ResultGrid& out, std::int64_t rowCap,
             std::string& error, CancelCheck) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ctx_) {
            error = "not connected";
            return false;
        }
        out = ResultGrid{};
        std::vector<std::string> args = splitArgs(stmt.sql);
        if (args.empty()) {
            out.affectedRows = 0;
            return true;
        }
        redisReply* r = commandArgv(args, error);
        if (!r) {
            return false;
        }
        out.columns.push_back(Column{"value", "", 240.0f});
        if (r->type == REDIS_REPLY_ARRAY) {
            std::int64_t n = 0;
            for (std::size_t i = 0; i < r->elements; ++i) {
                if (n >= rowCap) {
                    out.truncated = true;
                    break;
                }
                out.rows.push_back({cellOf(r->element[i])});
                ++n;
            }
            out.affectedRows = -1;
        } else if (r->type == REDIS_REPLY_NIL) {
            out.affectedRows = 0;
        } else {
            out.rows.push_back({cellOf(r)});
            out.affectedRows = -1;
        }
        freeReplyObject(r);
        return true;
    }

    std::string engineVersion() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ctx_) {
            return "Redis";
        }
        redisReply* r = static_cast<redisReply*>(redisCommand(ctx_, "INFO server"));
        std::string ver = "Redis";
        if (r && r->type == REDIS_REPLY_STRING && r->str) {
            std::string info(r->str, r->len);
            const std::size_t p = info.find("redis_version:");
            if (p != std::string::npos) {
                const std::size_t e = info.find("\r", p);
                const std::size_t len = (e == std::string::npos) ? std::string::npos : (e - p - 14);
                ver = "Redis " + info.substr(p + 14, len);
            }
        }
        if (r) {
            freeReplyObject(r);
        }
        return ver;
    }

private:
    static constexpr std::size_t kMaxKeys = 2000;   // 键列表上限，避免大库卡死 UI

    static int parseDb(const std::string& database) {
        if (database.rfind("db", 0) == 0) {
            try {
                return std::stoi(database.substr(2));
            } catch (...) {
                return 0;
            }
        }
        try {
            return std::stoi(database);
        } catch (...) {
            return 0;
        }
    }

    static std::vector<std::string> splitArgs(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        for (char ch : s) {
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                if (!cur.empty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += ch;
            }
        }
        if (!cur.empty()) {
            out.push_back(cur);
        }
        return out;
    }

    static std::string replyText(const redisReply* r) {
        if (!r) {
            return "";
        }
        if (r->type == REDIS_REPLY_NIL) {
            return "(nil)";
        }
        if (r->type == REDIS_REPLY_INTEGER) {
            return std::to_string(r->integer);
        }
        return r->str ? std::string(r->str, r->len) : std::string();
    }

    static CellValue cellOf(const redisReply* r) {
        CellValue c;
        if (!r || r->type == REDIS_REPLY_NIL) {
            c.isNull = true;
        } else if (r->type == REDIS_REPLY_INTEGER) {
            c.isNull = false;
            c.text = std::to_string(r->integer);
        } else {
            c.isNull = false;
            c.text = r->str ? std::string(r->str, r->len) : std::string();
        }
        return c;
    }

    // 执行 argv 命令；失败填 error 并返回 nullptr。调用方须已持锁。
    redisReply* commandArgv(const std::vector<std::string>& args, std::string& error) {
        std::vector<const char*> argv(args.size());
        std::vector<std::size_t> lens(args.size());
        for (std::size_t i = 0; i < args.size(); ++i) {
            argv[i] = args[i].data();
            lens[i] = args[i].size();
        }
        redisReply* r = static_cast<redisReply*>(
            redisCommandArgv(ctx_, static_cast<int>(args.size()), argv.data(), lens.data()));
        if (!r) {
            error = ctx_ ? ctx_->errstr : "redis 命令失败";
            return nullptr;
        }
        if (r->type == REDIS_REPLY_ERROR) {
            error = r->str ? std::string(r->str, r->len) : "redis 错误";
            freeReplyObject(r);
            return nullptr;
        }
        return r;
    }

    bool selectDb(int db, std::string& error) {
        redisReply* r = commandArgv({"SELECT", std::to_string(db)}, error);
        if (!r) {
            return false;
        }
        freeReplyObject(r);
        return true;
    }

    std::string keyType(const std::string& key) {
        std::string err;
        redisReply* r = commandArgv({"TYPE", key}, err);
        if (!r || r->type != REDIS_REPLY_STATUS) {
            if (r) {
                freeReplyObject(r);
            }
            return "";
        }
        std::string t(r->str ? r->str : "");
        freeReplyObject(r);
        return t;
    }

    std::vector<std::string> scanAllKeys(std::string& error) {
        std::vector<std::string> keys;
        long long cursor = 0;
        int iterations = 0;
        do {
            redisReply* r = commandArgv({"SCAN", std::to_string(cursor), "COUNT", "500"}, error);
            if (!r) {
                return keys;
            }
            if (r->type != REDIS_REPLY_ARRAY || r->elements < 2 ||
                r->element[0]->type != REDIS_REPLY_STRING) {
                freeReplyObject(r);
                error = "SCAN 返回异常";
                return keys;
            }
            cursor = std::strtoll(r->element[0]->str, nullptr, 10);
            redisReply* arr = r->element[1];
            if (arr && arr->type == REDIS_REPLY_ARRAY) {
                for (std::size_t i = 0; i < arr->elements && keys.size() < kMaxKeys; ++i) {
                    if (arr->element[i]->type == REDIS_REPLY_STRING && arr->element[i]->str) {
                        keys.emplace_back(arr->element[i]->str, arr->element[i]->len);
                    }
                }
            }
            freeReplyObject(r);
            if (keys.size() >= kMaxKeys || ++iterations > 200) {
                break;
            }
        } while (cursor != 0);
        return keys;
    }

    bool keyValueAsText(const std::string& key, const std::string& type, std::string& out,
                        std::string& error) {
        std::vector<std::string> args;
        if (type == "string") {
            args = {"GET", key};
        } else if (type == "hash") {
            args = {"HGETALL", key};
        } else if (type == "list") {
            args = {"LRANGE", key, "0", "-1"};
        } else if (type == "set") {
            args = {"SMEMBERS", key};
        } else if (type == "zset") {
            args = {"ZRANGE", key, "0", "-1", "WITHSCORES"};
        } else {
            error = "暂不支持的键类型: " + type;
            return false;
        }
        redisReply* r = commandArgv(args, error);
        if (!r) {
            return false;
        }
        out.clear();
        if (r->type == REDIS_REPLY_STRING || r->type == REDIS_REPLY_STATUS ||
            r->type == REDIS_REPLY_INTEGER) {
            out = replyText(r);
        } else if (r->type == REDIS_REPLY_NIL) {
            out = "(nil)";
        } else if (r->type == REDIS_REPLY_ARRAY) {
            std::string body;
            if (type == "hash") {
                for (std::size_t i = 0; i + 1 < r->elements; i += 2) {
                    body += replyText(r->element[i]) + " : " + replyText(r->element[i + 1]) + "\n";
                }
            } else if (type == "zset") {
                for (std::size_t i = 0; i + 1 < r->elements; i += 2) {
                    body += replyText(r->element[i]) + "  (" + replyText(r->element[i + 1]) + ")\n";
                }
            } else {
                for (std::size_t i = 0; i < r->elements; ++i) {
                    body += replyText(r->element[i]) + "\n";
                }
            }
            out = std::move(body);
        }
        freeReplyObject(r);
        return true;
    }

    redisContext* ctx_ = nullptr;
    mutable std::mutex mutex_;
};

} // namespace heibu
