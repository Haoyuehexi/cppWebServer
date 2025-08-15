#include "user_model.h"
#include "../database/db_connection_pool.h"

UserModel::UserModel(DBConnectionPool* dbPool)
    : dbPool_(dbPool) {}   // 保存指针

bool UserModel::registerUser(const std::string& username, const std::string& password) {
    if (!dbPool_) return false;

    auto conn = dbPool_->getConnection();
    if (!conn || !conn->isConnected()) return false;

    return conn->registerUser(username, password);
}

bool UserModel::loginUser(const std::string& username, const std::string& password) {
    if (!dbPool_) return false;

    auto conn = dbPool_->getConnection();
    if (!conn || !conn->isConnected()) return false;

    return conn->loginUser(username, password);
}
