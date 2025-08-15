#include "http_request.h"
#include <algorithm>
#include <sstream>

HttpRequest::HttpRequest() : method_(UNKNOWN) {}

void HttpRequest::addHeader(const std::string &key, const std::string &value) {
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                   ::tolower);
    headers_[lower_key] = value;
}

std::string HttpRequest::getMethodString() const {
    switch (method_) {
    case GET:
        return "GET";
    case POST:
        return "POST";
    case PUT:
        return "PUT";
    case DELETE:
        return "DELETE";
    case HEAD:
        return "HEAD";
    case OPTIONS:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

std::string HttpRequest::getHeader(const std::string &key) const {
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                   ::tolower);
    auto it = headers_.find(lower_key);
    return it != headers_.end() ? it->second : "";
}

void HttpRequest::addParam(const std::string &key, const std::string &value) {
    params_[key] = value;
}

std::string HttpRequest::getParam(const std::string &key) const {
    auto it = params_.find(key);
    return it != params_.end() ? it->second : "";
}

void HttpRequest::clear() {
    method_ = UNKNOWN;
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    params_.clear();
}

std::string HttpRequest::toString() const {
    std::ostringstream oss;
    oss << getMethodString() << " " << path_ << " " << version_ << "\r\n";

    for (const auto &header : headers_) {
        oss << header.first << ": " << header.second << "\r\n";
    }

    oss << "\r\n" << body_;
    return oss.str();
}

HttpRequest::Method HttpRequest::stringToMethod(const std::string &method) {
    if (method == "GET")
        return GET;
    if (method == "POST")
        return POST;
    if (method == "PUT")
        return PUT;
    if (method == "DELETE")
        return DELETE;
    if (method == "HEAD")
        return HEAD;
    if (method == "OPTIONS")
        return OPTIONS;
    return UNKNOWN;
}
