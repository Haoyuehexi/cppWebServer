#ifndef USER_MODEL_H
#define USER_MODEL_H

#include <string>
#include "../database/db_connection_pool.h"

class UserModel {
public:
    explicit UserModel(DBConnectionPool* dbPool);

    bool registerUser(const std::string& username, const std::string& password);
    bool loginUser(const std::string& username, const std::string& password);

private:
    DBConnectionPool* dbPool_;
};

#endif
