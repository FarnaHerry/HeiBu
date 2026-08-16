# PR 简报：compat.openssl 支持 Windows（源码构建，VC-WIN64A + nmake）

> 本文件是给「写 PR 的 agent」的完整简报。改动已在本机验证，直接照此整理 commit + PR body 即可。
> 涉及仓库：**mcpplibs/mcpp-index**（`pkgs/c/compat.openssl.lua`）

---

## 一、PR 标题建议

**compat.openssl: add Windows support (source build via VC-WIN64A + nmake)**

## 二、一句话概述

给 `compat.openssl` 加 Windows 支持：新增 windows xpm 行、windows ldflags、以及一个在 install() 钩子里从源码构建 OpenSSL 3.5.1（`perl Configure VC-WIN64A` + `nmake`）的 `_install_windows()` 实现，产出 MSVC ABI 静态库 `libssl.lib`/`libcrypto.lib`。

## 三、背景

- `compat.openssl` 此前只有 linux/macosx（perl Configure + GNU Make），Windows 被 `install()` 里的 `os.host() == "windows"` 硬拦。
- 卡点是 **`compat.libmysqlclient` 的 Windows 支持**：其 fork（`wellwei/libmysqlclient`）的 `mcpp.toml` 已写好 Windows 构建段（`win_timers.cc`、`-lws2_32` 等），唯一缺环就是 openssl 依赖在 Windows 不可用。openssl 通了，libmysqlclient 的 Windows 构建即可解锁。
- 本 PR **不含** libmysqlclient 的 windows xpm 行——那是下一个 PR（openssl 合并后）。

## 四、改动内容（`pkgs/c/compat.openssl.lua`，+192/−6）

### 1. `xpm` 加 `windows` 行
复用与 linux/macosx 相同的源码 tarball（同一 sha256），只是构建路径不同。

### 2. `mcpp` 块加 `windows` ldflags
```lua
windows = { ldflags = { "-Llib", "-llibssl", "-llibcrypto",
                        "-lws2_32", "-lcrypt32", "-ladvapi32", "-luser32" } },
```
- 命名是 `libssl.lib`/`libcrypto.lib`（MSVC 静态库）。**lld-link 的 `-lX` 找 `X.lib`**，所以 `-lssl` 会找 `ssl.lib` 而失败，必须 `-llibssl`。`-l:` 语法 lld-link 也不认。
- 静态 libcrypto 的系统依赖（winsock/crypt/registry/windowing）显式列出，同 linux 段 `-ldl/-lpthread` 的做法。

### 3. `install()` 路由 Windows 到 `_install_windows()`
不再 `os.host() == "windows"` 时直接报错。

### 4. 新增 `_install_windows()` + 三个帮手
- **`find_vcvars()`**：vswhere 定位 VS 安装路径，失败回退到已知路径（VS 18/2022 Community）。`os.rm` 用 `pcall` 包住（见坑 1）。
- **`resolve_perl_windows()` + `perl_usable_windows()`**：优先 Strawberry Perl 的常见位置（`C:\Strawberry`、scoop），再回退 PATH。模块检查**额外要求 `Locale::Maketext::Simple`**（见坑 2）。
- **`win_dirname()`**：xpkg 没有 `path.dirname`，自写（见坑 1）。
- **`_install_windows()`**：生成一个 `.bat`，在**单次 vcvars 环境**里跑完 `Configure + nmake + install_sw`，用 `os.exec("cmd /c <bat> > <log> 2>&1")` 驱动（见坑 3），最后验证 `libssl.lib`/`libcrypto.lib` 存在并写出 anchor TU。

构建配置：`no-shared no-dso no-tests no-apps no-engine no-asm`。

## 五、关键设计决策 & 给维护者的坑（重要）

这是本 PR 最有价值的部分——三件事都在真机实测确认：

### 坑 1：xpkg 钩子环境（Windows）缺两个 API
- **`os.rm` 不存在**：调用直接抛 `attempt to call a nil value`（会让整个 install() 崩溃）。必须 `pcall(os.rm, ...)` 或用 `os.tryrm`。
- **`path.dirname` 不存在**（`path.join` 有）。需自写：
  ```lua
  local function win_dirname(p)
      local s = tostring(p):gsub("[/\\]+$", "")
      return s:match("^(.*)[/\\][^/\\]+$") or s
  end
  ```

### 坑 2：MSYS/Windows perl 缺模块
- OpenSSL 的 Configure 额外需要 **`Locale::Maketext::Simple`**（经 Params::Check → IPC::Cmd）。Git-for-Windows 的 MSYS perl 没有它，会在 Configure 深处报 `Can't locate Locale/Maketext/Simple.pm in @INC`。
- 现有 `perl_usable()` 的模块检查列表（Config/FindBin/File::Path/…）**不包含**这个模块，会误判 MSYS perl 可用。Windows 分支的检查必须显式加 `-MLocale::Maketext::Simple`。
- 需 Strawberry Perl（scoop `perl` 清单或 `C:\Strawberry` 均可）。

### 坑 3：`os.exec("bash -c ...")` 在 Windows 钩子环境里静默不执行 ⚠️
- 原 descriptor 的 `run()` 帮手把所有命令包成 `bash -c '...'`，在 linux/macosx 正常。**在 Windows 钩子环境里，`os.exec("bash -c ...")` 返回 true 但命令根本没跑**（探针验证：目标文件没生成，vswhere 输出为空）。
- 因此 Windows 分支**不能复用 `run()`**，必须 `os.exec("cmd /c <bat> > <log> 2>&1")` 直接驱动。

### 坑 4：vcvars 在真机不崩（澄清旧注释）
- 原注释声称"running vcvars in ANY form takes the whole process chain down"。在 Windows 11 + VS 2022/18 真机上实测：`cmd /c "vcvars64.bat & set"`（含从 bash 调）**完全正常**，INCLUDE/LIB/PATH 都正确 dump。那个崩溃是 xlings 钩子环境特有（很可能就是坑 3 的 bash 失效），不是 Windows 的问题。
- PR body 里建议维护者更新这条过时的注释/ TODO 文档（`.agents/docs/2026-08-05-openssl-windows-todo.md`）。

## 六、验证证据（本机全链路）

1. **手工配方**：`perl Configure VC-WIN64A no-shared no-tests no-asm --prefix=... --libdir=lib` + `nmake` → `libcrypto.lib`(48MB) + `libssl.lib`(10MB)。
2. **clang 可链接**：mcpp LLVM 工具链的 `clang-cl` 链 `libssl.lib+libcrypto.lib+ws2_32+crypt32+advapi32+user32` → 最小测试 exe 输出 `OpenSSL 3.5.1`。
3. **mcpp 全链路**（从零）：`mcpp build` → 下载 50MB → install() 钩子内构建成功 → 编译 anchor → 链接 → exe 输出 `linked: OpenSSL 3.5.1 1 Jul 2025`。✅

## 七、已知限制 / 后续

- **`no-asm`**：Windows 默认关汇编（避免依赖 NASM）。纯 C 功能完整、性能略降。若以后有 `xim:nasm` 或等价 build dep，可再开。
- **路径带空格**：`cmd /c` 直接拼路径，标准布局（`C:\Users\<name>\...`）无空格。用户名带空格的极端情况需要给 bat 路径加引号，可后续补。
- **vswhere 输出**在钩子环境里实测为空（也是 bash 失效所致），故 find_vcvars 主要走已知路径回退；vswhere 是加分项不是依赖。
- **后续 PR**：`compat.libmysqlclient` 加 windows xpm 行并验证（其 mcpp.toml 的 Windows 构建段已就绪）。

## 八、供 agent 使用的材料

- 改动文件：`~/.mcpp/registry/data/mcpplibs/pkgs/c/compat.openssl.lua`（本机 mcpplibs 检出的工作副本，含全部改动）。
- 提交时把该文件的最新内容放入 PR；commit message 建议涵盖四、五两节要点。
- 如需复现验证：先 `scoop install perl`（Strawberry），再在任意 mcpp 项目里 `mcpp add compat.openssl@3.5.1` 后 `mcpp build`。
