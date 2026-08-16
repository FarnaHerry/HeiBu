// 黑簿 数据模型 — 纯数据结构，无任何依赖，可无头测试。
export module heibu.db.types;
import std;

export namespace heibu {

// 隐藏键列的保留别名：SQLite 无声明主键时，驱动 SELECT rowid AS "__heibu_key__" 带出，
// 由 postProcessGrid 剥离进 ResultGrid.keys。
inline constexpr const char* kHiddenKeyColumn = "__heibu_key__";

// 参数化语句：sql 的占位符风格由方言决定（SQLite ?N / MySQL ? / PostgreSQL $N），
// params[i] 按出现顺序对应第 i+1 个占位符；nullopt 表示 NULL。
struct BoundStatement {
    std::string sql;
    std::vector<std::optional<std::string>> params;

    BoundStatement() = default;
    BoundStatement(std::string s) : sql(std::move(s)) {}   // 无参数语句
    BoundStatement(const char* s) : sql(s ? s : "") {}      // 字面量便捷构造
};

// 一个网格单元格：文本 + 是否 NULL。
struct CellValue {
    bool isNull = true;
    std::string text;   // 显示/编辑形态，UTF-8
};

struct Column {
    std::string name;
    std::string declType;   // 声明的类型（可能为空）
    float width = 120.0f;   // px，用户可拖宽
};

// 一行数据的键值（主键列或 rowid 隐式键）：values 与键列一一对应；空 => 无键（只读）。
struct RowKey {
    std::vector<CellValue> values;
};

struct ResultGrid {
    std::vector<Column> columns;
    std::vector<std::vector<CellValue>> rows;   // 全量物化
    std::vector<std::string> keyColumns;        // 键列名（主键或 "rowid"）
    std::vector<RowKey> keys;                   // 与 rows 平行；空 => 只读
    bool editable = false;
    bool truncated = false;                     // 命中行上限
    std::int64_t affectedRows = -1;             // 非 SELECT 语句的影响行数
    double elapsedMs = 0.0;
};

// 连接参数：open 的入参，由 ConnectionInfo 拷贝而来。
struct DbConfig {
    std::string driver = "sqlite";
    std::string path;      // sqlite 文件路径（":memory:" 亦可）
    std::string host, user, password, database;   // 服务器库连接参数
    int port = 0;
};

struct ConnectionInfo {
    std::string id;        // 跨会话稳定
    std::string name;      // 用户标签
    std::string driver = "sqlite";
    std::string path;      // sqlite 文件路径
    std::string host, user, password, database;   // 服务器库连接参数
    int port = 0;
};

struct TableInfo {
    std::string name;
    std::string type;      // "table" | "view" | "trigger"
    std::string schema;    // PG schema；SQLite/MySQL 空
    std::string parent;    // trigger 专用：所属表名
};

// 表的键（主键）：columns 是键列名；hidden=true 表示键值来自隐藏列（SQLite rowid），
// SELECT 需显式带出（buildSelect 的 hidden 参数）。
struct TableKey {
    std::vector<std::string> columns;
    bool hidden = false;
};

// Redis 值项：hash=字段→值；list=索引→元素；zset=成员→分值；string/set 的 name 为空。
struct RedisEntry {
    std::string name;
    std::string value;
};

// Redis 键值 + TTL：type 为 string/hash/list/set/zset；ttl 秒数（-1=无过期，-2=键不存在）。
struct RedisValue {
    std::string type;
    std::int64_t ttl = -1;
    std::vector<RedisEntry> entries;
};

} // namespace heibu
