#include "auth_service.h"
#include "user_model.h"
#include <string>

void AuthService::handleRegister(const HttpRequest &req, HttpResponse &resp,
                                 DBConnectionPool *dbPool) {
    if (req.getMethod() != HttpRequest::Method::POST) {
        resp.setStatusCode(HttpResponse::StatusCode(405)); // Method Not Allowed
        resp.setBody("Method Not Allowed");
        return;
    }

    // 获取 POST 参数
    std::string username = req.getParam("username");
    std::string password = req.getParam("password");

    if (username.empty() || password.empty()) {
        resp.setStatusCode(HttpResponse::StatusCode(400));
        resp.setBody("Username or password cannot be empty");
        return;
    }

    // 使用 UserModel 操作数据库
    UserModel userModel(dbPool);
    bool success = userModel.registerUser(username, password);

    if (success) {
        resp.setStatusCode(HttpResponse::StatusCode(200));
        resp.setBody("Registration successful");
    } else {
        resp.setStatusCode(HttpResponse::StatusCode(409)); // Conflict
        resp.setBody("Registration failed: user may already exist");
    }
}

void AuthService::handleLogin(const HttpRequest &req, HttpResponse &resp,
                              DBConnectionPool *dbPool) {
    if (req.getMethod() != HttpRequest::Method::POST) {
        resp.setStatusCode(HttpResponse::StatusCode(405)); // Method Not Allowed
        resp.setBody("Method Not Allowed");
        return;
    }

    std::string username = req.getParam("username");
    std::string password = req.getParam("password");

    if (username.empty() || password.empty()) {
        resp.setStatusCode(HttpResponse::StatusCode(400));
        resp.setBody("Username or password cannot be empty");
        return;
    }

    UserModel userModel(dbPool);
    bool success = userModel.loginUser(username, password);

    if (success) {
        resp.setStatusCode(HttpResponse::StatusCode(200));
        resp.setBody("Login successful");
    } else {
        resp.setStatusCode(HttpResponse::StatusCode(401)); // Unauthorized
        resp.setBody("Login failed: invalid credentials");
    }
}
