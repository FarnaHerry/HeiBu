#pragma once
// 黑簿 应用状态 — 单例 AppState 是全应用唯一的数据源。
// compose() 自顶向下读 S() 构建元素树；事件回调改 S() 后由 app::requestUpdate() 触发重排。
#include "db/driver.h"
#include "db/types.h"
#include "i18n/strings.h"
#include "os_theme.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace heibu {

enum class TabKind { Table, Query, Settings };

struct CellRef {
    std::int64_t row;
    int col;
};

// 侧边栏树中当前选中的行（点击谁高亮谁；与活动标签解耦）。
struct SidebarSelection {
    enum class Kind { None, Conn, Database, Category, Object };
    Kind kind = Kind::None;
    std::string connId;
    std::string database;
    std::string name;   // Object 行：对象名；Category 行：分类类型名
    std::string type;   // Object/Category 行：类型
};

struct Tab {
    std::string id;      // 稳定 id；所有元素 id 由此派生
    TabKind kind = TabKind::Query;
    std::string connectionId;
    std::string database;    // Table 类标签专用：所在数据库/模式
    std::string tableName;   // Table 类标签专用
    std::string title;

    std::string sqlText;     // Query 类标签：编辑器缓冲

    std::optional<ResultGrid> result;
    bool running = false;
    std::string lastError;

    std::int64_t page = 0;        // 表浏览分页：0-based 当前页
    std::int64_t pageSize = 100;  // 每页行数
    std::int64_t totalRows = -1;  // 总行数（-1 = 未知）
    std::string pageInput = "1";  // 手动跳页输入缓冲（1-based）

    float scrollY = 0.0f;    // virtualList 偏移
    float scrollX = 0.0f;    // 横向偏移（自建）
    std::optional<CellRef> selected;
    std::optional<CellRef> editing;
    std::string editBuffer;
    bool editIsNull = false;
    bool addingRow = false;   // 正在追加草稿行（最后一行）
    bool dirty = false;       // 有未提交改动（事务已开）：编辑/删除/添一笔共用的修改状态
};

// 新建表对话框里的一个字段（列）。
struct ColumnField {
    std::string name;
    std::string type;   // 类型可带约束，如 "INTEGER PRIMARY KEY"
};

struct AppState {
    std::vector<ConnectionInfo> connections;   // 持久化顺序
    std::optional<std::string> activeConnectionId;
    SidebarSelection sidebarSel;               // 侧边栏选中行（点击即高亮）
    std::unordered_map<std::string, std::shared_ptr<IDbDriver>> sessions;   // connectionId -> 已打开连接
    std::unordered_map<std::string, std::vector<std::string>> databaseLists; // connectionId -> 数据库名列表
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<TableInfo>>> tableLists; // connectionId -> database -> 表列表
    std::set<std::string> expandedConnections;                               // 展开的连接 id
    std::set<std::string> expandedDatabases;                                 // 展开的数据库（connId + "\n" + database）
    std::set<std::string> expandedCategories;                                // 展开的对象分类（connId + "\n" + database + "\n" + type）

    std::vector<std::string> openTabIds;       // 标签条顺序
    std::unordered_map<std::string, Tab> tabs;
    std::optional<std::string> activeTabId;

    bool darkTheme = true;                     // 生效的深色布尔（System 时跟随 OS）
    ThemeMode themeMode = ThemeMode::System;   // 主题模式
    Locale locale = Locale::Zh;
    std::string statusMessage;                 // 瞬时提示
    float sidebarScrollY = 0.0f;               // 侧边栏卷宗列表滚动偏移
    float tabScrollX = 0.0f;                   // 标签条横向滚动偏移

    // 侧边栏右键菜单
    bool ctxMenuOpen = false;
    float ctxX = 0.0f;
    float ctxY = 0.0f;
    std::string ctxConnId;      // 右键命中的连接
    std::string ctxDatabase;    // 右键命中的数据库（空 = 连接级）
    std::string ctxTableName;   // 右键命中的表（空 = 非表级）

    // 标签右键菜单
    bool tabCtxOpen = false;
    float tabCtxX = 0.0f;
    float tabCtxY = 0.0f;
    std::string tabCtxId;       // 右键命中的标签 id

    // 分页大小下拉菜单
    bool pageSizeMenuOpen = false;
    std::string pageSizeMenuTabId;
    float pageSizeMenuX = 0.0f;
    float pageSizeMenuY = 0.0f;

    // 新建表 / 删除表对话框
    bool showCreateTable = false;
    std::string newTableName;
    std::vector<ColumnField> newTableColumns;   // 新建表的字段列表
    bool showDropTable = false;
    std::string dropTableName;

    // 连接对话框
    bool showConnDialog = false;
    int connStep = 0;          // 0 = 选类型，1 = 填信息
    std::string connDriver = "sqlite";
    std::string connName;
    std::string connPath;      // sqlite 路径
    std::string connHost, connPort, connUser, connPassword, connDatabase;   // 服务器库
    std::string connError;

    // 勾销（删除行）确认框
    bool showDeleteConfirm = false;
};

inline AppState& S() {
    static AppState s;
    return s;
}

inline std::string_view L(StrId id) {
    return tr(id, S().locale);
}

// 设置主题模式并同步生效的深色值。
inline void setThemeMode(ThemeMode mode) {
    S().themeMode = mode;
    S().darkTheme = effectiveDark(mode);
}

// 跟随系统时，每次 compose 前同步 OS 深色（轮询线程唤醒后触发重排再读）。
inline void syncTheme() {
    if (S().themeMode == ThemeMode::System) {
        S().darkTheme = osDark();
    }
}

// 生成唯一 id（时间戳 + 计数，会话内唯一）。
inline std::string newId() {
    static std::uint64_t counter = 0;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
    return std::to_string(ms) + "-" + std::to_string(++counter);
}

} // namespace heibu
