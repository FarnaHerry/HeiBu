// 黑簿 CSV 导出测试 — 序列化纯函数。
import heibu.db.csv;
import heibu.db.types;

#include <cassert>
#include <string>
#include <vector>

using namespace heibu;

int main() {
    // 1) 简单字段
    {
        assert(csvField("plain") == "plain");
        assert(csvField("a,b") == "\"a,b\"");
        assert(csvField("say \"hi\"") == "\"say \"\"hi\"\"\"");
        assert(csvField("line\nbreak") == "\"line\nbreak\"");
        assert(csvField("a", true) == "\"a\"");
    }

    // 2) 空结果网格：只有 BOM + 表头
    {
        ResultGrid g;
        g.columns = {{"a", "", 0}, {"b", "", 0}};
        const std::string s = resultToCsv(g);
        assert(s.size() >= 3 && s[0] == '\xEF' && s[1] == '\xBB' && s[2] == '\xBF');
        assert(s.find("\"a\",\"b\"\r\n") != std::string::npos);
    }

    // 3) 带数据 + NULL + 逗号/引号转义
    {
        ResultGrid g;
        g.columns = {{"id", "", 0}, {"name", "", 0}, {"note", "", 0}};
        g.rows.push_back({CellValue{false, "1"}, CellValue{false, "张三"}, CellValue{false, "你好"}});
        g.rows.push_back({CellValue{false, "2"}, CellValue{true, ""}, CellValue{false, "a,b\"c"}});
        const std::string s = resultToCsv(g);
        // BOM 开头
        assert(s.size() >= 3 && s[0] == '\xEF');
        // 转义正确
        assert(s.find("\"a,b\"\"c\"") != std::string::npos);
        // 行分隔是 \r\n
        std::size_t crlf = 0;
        for (std::size_t i = 0; i + 1 < s.size(); ++i) {
            if (s[i] == '\r' && s[i + 1] == '\n') {
                ++crlf;
            }
        }
        assert(crlf == 3);   // 表头 + 2 数据行
    }

    return 0;
}
