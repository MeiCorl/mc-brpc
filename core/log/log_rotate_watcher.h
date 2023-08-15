#pragma once

#include "log_consts.h"
#include <string>
#include <butil/threading/simple_thread.h>
#include <fcntl.h>

namespace server {
namespace logger {

/* Note: the lease is released by either an explicit F_UNLCK operation on the duplicate file
*       descriptors, or when the file descriptor has been closed.
*/
// int call_fctnl_flock(int fd, bool do_lock = true);

// int get_sub_ins_index(const int port, const bool asyn_log, const std::string bin_name);

/**
 * 在 log rotate 的时候，brpc 继续写被 mv 走的文件。这个 watcher 会监控文件变化，如果
 * 有 mv 操作，就会自动重新打开一次 log 文件。
 */
class LogRotateWatcher : public butil::SimpleThread {
public:
    /**
     * 初始化
     * @param log_path 要监控的文件路径
     */
    LogRotateWatcher(const std::string& log_path);

    ~LogRotateWatcher() { Stop(); }

    /**
     * 停止线程
     */
    void Stop() {
        _is_asked_to_quit = true;
        if (this->HasBeenStarted() && !this->HasBeenJoined()) {
            this->Join();
        }
    }

    /**
     * 线程函数
     */
    void Run();

private:
    std::string _watch_path; ///< 要监控的 log 文件
    FILE* _log_file;         ///< 要监控的log文件指针
    bool _is_asked_to_quit;  ///< 是否结束线程
    int64_t _quit_check_interval; ///< 退出检查的时间间隔，单位毫秒，默认 200ms，除单测外不要修改
};

//needs a independent thread call this
class LogRotateThreadWatcher {
public:
    /**
     * 初始化
     * @param log_path 要监控的文件路径
     */
    LogRotateThreadWatcher(const std::string& log_path);

    ~LogRotateThreadWatcher() { Stop(); }

    /**
     * 停止线程
     */
    void Stop() { _is_asked_to_quit = true; }

    void Start();

private:
    std::string _watch_path; ///< 要监控的 log 文件
    FILE* _log_file;         ///< 要监控的log文件指针
    bool _is_asked_to_quit;  ///< 是否结束线程
    int64_t _quit_check_interval; ///< 退出检查的时间间隔，单位毫秒，默认 200ms，除单测外不要修改
};

} // namespace logger
} // namespace server
