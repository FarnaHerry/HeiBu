# 黑簿 Heibu

> 直翻阅、批改各家数据库的那支笔 —— 一个轻量级数据库管理桌面应用（mini-DBeaver）。

黑簿是一个开箱即用的多数据库 GUI 工具，用 **C++23 模块** + **eui-neo 0.5.6** 构建，通过 mcpp 包管理器管理依赖。支持 SQLite / MySQL / PostgreSQL / Redis，树状导航 + 表格批改 + SQL 查询。

## 特性

- **多数据库导航**：连接 → 数据库 → 对象分类（表/视图/存储过程/函数/触发器）→ 对象，四层树
  - SQLite：直连 `sqlite3`
  - MySQL / MariaDB / PostgreSQL：ODBC（nanodbc）
  - Redis：RESP（hiredis），键类型（字符串/哈希/列表/集合/有序集合）分类浏览
- **表格批改**：双击单元格就地编辑，支持置 NULL、添一笔、勾销，改动走事务（保存/取消）
- **分页浏览**：首页/上一页/跳页/下一页/末页 + 每页行数可调
- **查询页**：SQL 编辑器 + 结果网格，对象 DDL / Redis 键值查看
- **结果网格**：列宽拖动、横向滚动、超长省略号、列分隔线
- **标签管理**：溢出横向滚动、右键菜单（关闭左侧/右侧/其他/全部）
- **主题**：跟随系统 / 深色 / 浅色；中英双语

## 支持的数据库

| 数据库 | 连接方式 | 备注 |
|---|---|---|
| SQLite | 内联 `sqlite3` | 自包含，开箱即用 |
| MySQL / MariaDB | ODBC（nanodbc） | 需安装对应 ODBC 驱动 |
| PostgreSQL | ODBC（nanodbc） | 需安装对应 ODBC 驱动 |
| Redis | 内联 `hiredis` | 自包含，开箱即用 |

## 构建

需要 [mcpp](https://github.com/mcpp-community/mcpp)（C++23 包管理器）。

```sh
mcpp build     # 编译
mcpp run       # 运行
mcpp test      # 运行测试
```

## 截图

（待补充）

## 路线图

- [ ] MySQL / PostgreSQL 原生驱动（开箱即用，不依赖 ODBC 驱动）—— 依赖 mcpp `compat.openssl` 的 Windows 支持
- [ ] 数据导出 CSV
- [ ] 查询编辑器语法高亮
- [ ] 表数据排序 / 筛选
- [ ] SQL Server 等更多 ODBC 库

## 许可证

[MIT](./LICENSE) © FarnaHerry
