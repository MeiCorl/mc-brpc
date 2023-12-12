/**
 * DB连接池实现
 * @date: 2023-12-08 17:30:00
 * @author: meicorl
 */

#pragma once

#include <queue>
#include "bthread/mutex.h"
#include "bthread/condition_variable.h"
#include "core/config/server_config.h"
#include "mysql_conn.h"

namespace server {
namespace db {

using server::config::DbConfig;

enum LockStatus { UnLock = 0, Locked = 1 };
class DBPool {
private:
    std::string m_clusterName;  // 当前pool对应集群名称
    DbConfig m_dbConf;          // db参数配置（server.conf中指定)
    /**
        message DbConfig {
           string user = 1;
           string passwd = 2;
           string ip = 3;
           uint32 port = 4;
           string db_name = 5;
           uint32 max_active = 6;          // 最大活跃连接数
           uint32 min_idle = 8;            // 最小空闲连接数
           uint32 ilde_timeout_ms = 9;     // 空闲连接超时时间(超过会自动释放连接)
           uint32 timeout_ms = 10;         // 获取连接超时时间
       }*/
    uint32_t m_curActive;  // 当前活跃连接数量

    std::queue<MysqlConn*> m_connQue;  // 空闲连接队列
    bthread::Mutex m_mtx;
    bthread::ConditionVariable m_cond;

    bool AddConnect();
    void RecycleConnection();

public:
    DBPool(const std::string& cluster_name, const DbConfig& db_conf);
    DBPool(const DBPool& pool) = delete;
    DBPool& operator=(const DBPool& pool) = delete;
    DBPool(DBPool&& pool) = delete;
    DBPool& operator=(DBPool&& pool) = delete;
    ~DBPool();

    std::shared_ptr<MysqlConn> GetConnection();
};

}  // namespace db
}  // namespace server