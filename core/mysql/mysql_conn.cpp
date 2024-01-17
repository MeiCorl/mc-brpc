#include "mysql_conn.h"

namespace server {
namespace db {

MysqlConn::MysqlConn() {
    // 获取一个MYSQL句柄
    m_conn = mysql_init(nullptr);
    // 设置字符集
    mysql_set_character_set(m_conn, "utf8");
}

MysqlConn::~MysqlConn() {
    if (m_conn != nullptr) {
        mysql_close(m_conn);
    }
    freeResult();
}

bool MysqlConn::connect(
    const std::string& ip,
    const std::string& user,
    const std::string& passwd,
    const std::string& dbName,
    unsigned int port) {
    /*
    MYSQL *mysql_real_connect(MYSQL *mysql, const char *host,
    const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long
    clientflag)
    */
    MYSQL* p = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
    return p != nullptr;
}

bool MysqlConn::undate(const std::string& sql) {
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }

    return true;
}

bool MysqlConn::query(const std::string& sql) {
    freeResult();
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    // 得到结果集
    m_result = mysql_store_result(m_conn);

    return true;
}

bool MysqlConn::next() {
    if (m_result != nullptr) {
        m_row = mysql_fetch_row(m_result);  // 获取一行
        if (m_row != nullptr) {
            return true;
        }
    }

    return false;
}

unsigned int MysqlConn::cols() {
    return mysql_num_fields(m_result);
}

std::string MysqlConn::value(int index) {
    int rowCount = mysql_num_fields(m_result);
    if (index >= rowCount || index < 0) {
        return "";
    }

    char* ans = m_row[index];
    unsigned long length = mysql_fetch_lengths(m_result)[index];

    return std::string(ans, length);
}

bool MysqlConn::transaction() {
    return mysql_autocommit(m_conn, false);  // 自动提交改为手动提交
}

bool MysqlConn::commit() {
    return mysql_commit(m_conn);
}

bool MysqlConn::rollback() {
    return mysql_rollback(m_conn);
}

void MysqlConn::freeResult() {
    if (m_result) {
        mysql_free_result(m_result);
        m_result = nullptr;
    }
}

void MysqlConn::refreshFreeTime() {
    m_freeTime = std::chrono::steady_clock::now();
}

// 计算连接空闲时长
long long MysqlConn::getFreeTime() {
    std::chrono::nanoseconds res = std::chrono::steady_clock::now() - m_freeTime;                // 纳秒
    std::chrono::milliseconds mil = std::chrono::duration_cast<std::chrono::milliseconds>(res);  // 将纳秒转成毫秒妙

    return mil.count();
}

// 返回当前连接id
unsigned long MysqlConn::id() {
    return m_conn->thread_id;
}

uint32_t MysqlConn::errNo() {
    return mysql_errno(m_conn);
}

std::string MysqlConn::errMsg() {
    return mysql_error(m_conn);
}

bool MysqlConn::refresh() {
    int res_code = mysql_ping(m_conn);
    if (res_code == 0) {
        return true;
    } else {
        return false;
    }
}

}  // namespace db
}  // namespace server