#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <map>
#include <string>
#include <vector>

class HttpRequest {
  public:
    enum Method { GET, POST, PUT, DELETE, HEAD, OPTIONS, UNKNOWN };

  private:
    Method method_;
    std::string path_;
    std::string version_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    std::map<std::string, std::string> params_;

  public:
    HttpRequest();

    void setMethod(Method method) { method_ = method; }
    void setPath(const std::string &path) { path_ = path; }
    void setVersion(const std::string &version) { version_ = version; }
    void addHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &body) { body_ = body; }
    void addParam(const std::string &key, const std::string &value);

    Method getMethod() const { return method_; }
    std::string getMethodString() const;
    const std::string &getPath() const { return path_; }
    const std::string &getVersion() const { return version_; }
    const std::map<std::string, std::string> &getHeaders() const {
        return headers_;
    }
    std::string getHeader(const std::string &key) const;
    const std::string &getBody() const { return body_; }
    std::string getParam(const std::string &key) const;
    const std::map<std::string, std::string> &getParams() const {
        return params_;
    }

    void clear();
    std::string toString() const;

    static Method stringToMethod(const std::string &method);
};

#endif