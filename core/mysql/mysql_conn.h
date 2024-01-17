#pragma once

#include <mysql/mysql.h>
#include <chrono>
#include <string>

/**
 * 基于MySQL C API（libmysqlclient）封装C++面向对象Mysql操作类
 * @date: 2023-12-12 17:51:00
 * @author: meicorl
 */

namespace server {
namespace db {

class MysqlConn {
public:
    // 创建一个MYSQL实例对象并设置字符集
    MysqlConn();

    // 关闭连接，释放资源
    ~MysqlConn();

    // 连接指定的数据库
    bool connect(
        const std::string& ip,
        const std::string& user,
        const std::string& passwd,
        const std::string& bdName,
        unsigned int port);

    // 更新:增加、删除、修改
    bool undate(const std::string& sql);

    // 查询
    bool query(const std::string& sql);

    // 遍历得到的结果集
    bool next();

    // 返回结果集中字段数目
    unsigned int cols();

    // 获取结果集里的值
    std::string value(int index);

    // 开启事务
    bool transaction();

    // 事务提交
    bool commit();

    // 事务回滚
    bool rollback();

    // 更新空闲时间点
    void refreshFreeTime();

    // 计算连接空闲时长
    long long getFreeTime();

    // 返回当前连接id
    unsigned long id();

    // 返回上次sql执行错误码
    uint32_t errNo();

    // 返回上次sql执行错误信息
    std::string errMsg();

    // 检测刷新连接，避免长时间空闲被服务器断开
    bool refresh();

private:
    // 每次搜索都需要更新结果集
    void freeResult();

    MYSQL* m_conn = nullptr;
    MYSQL_RES* m_result = nullptr;
    MYSQL_ROW m_row;

    std::chrono::steady_clock::time_point m_freeTime;
};

}  // namespace db
}  // namespace server