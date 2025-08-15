#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "http_request.h"
#include <string>

class HttpParser {
  public:
    enum ParseState { REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR };

  private:
    ParseState state_;
    HttpRequest request_;
    size_t content_length_;

    bool parseRequestLine(const std::string &line);  // 解析 HTTP 请求行。
    bool parseHeader(const std::string &line);       // 解析 HTTP 头部。
    void parseQueryString(const std::string &query); // 解析查询字符串。
    void parseFormData(const std::string &data);     // 解析表单数据。
    std::string urlDecode(const std::string &str);   // URL 解码。

  public:
    HttpParser();

    ParseState parse(const char *data,
                     size_t length); // 解析输入的原始 HTTP 请求数据。
    ParseState getState() const { return state_; }
    const HttpRequest &getRequest() const { return request_; }
    void reset();

    bool isComplete() const { return state_ == COMPLETE; }
    bool hasError() const { return state_ == ERROR; }
};

#endif