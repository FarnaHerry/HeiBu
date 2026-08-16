// 黑簿 驱动工厂 — 全项目唯一按 driver 字符串分派的点。
export module heibu.db.driver_factory;
import std;
import heibu.db.dialect;
import heibu.db.driver;
import heibu.db.driver_odbc;
import heibu.db.driver_redis;
import heibu.db.driver_sqlite;

export namespace heibu {

inline std::shared_ptr<IDbDriver> createDriver(const std::string& kind) {
    if (kind.empty() || kind == "sqlite") {
        return std::make_shared<SqliteDriver>();
    }
    // MySQL / PostgreSQL 等走 ODBC（DriverOdbc 按方言生成 SQL + 连接串）。
    const Dialect d = dialectFromDriver(kind);
    if (kind == "mysql" || kind == "mariadb" || kind == "postgres" || kind == "postgresql" ||
        kind == "pg") {
        return std::make_shared<DriverOdbc>(d);
    }
    if (kind == "redis") {
        return std::make_shared<RedisDriver>();
    }
    return nullptr;   // 未知驱动
}

} // namespace heibu
