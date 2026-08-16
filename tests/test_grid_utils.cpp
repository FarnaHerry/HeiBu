// 黑簿 — postProcessGrid 纯函数测试：隐藏键剥离、声明主键提取、复合键、无键只读。
import heibu.db.grid_utils;
import heibu.db.types;

import std;

namespace {
int failures = 0;

void check(bool ok, const std::string& msg) {
    if (!ok) {
        std::println("FAIL: {}", msg);
        ++failures;
    }
}
} // namespace

int main() {
    using namespace heibu;

    // 隐藏键剥离（SQLite rowid 回退）
    {
        ResultGrid g;
        Column c0;
        c0.name = kHiddenKeyColumn;
        Column c1;
        c1.name = "name";
        g.columns = {c0, c1};
        g.rows = {
            {CellValue{false, "1"}, CellValue{false, "张三"}},
            {CellValue{false, "2"}, CellValue{true, ""}},
        };
        g.truncated = true;   // 截断不应触发弹行

        postProcessGrid(g, {"rowid"});

        check(g.editable, "editable via hidden key");
        check(g.keyColumns.size() == 1 && g.keyColumns[0] == "rowid", "keyColumns recorded");
        check(g.columns.size() == 1 && g.columns[0].name == "name", "hidden column stripped");
        check(g.keys.size() == 2 && g.keys[0].values[0].text == "1" &&
                  g.keys[1].values[0].text == "2", "keys extracted from hidden column");
        check(g.rows[0][0].text == "张三" && g.rows[1][0].isNull, "remaining cells intact");
    }

    // 声明主键提取（可见列按名）
    {
        ResultGrid g;
        Column c0;
        c0.name = "id";
        Column c1;
        c1.name = "name";
        g.columns = {c0, c1};
        g.rows = {{CellValue{false, "7"}, CellValue{false, "张三"}}};

        postProcessGrid(g, {"id"});

        check(g.editable, "editable via declared PK");
        check(g.keyColumns.size() == 1 && g.keyColumns[0] == "id", "keyColumns = {id}");
        check(g.keys.size() == 1 && g.keys[0].values[0].text == "7", "key extracted by name");
        check(g.columns.size() == 2 && g.columns[0].name == "id", "no column stripped");
    }

    // 复合主键
    {
        ResultGrid g;
        Column c0;
        c0.name = "tenant";
        Column c1;
        c1.name = "seq";
        Column c2;
        c2.name = "val";
        g.columns = {c0, c1, c2};
        g.rows = {{CellValue{false, "A"}, CellValue{false, "1"}, CellValue{false, "x"}}};

        postProcessGrid(g, {"tenant", "seq"});
        check(g.keys.size() == 1 && g.keys[0].values.size() == 2 &&
                  g.keys[0].values[0].text == "A" && g.keys[0].values[1].text == "1",
              "composite key extracted in order");
    }

    // 无键：只读
    {
        ResultGrid g;
        Column c0;
        c0.name = "id";
        g.columns = {c0};
        g.rows = {{CellValue{false, "1"}}};

        postProcessGrid(g, {});
        check(!g.editable, "editable false with no key");
        check(g.keys.empty(), "keys empty");
        check(g.columns.size() == 1 && g.rows.size() == 1, "untouched");
    }

    if (failures == 0) {
        std::println("test_grid_utils: ok");
        return 0;
    }
    std::println("test_grid_utils: {} failure(s)", failures);
    return 1;
}
