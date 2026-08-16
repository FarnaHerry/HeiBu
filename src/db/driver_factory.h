#pragma once
// 黑簿 驱动工厂 — 全项目唯一按 driver 字符串分派的点。
#include "db/driver.h"
#include "db/driver_odbc.h"
#include "db/driver_redis.h"
#include "db/driver_sqlite.h"

#include <memory>
#include <string>

namespace heibu {

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
