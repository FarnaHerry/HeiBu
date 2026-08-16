module;
#include <nanodbc/nanodbc.h>

// 黑簿 ODBC 层 —— 把经典 C++14 的 nanodbc（compat.nanodbc 包）包成 C++23 模块，
// 只导出 IDbDriver 需要的面。DriverOdbc 通过 `import heibu.odbc;` 使用。
export module heibu.odbc;

export namespace heibu::odbc {
    using nanodbc::connection;
    using nanodbc::result;
    using nanodbc::statement;
    using nanodbc::catalog;
    using nanodbc::driver;
    using nanodbc::list_drivers;

    using nanodbc::database_error;
    using nanodbc::type_incompatible_error;
    using nanodbc::null_access_error;
    using nanodbc::index_range_error;
    using nanodbc::programming_error;
} // namespace heibu::odbc
