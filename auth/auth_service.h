#pragma once
#include "../http/http_request.h"
#include "../http/http_response.h"
#include "../database/db_connection_pool.h"

class AuthService {
public:
    // 添加 dbPool 参数
    static void handleRegister(const HttpRequest& req, HttpResponse& resp, DBConnectionPool* dbPool);
    static void handleLogin(const HttpRequest& req, HttpResponse& resp, DBConnectionPool* dbPool);
};
