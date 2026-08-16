// 黑簿 连接配置持久化 — JSON 存 %APPDATA%\Heibu\connections.json，用 yyjson 读写。
module;
#include <yyjson.h>

export module heibu.store.connections;
import std;
import heibu.db.types;

export namespace heibu {

struct ConnectionStore {
    static std::filesystem::path configPath() {
        const char* appdata = std::getenv("APPDATA");
        if (appdata && *appdata) {
            return std::filesystem::path(appdata) / "Heibu" / "connections.json";
        }
        return std::filesystem::path("connections.json");
    }

    // 显式路径版本（测试用临时文件）；下面 load/save 委托到 configPath()。
    static std::vector<ConnectionInfo> loadFrom(const std::filesystem::path& path, std::string& error) {
        std::vector<ConnectionInfo> out;
        error.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return out;   // 首次运行：空列表，不算错误
        }
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error = "无法读取 " + path.string();
            return out;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!parse(content, out, error)) {
            out.clear();
        }
        return out;
    }

    static bool saveTo(const std::filesystem::path& path,
                       const std::vector<ConnectionInfo>& connections, std::string& error) {
        error.clear();
        const std::string json = serialize(connections);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            error = "无法写入 " + path.string();
            return false;
        }
        out << json;
        return true;
    }

    static std::vector<ConnectionInfo> load(std::string& error) {
        return loadFrom(configPath(), error);
    }

    static bool save(const std::vector<ConnectionInfo>& connections, std::string& error) {
        return saveTo(configPath(), connections, error);
    }

private:
    static bool parse(const std::string& content, std::vector<ConnectionInfo>& out, std::string& error) {
        yyjson_doc* doc = yyjson_read(content.c_str(), content.size(), 0);
        if (!doc) {
            error = "JSON 解析失败";
            return false;
        }
        yyjson_val* root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            error = "根节点不是对象";
            yyjson_doc_free(doc);
            return false;
        }
        yyjson_val* arr = yyjson_obj_get(root, "connections");
        if (arr && yyjson_is_arr(arr)) {
            const std::size_t n = yyjson_arr_size(arr);
            for (std::size_t i = 0; i < n; ++i) {
                yyjson_val* item = yyjson_arr_get(arr, i);
                if (!yyjson_is_obj(item)) {
                    continue;
                }
                auto getStr = [&](const char* key) -> std::string {
                    yyjson_val* v = yyjson_obj_get(item, key);
                    return (v && yyjson_is_str(v)) ? std::string(yyjson_get_str(v)) : std::string();
                };
                ConnectionInfo ci;
                ci.id = getStr("id");
                ci.name = getStr("name");
                ci.path = getStr("path");
                ci.driver = getStr("driver");
                if (ci.driver.empty()) {
                    ci.driver = "sqlite";
                }
                ci.host = getStr("host");
                ci.user = getStr("user");
                ci.password = getStr("password");
                ci.database = getStr("database");
                yyjson_val* pv = yyjson_obj_get(item, "port");
                if (pv && yyjson_is_int(pv)) {
                    ci.port = static_cast<int>(yyjson_get_int(pv));
                }
                if (!ci.id.empty() || !ci.name.empty() || !ci.path.empty() || !ci.host.empty() ||
                    !ci.database.empty()) {
                    out.push_back(std::move(ci));
                }
            }
        }
        yyjson_doc_free(doc);
        return true;
    }

    static std::string serialize(const std::vector<ConnectionInfo>& connections) {
        yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* root = yyjson_mut_obj(doc);
        yyjson_mut_val* arr = yyjson_mut_arr(doc);
        for (const ConnectionInfo& c : connections) {
            yyjson_mut_val* obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, obj, "id", c.id.c_str());
            yyjson_mut_obj_add_str(doc, obj, "name", c.name.c_str());
            yyjson_mut_obj_add_str(doc, obj, "path", c.path.c_str());
            yyjson_mut_obj_add_str(doc, obj, "driver", c.driver.c_str());
            yyjson_mut_obj_add_str(doc, obj, "host", c.host.c_str());
            yyjson_mut_obj_add_str(doc, obj, "user", c.user.c_str());
            yyjson_mut_obj_add_str(doc, obj, "password", c.password.c_str());
            yyjson_mut_obj_add_str(doc, obj, "database", c.database.c_str());
            yyjson_mut_obj_add_int(doc, obj, "port", c.port);
            yyjson_mut_arr_append(arr, obj);
        }
        yyjson_mut_obj_add_val(doc, root, "connections", arr);
        yyjson_mut_doc_set_root(doc, root);
        std::size_t len = 0;
        const char* json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &len);
        std::string result(json ? std::string(json, len) : std::string("{}"));
        if (json) {
            std::free(const_cast<char*>(json));
        }
        yyjson_mut_doc_free(doc);
        return result;
    }
};

} // namespace heibu
