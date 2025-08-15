#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <map>
#include <string>

class HttpResponse {
  public:
    enum StatusCode {
        OK = 200,
        CREATED = 201,
        NO_CONTENT = 204,
        MOVED_PERMANENTLY = 301,
        FOUND = 302,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        SERVICE_UNAVAILABLE = 503
    };

  private:
    StatusCode status_code_;
    std::string version_;
    std::map<std::string, std::string> headers_;
    std::string body_;

  public:
    HttpResponse(StatusCode status = OK,
                 const std::string &version = "HTTP/1.1");

    void setStatusCode(StatusCode status) { status_code_ = status; }
    void setVersion(const std::string &version) { version_ = version; }
    void addHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &body);
    void setContentType(const std::string &content_type);

    StatusCode getStatusCode() const { return status_code_; }
    std::string getStatusText() const;
    const std::string &getVersion() const { return version_; }
    const std::map<std::string, std::string> &getHeaders() const {
        return headers_;
    }
    const std::string &getBody() const { return body_; }

    std::string toString() const;
    void clear();

    // 便捷方法
    static HttpResponse ok(const std::string &body = "",
                           const std::string &content_type = "text/html");
    static HttpResponse json(const std::string &json_body);
    static HttpResponse redirect(const std::string &location);
    static HttpResponse notFound(const std::string &message = "Not Found");
    static HttpResponse badRequest(const std::string &message = "Bad Request");
    static HttpResponse
    unauthorized(const std::string &message = "Unauthorized");
    static HttpResponse
    internalError(const std::string &message = "Internal Server Error");
};

#endif