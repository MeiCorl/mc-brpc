#pragma once

#include "butil/memory/singleton.h"
#include "db_pool.h"

namespace server {
namespace db {

class DBManager {
public:
    void Init();
    static DBManager* GetInstance() {
        return Singleton<DBManager>::get();
    }

    std::shared_ptr<MysqlConn> GetDBConnection(const char* db_name);
    std::shared_ptr<MysqlConn> GetDBConnection(const std::string& db_name);
};

}  // namespace db
}  // namespace server
