#include "http_response.h"
#include <sstream>

HttpResponse::HttpResponse(StatusCode status, const std::string &version)
    : status_code_(status), version_(version) {
    addHeader("Server", "SimpleHttpServer/1.0");
    addHeader("Connection", "close");
}

void HttpResponse::addHeader(const std::string &key, const std::string &value) {
    headers_[key] = value;
}

void HttpResponse::setBody(const std::string &body) {
    body_ = body;
    addHeader("Content-Length", std::to_string(body_.length()));
}

void HttpResponse::setContentType(const std::string &content_type) {
    addHeader("Content-Type", content_type);
}

std::string HttpResponse::getStatusText() const {
    switch (status_code_) {
    case OK:
        return "OK";
    case CREATED:
        return "Created";
    case NO_CONTENT:
        return "No Content";
    case MOVED_PERMANENTLY:
        return "Moved Permanently";
    case FOUND:
        return "Found";
    case BAD_REQUEST:
        return "Bad Request";
    case UNAUTHORIZED:
        return "Unauthorized";
    case FORBIDDEN:
        return "Forbidden";
    case NOT_FOUND:
        return "Not Found";
    case METHOD_NOT_ALLOWED:
        return "Method Not Allowed";
    case INTERNAL_SERVER_ERROR:
        return "Internal Server Error";
    case NOT_IMPLEMENTED:
        return "Not Implemented";
    case SERVICE_UNAVAILABLE:
        return "Service Unavailable";
    default:
        return "Unknown";
    }
}

std::string HttpResponse::toString() const {
    std::ostringstream oss;
    oss << version_ << " " << static_cast<int>(status_code_) << " "
        << getStatusText() << "\r\n";

    for (const auto &header : headers_) {
        oss << header.first << ": " << header.second << "\r\n";
    }

    oss << "\r\n" << body_;
    return oss.str();
}

void HttpResponse::clear() {
    status_code_ = OK;
    version_ = "HTTP/1.1";
    headers_.clear();
    body_.clear();
}

HttpResponse HttpResponse::ok(const std::string &body,
                              const std::string &content_type) {
    HttpResponse response(OK);
    response.setContentType(content_type);
    response.setBody(body);
    return response;
}

HttpResponse HttpResponse::json(const std::string &json_body) {
    HttpResponse response(OK);
    response.setContentType("application/json");
    response.setBody(json_body);
    return response;
}

HttpResponse HttpResponse::redirect(const std::string &location) {
    HttpResponse response(FOUND);
    response.addHeader("Location", location);
    return response;
}

HttpResponse HttpResponse::notFound(const std::string &message) {
    HttpResponse response(NOT_FOUND);
    response.setContentType("text/plain");
    response.setBody(message);
    return response;
}

HttpResponse HttpResponse::badRequest(const std::string &message) {
    HttpResponse response(BAD_REQUEST);
    response.setContentType("text/plain");
    response.setBody(message);
    return response;
}

HttpResponse HttpResponse::unauthorized(const std::string &message) {
    HttpResponse response(UNAUTHORIZED);
    response.setContentType("text/plain");
    response.setBody(message);
    return response;
}

HttpResponse HttpResponse::internalError(const std::string &message) {
    HttpResponse response(INTERNAL_SERVER_ERROR);
    response.setContentType("text/plain");
    response.setBody(message);
    return response;
}