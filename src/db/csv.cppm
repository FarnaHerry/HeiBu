// 黑簿 CSV 导出 — 纯函数：把 ResultGrid 序列化为 CSV 文本（UTF-8 BOM + 表头 + 数据行）。可无头测试。
export module heibu.db.csv;
import std;
import heibu.db.types;

export namespace heibu {

// CSV 字段转义：含逗号/引号/换行时用双引号包裹，内部双引号翻倍。
inline std::string csvField(const std::string& text, bool quoteAlways = false) {
    bool needQuote = quoteAlways;
    for (char ch : text) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            needQuote = true;
            break;
        }
    }
    if (!needQuote) {
        return text;
    }
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '"') {
            out += '"';
        }
        out += ch;
    }
    out += '"';
    return out;
}

// 结果网格 → CSV。表头恒加引号（防列名含逗号）；NULL 导出为空字段；空串不加引号。
// 输出以 UTF-8 BOM 开头，Excel 打开中文不乱码。
inline std::string resultToCsv(const ResultGrid& grid) {
    std::string out;
    out += "\xEF\xBB\xBF";   // UTF-8 BOM

    for (std::size_t c = 0; c < grid.columns.size(); ++c) {
        if (c) {
            out += ',';
        }
        out += csvField(grid.columns[c].name, true);
    }
    out += "\r\n";

    for (const std::vector<CellValue>& row : grid.rows) {
        for (std::size_t c = 0; c < grid.columns.size(); ++c) {
            if (c) {
                out += ',';
            }
            const CellValue& cell = c < row.size() ? row[c] : CellValue{};
            out += cell.isNull ? std::string() : csvField(cell.text, false);
        }
        out += "\r\n";
    }
    return out;
}

} // namespace heibu
