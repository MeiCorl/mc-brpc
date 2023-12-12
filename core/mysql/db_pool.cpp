#include "db_pool.h"
#include "butil/logging.h"
#include "bthread/butex.h"
#include <thread>

namespace server {
namespace db {

DBPool::DBPool(const std::string& cluster_name, const DbConfig& db_conf) :
        m_clusterName(cluster_name), m_dbConf(db_conf), m_curActive(0) {
    // 创建子线程用于销毁空闲链接
    std::thread(&DBPool::RecycleConnection, this).detach();

    LOG(INFO) << "db pool inited[" << m_clusterName << "]";
}

DBPool::~DBPool() {
    std::lock_guard<bthread::Mutex> lck(m_mtx);
    while (!m_connQue.empty()) {
        MysqlConn* conn = m_connQue.front();
        m_connQue.pop();
        delete conn;
    }
}

/**
 * 获取从队列头部获取一个连接（也即空闲最久的连接），释放连接时自动将连接插入队列尾部
 */
std::shared_ptr<MysqlConn> DBPool::GetConnection() {
    std::unique_lock<bthread::Mutex> lck(m_mtx);
    // 当空闲连接队列为空则判断：1、当前最大活跃连接数已达上限，则等待  2、当前最大活跃连接数未达上限，新建连接
    if (m_connQue.empty()) {
        if (m_curActive >= m_dbConf.max_active()) {
            LOG(INFO) << "WaitForConnection, size:" << m_connQue.size() << ", active_size:" << m_curActive;
            int64_t timeout_us = (m_dbConf.timeout_ms() > 0 ? m_dbConf.timeout_ms() : 1000) * 1000;
            int64_t now = butil::gettimeofday_us();
            while (m_connQue.empty()) {
                int status = m_cond.wait_for(lck, timeout_us);
                if (status == ETIMEDOUT) {
                    // 获取链接超时返回nullptr
                    LOG(ERROR) << "[!] get db connection timeout:" << status << " [" << m_clusterName << "."
                               << m_dbConf.db_name() << "], conn_timeout_ms:" << m_dbConf.timeout_ms();
                    return nullptr;
                }
                timeout_us -= (butil::gettimeofday_us() - now);
            }
        } else {
            bool is_succ = AddConnect();
            if (!is_succ) {
                return nullptr;
            }
        }
    }

    // 自定义shared_ptr析构方法, 重新将连接放回到连接池中, 而不是销毁
    std::shared_ptr<MysqlConn> connptr(m_connQue.front(), [this](MysqlConn* conn) {
        std::unique_lock<bthread::Mutex> lck(this->m_mtx);
        conn->refreshAliveTime();
        LOG(DEBUG) << "release connection, id:" << conn->id();
        this->m_connQue.push(conn);
        this->m_curActive--;
        this->m_cond.notify_one();
    });

    LOG(DEBUG) << "get connection, id:" << connptr->id();
    m_connQue.pop();
    m_curActive++;
    return connptr;
}

bool DBPool::AddConnect() {
    MysqlConn* conn = new MysqlConn;
    if (!conn->connect(m_dbConf.ip(), m_dbConf.user(), m_dbConf.passwd(), m_dbConf.db_name(), m_dbConf.port())) {
        LOG(ERROR) << "[!] fail to connect to db: " << m_dbConf.ShortDebugString();
        return false;
    }

    LOG(INFO) << "add new connection, id:" << conn->id() << "[" << m_clusterName << "." << m_dbConf.db_name() << "]";
    conn->refreshAliveTime();
    m_connQue.push(conn);
    return true;
}

/**
 * 子线程--每5s执行一次检测并释放空闲(超时)连接
 */
void DBPool::RecycleConnection() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::unique_lock<bthread::Mutex> lck(m_mtx);
        while (m_connQue.size() > m_dbConf.min_idle()) {
            MysqlConn* recyConn = m_connQue.front();
            if (recyConn->getAliveTime() >= m_dbConf.ilde_timeout_ms()) {
                LOG(INFO) << "recycle connection, id:" << recyConn->id() << "[" << m_clusterName << "."
                          << m_dbConf.db_name() << "]";
                m_connQue.pop();
                delete recyConn;
            } else {
                break;
            }
        }
    }
}

}  // namespace db
}  // namespace server