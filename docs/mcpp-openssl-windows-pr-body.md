## Summary

Adds Windows support to `compat.openssl` — the last major package in the index still
marked "windows deferred". OpenSSL is built from the same source tarball the other
platforms use, through Windows' only x64 configuration path:
`perl Configure VC-WIN64A` + `nmake`, producing MSVC-ABI static
`libssl.lib` / `libcrypto.lib`.

This is the missing link that unblocks `compat.libmysqlclient` on Windows — its
`mcpp.toml` already has the Windows build segment written (`win_timers.cc`,
`-lws2_32`, …); openssl was the one dependency it couldn't resolve. This PR
intentionally does **not** add libmysqlclient's windows xpm line; that's the follow-up.

## Changes

**`pkgs/c/compat.openssl.lua` (+192/−6)**

1. **`xpm`: add a `windows` entry** — same source tarball / sha256 as linux/macosx;
   only the build path differs.
2. **`mcpp`: add `windows` ldflags**

   ```lua
   windows = { ldflags = { "-Llib", "-llibssl", "-llibcrypto",
                           "-lws2_32", "-lcrypt32", "-ladvapi32", "-luser32" } },
   ```

   Naming matters: the MSVC static libs are `libssl.lib` / `libcrypto.lib`, and
   lld-link's `-lX` looks for `X.lib`, so `-lssl` would fail — they must be
   `-llibssl` / `-llibcrypto`. Static libcrypto's own system deps
   (winsock / crypt / registry / windowing) are listed explicitly, mirroring how the
   linux leg lists `-ldl` / `-lpthread`.
3. **`install()` routes Windows to `_install_windows()`** instead of erroring out.
4. **`_install_windows()` + helpers** — generates one `.bat` that runs the whole build
   inside a single `vcvars64` environment (Configure → nmake → install_sw), verifies
   `libssl.lib` / `libcrypto.lib`, and writes the anchor TU. Helpers:
   `find_vcvars()` (vswhere, then well-known VS paths),
   `resolve_perl_windows()` / `perl_usable_windows()` (Strawberry-first, with the
   extra module probe), `win_dirname()` (xpkg has no `path.dirname`).
   Build config: `no-shared no-dso no-tests no-apps no-engine no-asm`.

**`tests/examples/openssl` — enable the member on Windows**

The member now declares `compat.openssl = "3.5.1"` and `HAVE_OPENSSL=1` on
**all three** platforms, so `tls.cpp` compiles and runs its real
TLS-context + SHA-256 checks on Windows too, instead of a no-op `main()`.
Stale "linux + macOS only" comments in `mcpp.toml` and `tests/tls.cpp` are
updated to match.

## Pitfalls for maintainers (all found on real hardware)

These are why the Windows path looks the way it does:

1. **The xpkg hook env on Windows is missing `os.rm` and `path.dirname`.** Calling
   `os.rm` throws `attempt to call a nil value` and would take the whole `install()`
   down, so it's invoked through `pcall`. `path.join` exists but `path.dirname`
   doesn't — `win_dirname()` hand-rolls it.
2. **OpenSSL's Configure needs `Locale::Maketext::Simple`** (via Params::Check →
   IPC::Cmd). Git-for-Windows' MSYS perl doesn't ship it and dies deep inside
   Configure. The existing `perl_usable()` module check (Config/FindBin/File::Path/…)
   does **not** include this module and would wrongly accept MSYS perl, so the Windows
   probe explicitly requires `Locale::Maketext::Simple`. Strawberry Perl
   (scoop `perl` or `C:\Strawberry`) is required.
3. **`os.exec("bash -c …")` silently does nothing in the Windows hook env.** It
   returns `true` but the command never runs (probed directly: target file not
   produced, vswhere output empty). So the Windows branch cannot reuse the shared
   `run()` helper — it drives the build via `os.exec("cmd /c <bat> > <log> 2>&1")`
   instead.
4. **vcvars does NOT crash on real Windows — the old note is stale.** The prior
   comment claimed "running vcvars in ANY form takes the whole process chain down".
   Tested on Windows 11 + VS 2022/18: `cmd /c "vcvars64.bat & set"` runs fine (also
   invoked from bash) and dumps INCLUDE/LIB/PATH correctly. That crash was specific
   to the old xlings hook env (almost certainly the same `bash -c` no-op as #3), not
   a property of Windows.

→ Could a maintainer update the now-outdated note in
`.agents/docs/2026-08-05-openssl-windows-todo.md` (the "vcvars takes down the process
chain" claim) accordingly?

## Verification (this machine, full chain)

1. Manual recipe: `perl Configure VC-WIN64A no-shared no-tests no-asm --prefix=…
   --libdir=lib` + `nmake` → `libcrypto.lib` (48MB) + `libssl.lib` (10MB).
2. `clang-cl` links `libssl.lib + libcrypto.lib + ws2_32 + crypt32 + advapi32 +
   user32` → minimal test exe prints `OpenSSL 3.5.1`.
3. Full `mcpp build` from scratch: downloads the 50MB tarball → builds inside the
   `install()` hook → compiles the anchor → links → exe prints
   `linked: OpenSSL 3.5.1 1 Jul 2025`. ✅
4. `mcpp test -p openssl` on Windows (llvm/clang toolchain, MSVC-built
   `libssl.lib` + `libcrypto.lib`): `tls ... ok` — `1 passed; 0 failed`. The
   `tls` TU is compiled with `-DHAVE_OPENSSL=1`, so this is the real test
   (TLS context + SHA-256 digest), not the fallback `main()`. ✅

## Known limitations

- **`no-asm`**: Windows builds default to pure-C (no NASM dependency). Functionally
  identical, slightly slower. Can be revisited if a `xim:nasm`-style build dep ever
  lands.
- **Spaces in paths**: `cmd /c` splices paths directly; the standard layout
  (`C:\Users\<name>\…`) is space-free. Usernames with spaces would need quoting the
  bat path — can be patched later.
- **vswhere output is empty in the hook env** (same `bash -c` no-op as #3), so
  `find_vcvars` primarily uses the known-path fallback; vswhere is a bonus, not a
  dependency.

## Follow-up

- `compat.libmysqlclient`: add the windows xpm line and verify (its Windows build
  segment is already in place). Next PR, gated on this one.
