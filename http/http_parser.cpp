#include "http_parser.h"
#include <algorithm>
#include <sstream>

HttpParser::HttpParser() : state_(REQUEST_LINE), content_length_(0) {}

HttpParser::ParseState HttpParser::parse(const char *data, size_t length) {
    std::string input(data, length);
    std::istringstream iss(input);
    std::string line;

    while (std::getline(iss, line) && state_ != COMPLETE && state_ != ERROR) {
        // 移除行尾的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        switch (state_) {
        case REQUEST_LINE: {
            if (parseRequestLine(line)) {
                state_ = HEADERS;
            } else {
                state_ = ERROR;
            }
            break;
        }
        case HEADERS: {
            if (line.empty()) {
                // 空行，头部结束
                if (content_length_ > 0) {
                    state_ = BODY;
                } else {
                    state_ = COMPLETE;
                }
            } else {
                if (!parseHeader(line)) {
                    state_ = ERROR;
                }
            }
            break;
        }
        case BODY: {
            // 读取剩余的数据作为body
            std::string remaining_data;
            std::string temp_line;
            remaining_data += line;
            while (std::getline(iss, temp_line)) {
                remaining_data += "\n" + temp_line;
            }

            request_.setBody(remaining_data);

            // 如果是POST请求，解析表单数据
            if (request_.getMethod() == HttpRequest::POST) {
                std::string content_type = request_.getHeader("content-type");
                if (content_type.find("application/x-www-form-urlencoded") !=
                    std::string::npos) {
                    parseFormData(request_.getBody());
                }
            }

            state_ = COMPLETE;
            break;
        }
        default:
            break;
        }
    }

    return state_;
}

bool HttpParser::parseRequestLine(const std::string &line) {
    std::istringstream iss(line);
    std::string method, path, version;

    if (!(iss >> method >> path >> version)) {
        return false;
    }

    request_.setMethod(HttpRequest::stringToMethod(method));

    // 解析路径和查询字符串
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        std::string query_string = path.substr(query_pos + 1);
        path = path.substr(0, query_pos);
        parseQueryString(query_string);
    }

    request_.setPath(urlDecode(path));
    request_.setVersion(version);

    return true;
}

bool HttpParser::parseHeader(const std::string &line) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    // 去除前后空白
    key.erase(0, key.find_first_not_of(' '));
    key.erase(key.find_last_not_of(' ') + 1);
    value.erase(0, value.find_first_not_of(' '));
    value.erase(value.find_last_not_of(' ') + 1);

    request_.addHeader(key, value);

    // 检查Content-Length
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                   ::tolower);
    if (lower_key == "content-length") {
        content_length_ = std::stoul(value);
    }

    return true;
}

void HttpParser::parseQueryString(const std::string &query) {
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq_pos));
            std::string value = urlDecode(pair.substr(eq_pos + 1));
            request_.addParam(key, value);
        }
    }
}

void HttpParser::parseFormData(const std::string &data) {
    std::istringstream iss(data);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq_pos));
            std::string value = urlDecode(pair.substr(eq_pos + 1));
            request_.addParam(key, value);
        }
    }
}

std::string HttpParser::urlDecode(const std::string &str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
            result += ch;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

void HttpParser::reset() {
    state_ = REQUEST_LINE;
    request_.clear();
    content_length_ = 0;
}