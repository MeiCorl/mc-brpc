#pragma once

#include "core/utils/singleton.h"
#include "db_pool.h"

namespace server {
namespace db {

class DbManager {
public:
    void Init();

    std::shared_ptr<MysqlConn> GetDBConnection(const char* db_name);
    std::shared_ptr<MysqlConn> GetDBConnection(const std::string& db_name);
};

}  // namespace db
}  // namespace server

using DBManager = server::utils::Singleton<server::db::DbManager>;
