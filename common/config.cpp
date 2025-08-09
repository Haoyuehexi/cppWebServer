#include "config.h"
#include "./util.h"
#include <algorithm>
#include <fstream>
#include <sstream>

static std::string extractString(const std::string &src,
                                 const std::string &key) {
    size_t pos = src.find("\"" + key + "\"");
    if (pos == std::string::npos)
        return "";
    pos = src.find(":", pos);
    if (pos == std::string::npos)
        return "";
    pos++;
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\"'))
        pos++;
    size_t end = pos;
    while (end < src.size() && src[end] != '\"' && src[end] != ',' &&
           src[end] != '}')
        end++;
    return src.substr(pos, end - pos);
}

static int extractInt(const std::string &src, const std::string &key) {
    return std::stoi(extractString(src, key));
}

static bool extractBool(const std::string &src, const std::string &key) {
    std::string val = Util::trim(extractString(src, key));
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    return (val == "true");
}

bool ConfigLoader::load(const std::string &path, Config &out) {
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string data = ss.str();

    // server
    out.server.port = extractInt(data, "port");
    out.server.host = extractString(data, "host");
    out.server.thread_pool_size = extractInt(data, "thread_pool_size");
    out.server.max_connections = extractInt(data, "max_connections");
    out.server.timeout_ms = extractInt(data, "timeout_ms");
    out.server.keep_alive = extractBool(data, "keep_alive");

    // logging
    out.logging.level = extractString(data, "level");
    out.logging.file = extractString(data, "file");
    out.logging.max_file_size_mb = extractInt(data, "max_file_size_mb");
    out.logging.enable_console = extractBool(data, "enable_console");

    // http
    out.http.document_root = extractString(data, "document_root");
    out.http.default_page = extractString(data, "default_page");
    out.http.max_request_size_kb = extractInt(data, "max_request_size_kb");
    out.http.enable_directory_listing =
        extractBool(data, "enable_directory_listing");

    // database
    out.database.enable = extractBool(data, "enable");
    out.database.host = extractString(data, "host");
    out.database.port = extractInt(data, "port");
    out.database.username = extractString(data, "username");
    out.database.password = extractString(data, "password");
    out.database.database = extractString(data, "database");
    out.database.connection_pool_size =
        extractInt(data, "connection_pool_size");

    return true;
}
