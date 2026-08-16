// 黑簿 — 连接配置 JSON 往返（含服务器库字段）、缺文件、坏文件、CJK 名称。
import heibu.store.connections;
import heibu.db.types;

#include <filesystem>
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

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "heibu_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    const std::filesystem::path file = dir / "connections.json";

    // 缺文件 -> 空列表，无错误
    {
        std::string err = "sentinel";
        auto list = ConnectionStore::loadFrom(file, err);
        check(list.empty(), "missing file -> empty");
        check(err.empty(), "missing file -> no error");
    }

    // 保存 + 读取往返一致（sqlite + 服务器库，CJK、反斜杠、空串）
    {
        std::vector<ConnectionInfo> in;
        ConnectionInfo a;
        a.id = "c1";
        a.name = "本地账册";
        a.path = "C:\\data\\账册.db";
        a.driver = "sqlite";
        ConnectionInfo b;
        b.id = "c2";
        b.name = "";
        b.path = ":memory:";
        b.driver = "sqlite";
        ConnectionInfo c;
        c.id = "c3";
        c.name = "阴司备份";
        c.driver = "mysql";
        c.host = "127.0.0.1";
        c.port = 3306;
        c.user = "root";
        c.password = "secret";
        c.database = "heibu";
        in.push_back(a);
        in.push_back(b);
        in.push_back(c);

        std::string err;
        check(ConnectionStore::saveTo(file, in, err), "save: " + err);

        auto out = ConnectionStore::loadFrom(file, err);
        check(err.empty(), "load no error: " + err);
        check(out.size() == 3, "3 connections round-trip");
        if (out.size() == 3) {
            check(out[0].id == "c1" && out[0].name == "本地账册", "CJK name survives");
            check(out[0].path == "C:\\data\\账册.db", "backslash path survives");
            check(out[0].driver == "sqlite", "sqlite driver survives");
            check(out[1].name.empty() && out[1].path == ":memory:", "empty name + memory path");

            check(out[2].driver == "mysql", "mysql driver survives");
            check(out[2].host == "127.0.0.1", "host survives");
            check(out[2].port == 3306, "port survives");
            check(out[2].user == "root", "user survives");
            check(out[2].password == "secret", "password survives");
            check(out[2].database == "heibu", "database survives");
            check(out[2].path.empty(), "server conn has empty path");
        }
    }

    // 坏文件 -> 报错不崩溃，返回空
    {
        std::ofstream bad(file, std::ios::binary);
        bad << "this is not json {{";
        bad.close();
        std::string err;
        auto out = ConnectionStore::loadFrom(file, err);
        check(out.empty(), "corrupt -> empty");
        check(!err.empty(), "corrupt -> error set");
    }

    std::filesystem::remove_all(dir, ec);

    if (failures == 0) {
        std::println("test_connections: ok");
        return 0;
    }
    std::println("test_connections: {} failure(s)", failures);
    return 1;
}
