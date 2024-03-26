#include "brpc/extension.h"

#include "db_manager.h"

using namespace server::db;
using namespace server::config;

void DBManager::Init() {
    const google::protobuf::Map<std::string, DbConfig>& db_confs = ServerConfig::GetInstance()->GetDbConfig();
    for (auto it = db_confs.begin(); it != db_confs.end(); ++it) {
        brpc::Extension<DBPool>::instance()->RegisterOrDie(it->first, new DBPool(it->first, it->second));
    }
}

std::shared_ptr<MysqlConn> DBManager::GetDBConnection(const char* db_name) {
    DBPool* pool = brpc::Extension<DBPool>::instance()->Find(db_name);
    if (pool == nullptr) {
        LOG(ERROR) << "[!] db not found: " << db_name;
        return nullptr;
    }
    return pool->GetConnection();
}

std::shared_ptr<MysqlConn> DBManager::GetDBConnection(const std::string& db_name) {
    return GetDBConnection(db_name.c_str());
}