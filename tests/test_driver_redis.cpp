// 黑簿 — RedisDriver 读写验证：连本地 redis，写/读/改 TTL。redis 不可用时优雅 SKIP。
import heibu.db.driver_redis;
import heibu.db.types;
import std;

using namespace heibu;

int main() {
    RedisDriver drv;
    DbConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 6379;
    std::string err;
    if (!drv.open(cfg, err)) {
        std::println("SKIP: {}", err);
        return 0;
    }
    const std::string key = "heibu:test:key";

    // 写 string + TTL
    RedisValue v;
    v.type = "string";
    v.entries.push_back({"", "hello 你好"});
    if (!drv.redisSave("db0", key, v, {}, true, err)) {
        std::println("FAIL save: {}", err);
        return 1;
    }
    // 读回
    RedisValue out;
    if (!drv.redisValue("db0", key, out, err)) {
        std::println("FAIL read: {}", err);
        return 1;
    }
    std::println("read type={} value={}", out.type,
                 out.entries.empty() ? "(none)" : out.entries[0].value);
    if (out.type != "string" || out.entries.empty() || out.entries[0].value != "hello 你好") {
        std::println("FAIL: value mismatch");
        return 1;
    }
    // hash 写/读（独立键，避免类型冲突）
    const std::string hkey = "heibu:test:hash";
    RedisValue h;
    h.type = "hash";
    h.entries = {{"a", "1"}, {"b", "2"}};
    if (!drv.redisSave("db0", hkey, h, {}, false, err)) {
        std::println("FAIL hash save: {}", err);
        return 1;
    }
    RedisValue hout;
    if (!drv.redisValue("db0", hkey, hout, err)) {
        std::println("FAIL hash read: {}", err);
        return 1;
    }
    std::println("hash fields={}", hout.entries.size());
    if (hout.type != "hash" || hout.entries.size() != 2) {
        std::println("FAIL: hash mismatch");
        return 1;
    }
    // 清理
    ResultGrid g;
    drv.run(BoundStatement("DEL " + key), g, 10, err);
    drv.run(BoundStatement("DEL " + hkey), g, 10, err);
    std::println("redis driver: ok");
    return 0;
}
