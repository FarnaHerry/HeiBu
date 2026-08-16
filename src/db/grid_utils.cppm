// 黑簿 网格后处理 — 纯函数：剥离隐藏键列、按键列提取键值、置 editable。可无头测试。
export module heibu.db.grid_utils;
import std;
import heibu.db.types;

export namespace heibu {

// 1) 若首列是隐藏键列（SELECT rowid AS "__heibu_key__" 的产物），剥离进 keys 并擦除该列。
// 2) 否则若 keyColumns 非空，从可见列按名提取键值。
// 3) editable = !keys.empty()（有键即可编辑，无键只读）。
inline void postProcessGrid(ResultGrid& grid, const std::vector<std::string>& keyColumns) {
    grid.keyColumns = keyColumns;
    grid.keys.clear();

    const bool hasHidden = !grid.columns.empty() && grid.columns[0].name == kHiddenKeyColumn;
    if (hasHidden) {
        grid.keys.reserve(grid.rows.size());
        for (std::vector<CellValue>& row : grid.rows) {
            RowKey k;
            if (!row.empty()) {
                k.values.push_back(row[0]);   // 隐藏键值（含 NULL 也保留，主键一般非空）
            }
            row.erase(row.begin());
            grid.keys.push_back(std::move(k));
        }
        grid.columns.erase(grid.columns.begin());
    } else if (!keyColumns.empty()) {
        // 按名定位可见键列。
        std::vector<int> colIdx;
        colIdx.reserve(keyColumns.size());
        for (const std::string& kc : keyColumns) {
            int idx = -1;
            for (std::size_t c = 0; c < grid.columns.size(); ++c) {
                if (grid.columns[c].name == kc) {
                    idx = static_cast<int>(c);
                    break;
                }
            }
            colIdx.push_back(idx);
        }
        grid.keys.reserve(grid.rows.size());
        for (std::vector<CellValue>& row : grid.rows) {
            RowKey k;
            for (int idx : colIdx) {
                k.values.push_back(idx >= 0 && static_cast<std::size_t>(idx) < row.size()
                                       ? row[static_cast<std::size_t>(idx)]
                                       : CellValue{true, ""});
            }
            grid.keys.push_back(std::move(k));
        }
    }

    grid.editable = !grid.keys.empty();
}

} // namespace heibu
