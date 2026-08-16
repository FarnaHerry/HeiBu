# 打包 nanodbc 进 mcpp 社区 — 自包含执行方案

> 给接手此任务的 agent 的完整指引。目标：把 nanodbc 打包成 mcpp 生态的 `compat.nanodbc` 包，提交到 `github.com/mcpplibs/mcpp-index`，让任意 mcpp 项目能 `[dependencies.compat] nanodbc = "2.14.0"` 使用。

## 一、背景与已核实的事实

- **nanodbc**：ODBC 的薄 C++ 封装，两个文件 —— `nanodbc/nanodbc.h`（头）+ `nanodbc/nanodbc.cpp`（实现）。C++14+，**MIT**，已冻结（末版 v2.14.0，约 2022），正适合一次性打包。
- **官方 tarball**：`https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz`
  - **sha256 = `56228372042B689BECCD96B0AC3476643EA85B3F57B3F23FB11CA4314E68B9A5`**（已实测，大小 707731 字节）
  - 解压根目录 `nanodbc-2.14.0/`，内含 `nanodbc/nanodbc.h` 与 `nanodbc/nanodbc.cpp`。
- **mcpp 索引（live）**：`~/.mcpp/registry/data/mcpplibs/`（即 `github.com/mcpplibs/mcpp-index` 的 git checkout）。
- **本地索引（先测后提）**：`~/.mcpp/registry/data/local-indexrepo/`，包放 `pkgs/c/compat.nanodbc.lua`（平铺）。
- **包形状**：**Shape A（源码编 lib）**——nanodbc 有可编译源码 `nanodbc.cpp`，不是纯头。
- **ODBC 系统链接库**（关键，各平台不同，用 `mcpp` 表内 per-platform `ldflags` 声明）：
  - Windows → `-lodbc32`
  - macOS → `-liodbc`
  - Linux → `-lodbc`（unixODBC）
- **权威模板**：
  - `~/.mcpp/registry/data/mcpplibs/docs/package-types.md`（Shape A 骨架 + 规则）
  - `~/.mcpp/registry/data/mcpplibs/pkgs/c/compat.sqlite3.lua`（Form B 内联 recipe 参考）
  - `~/.mcpp/registry/data/mcpplibs/pkgs/e/compat.eui-neo.lua`（per-platform `cflags/cxxflags/ldflags` 写法）

## 二、descriptor（最终形态）

文件路径：`pkgs/c/compat.nanodbc.lua`

```lua
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "nanodbc",
    description = "nanodbc — a small C++ wrapper for the native C ODBC API",
    licenses    = {"MIT"},
    repo        = "https://github.com/nanodbc/nanodbc",
    type        = "package",

    xpm = {
        linux   = { ["2.14.0"] = { url = { GLOBAL = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz",
                                            CN     = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz" },
                                    sha256 = "56228372042B689BECCD96B0AC3476643EA85B3F57B3F23FB11CA4314E68B9A5" } },
        macosx  = { ["2.14.0"] = { url = { GLOBAL = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz",
                                            CN     = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz" },
                                    sha256 = "56228372042B689BECCD96B0AC3476643EA85B3F57B3F23FB11CA4314E68B9A5" } },
        windows = { ["2.14.0"] = { url = { GLOBAL = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz",
                                            CN     = "https://github.com/nanodbc/nanodbc/archive/refs/tags/v2.14.0.tar.gz" },
                                    sha256 = "56228372042B689BECCD96B0AC3476643EA85B3F57B3F23FB11CA4314E68B9A5" } },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        include_dirs = { "*" },                 -- 暴露 <nanodbc/nanodbc.h>
        sources      = { "*/nanodbc/nanodbc.cpp" },
        targets      = { ["nanodbc"] = { kind = "lib" } },
        windows      = { ldflags = { "-lodbc32" } },
        macosx       = { ldflags = { "-liodbc" } },
        linux        = { ldflags = { "-lodbc" } },
        deps         = { },
    },
}
```

> **CN 镜像说明**：上面 CN 暂填与 GLOBAL 相同（GitHub）。正式 PR 时按 mcpplibs 规范换成 gitcode 镜像（`https://gitcode.com/mcpp-res/<slug>/releases/download/...`），由维护者补或按其要求填；本地测试用上面的即可。

## 三、执行步骤

### 第 1 步：本地索引验证（不碰上游）
1. 把 descriptor 写入 `~/.mcpp/registry/data/local-indexrepo/pkgs/c/compat.nanodbc.lua`。
2. 新建临时消费工程，`mcpp.toml` 写 `[dependencies.compat] nanodbc = "2.14.0"`；`src/main.cpp` 里 `#include <nanodbc/nanodbc.h>` 并真实连一个 ODBC 数据源（或至少 `nanodbc::catalog` 列 catalog），`mcpp build` 通过即证明编译 + 链接 odbc32 都 OK。

### 第 2 步：加测试例子
在 mcpp-index 仓库 `tests/examples/nanodbc/` 建最小消费工程（`mcpp.toml` + `src/main.cpp`，main 返回 `ok ? 0 : 1`，含真实断言）。参考已有 `tests/examples/libmysqlclient/` 的布局。

### 第 3 步：sha256 + CN 镜像 + lint
- sha256 已算好（见上）。
- CN 镜像 URL 补齐。
- 用索引仓库 pinned 的 mcpp 跑 descriptor lint / 校验（参照仓库 CONTRIBUTING 或 `.agents/` 文档）。

### 第 4 步：提 PR
fork `github.com/mcpplibs/mcpp-index` → 加 `pkgs/c/compat.nanodbc.lua` + `tests/examples/nanodbc/` → PR。PR 描述注明：MIT、已冻结、Shape A、ODBC 三平台 ldflags。

## 四、关键坑（摘自 package-types.md）

- **三平台 xpm 都必须声明**（linux/macosx/windows）；纯源码包三者可共用同一 url + sha256。
- `mcpp` 表内路径是**相对 verdir 的 GLOB**：开头 `*` 吸收 `<repo>-<tag>/` 包层；`*` 匹配单段、`**` 跨段。
- 身份 = `(namespace, name)`；文件放 `pkgs/<initial>/<namespace>.<name>.lua`（命中 mcpp 快路径）。
- 若需要 `generated_files` 生成 `.cppm`/config：**内容不能用 Lua 长括号 `[[ ]]`**（mcpp 段解析器不支持），用双引号 + `\n` 转义。
- ODBC 头（`sql.h`）在 Windows 来自 Windows SDK、Linux 来自 unixODBC dev、macOS 来自 iODBC —— 这些是**系统依赖**，不打包进 descriptor（`ldflags` 只负责链接）。
- nanodbc.cpp 自带 `#pragma comment(lib, "comsuppw.lib")` 等 MSVC pragma，Windows 下 clang 兼容；Linux/macOS 走 `#ifdef` 分支不受影响。

## 五、（可选）后续：Shape C 模块化

若社区想要 `import compat.nanodbc;` 的模块形式，可在 Shape A 之上叠加 Shape C（`generated_files` 里 synthesize 一个 `.cppm`：`module; #include <nanodbc/nanodbc.h> export module compat.nanodbc; export using …`），参照 `pkgs/n/nlohmann.json.lua`。但**首版建议先只交 Shape A**（最小、无争议、最易过审）。
