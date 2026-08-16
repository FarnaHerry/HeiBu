#pragma once
// 黑簿 i18n — 中文打底，英文预留。字符串统一走 tr(StrId, Locale)，界面代码不硬编码文案。
#include <string>
#include <string_view>

namespace heibu {

enum class Locale { Zh, En };

enum class StrId {
    AppTitle = 0,
    Ready,
    Connected,
    ReadOnly,
    Connections,
    Tables,
    NewConnection,
    CloseConnection,
    NewQuery,
    Run,
    Running,
    AffectedRows,   // 影响 {} 行
    Truncated,      // 已截断
    Rows,           // 行
    NoTables,
    Save,
    Cancel,
    Name,
    Path,
    Browse,
    Test,
    Delete,
    Edit,
    SetNull,
    AddRow,
    Committed,
    ConfirmDelete,
    ConfirmDeleteMsg,
    EmptyQuery,
    ReadOnlyQuery,
    RowIdMissing,
    Error,
    QueryError,
    NoConnection,
    Table,
    View,
    Settings,
    ThemeModeLabel,
    FollowSystem,
    ThemeDark,
    ThemeLight,
    NewTable,
    Refresh,
    DropTable,
    DropTableMsg,
    NewQueryTab,
    LanguageLabel,
    LanguageZh,
    LanguageEn,
    Host,
    Port,
    User,
    Password,
    Database,
    Type,
    Count,
};

struct StringEntry {
    const char* zh;
    const char* en;
};

inline constexpr StringEntry kStrings[] = {
    {"黑簿 Heibu", "Heibu"},
    {"就绪", "Ready"},
    {"已连接", "Connected"},
    {"只读", "Read-only"},
    {"命籍", "Fates"},
    {"卷宗", "Records"},
    {"新命籍", "New fate"},
    {"关闭命籍", "Close fate"},
    {"查卷", "New query"},
    {"批阅", "Run"},
    {"批阅中…", "Running…"},
    {"影响 {} 行", "{} row(s) affected"},
    {"已截断", "truncated"},
    {"行", "rows"},
    {"无卷宗", "No records"},
    {"保存", "Save"},
    {"取消", "Cancel"},
    {"名称", "Name"},
    {"路径", "Path"},
    {"浏览…", "Browse…"},
    {"测试连接", "Test"},
    {"删除", "Delete"},
    {"批改", "Edit"},
    {"置为 NULL", "Set NULL"},
    {"添一笔", "Add row"},
    {"已批改", "Committed"},
    {"勾销此名", "Strike this name"},
    {"将从此卷勾销该行，魂飞魄散，不可撤回。", "This row will be struck from the record. Irreversible."},
    {"请输入 SQL", "Enter SQL"},
    {"查卷结果不可批改", "Query results are read-only"},
    {"此卷无主键，只读", "No primary key; read-only"},
    {"出错", "Error"},
    {"查询出错", "Query error"},
    {"未连接", "Not connected"},
    {"表", "table"},
    {"视图", "view"},
    {"设置", "Settings"},
    {"主题模式", "Theme mode"},
    {"跟随系统", "Follow system"},
    {"深色", "Dark"},
    {"浅色", "Light"},
    {"新建表", "New table"},
    {"刷新", "Refresh"},
    {"删除表", "Drop table"},
    {"将删除该表，不可撤回。", "This table will be dropped. Irreversible."},
    {"新建查询", "New query"},
    {"语言", "Language"},
    {"中文", "中文"},
    {"English", "English"},
    {"主机", "Host"},
    {"端口", "Port"},
    {"用户", "User"},
    {"密码", "Password"},
    {"数据库", "Database"},
    {"类型", "Type"},
};

static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == static_cast<int>(StrId::Count),
              "StrId / kStrings 条目数不一致");

inline std::string_view tr(StrId id, Locale loc) {
    const StringEntry& e = kStrings[static_cast<int>(id)];
    return loc == Locale::En ? std::string_view(e.en) : std::string_view(e.zh);
}

// 把模板中第一个 "{}" 替换为 s（如 "影响 {} 行" -> "影响 42 行"）。
inline std::string fillFirst(std::string_view tmpl, const std::string& s) {
    std::string out;
    out.reserve(tmpl.size() + s.size());
    const std::size_t pos = tmpl.find("{}");
    if (pos == std::string_view::npos) {
        return std::string(tmpl);
    }
    out.append(tmpl.substr(0, pos));
    out.append(s);
    out.append(tmpl.substr(pos + 2));
    return out;
}

} // namespace heibu
