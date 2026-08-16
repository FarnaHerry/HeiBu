// 黑簿 结果导出序列化 — 纯函数：把 ResultGrid 序列化为 CSV / TSV / JSON / SQL INSERT。可无头测试。
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

// TSV：制表符分隔，带 UTF-8 BOM；字段内制表符/换行替换为空格（Excel 打开直观）。
inline std::string resultToTsv(const ResultGrid& grid) {
    const auto cell = [](const CellValue& v) -> std::string {
        if (v.isNull) {
            return std::string();
        }
        std::string s = v.text;
        for (char& ch : s) {
            if (ch == '\t' || ch == '\n' || ch == '\r') {
                ch = ' ';
            }
        }
        return s;
    };
    std::string out;
    out += "\xEF\xBB\xBF";
    for (std::size_t c = 0; c < grid.columns.size(); ++c) {
        if (c) {
            out += '\t';
        }
        out += cell(CellValue{false, grid.columns[c].name});
    }
    out += "\r\n";
    for (const std::vector<CellValue>& row : grid.rows) {
        for (std::size_t c = 0; c < grid.columns.size(); ++c) {
            if (c) {
                out += '\t';
            }
            out += c < row.size() ? cell(row[c]) : std::string();
        }
        out += "\r\n";
    }
    return out;
}

// JSON 字符串转义。
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out;
}

// JSON：数组套对象，键为列名，值为字符串（NULL → null）。
inline std::string resultToJson(const ResultGrid& grid) {
    std::string out = "[\n";
    for (std::size_t r = 0; r < grid.rows.size(); ++r) {
        out += "  {";
        for (std::size_t c = 0; c < grid.columns.size(); ++c) {
            if (c) {
                out += ", ";
            }
            out += "\"" + jsonEscape(grid.columns[c].name) + "\": ";
            const CellValue& cell = c < grid.rows[r].size() ? grid.rows[r][c] : CellValue{};
            out += cell.isNull ? "null" : ("\"" + jsonEscape(cell.text) + "\"");
        }
        out += r + 1 == grid.rows.size() ? "}\n" : "},\n";
    }
    out += "]\n";
    return out;
}

// SQL INSERT：INSERT INTO "table" ("c1","c2") VALUES ('v1','v2'); 单引号翻倍转义，NULL → NULL。
inline std::string resultToSqlInsert(const ResultGrid& grid, const std::string& tableName) {
    const std::string tbl = tableName.empty() ? "data" : tableName;
    std::string out;
    for (const std::vector<CellValue>& row : grid.rows) {
        out += "INSERT INTO \"" + tbl + "\" (";
        for (std::size_t c = 0; c < grid.columns.size(); ++c) {
            if (c) {
                out += ", ";
            }
            out += "\"" + grid.columns[c].name + "\"";
        }
        out += ") VALUES (";
        for (std::size_t c = 0; c < grid.columns.size(); ++c) {
            if (c) {
                out += ", ";
            }
            const CellValue& cell = c < row.size() ? row[c] : CellValue{};
            if (cell.isNull) {
                out += "NULL";
            } else {
                out += "'";
                for (char ch : cell.text) {
                    if (ch == '\'') {
                        out += "''";
                    } else {
                        out += ch;
                    }
                }
                out += "'";
            }
        }
        out += ");\n";
    }
    return out;
}

} // namespace heibu
