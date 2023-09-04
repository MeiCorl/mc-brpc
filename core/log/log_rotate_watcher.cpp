#include "log_rotate_watcher.h"

#ifdef USE_ASYNC_LOGSINK
#include "async_logsink.h"
#endif

#include <bthread/unstable.h>
#include <bthread/bthread.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/inotify.h>

using namespace server::logger;

LogRotateWatcher::LogRotateWatcher(const std::string& log_path)
    : butil::SimpleThread("LogRotateWatcher")
    , _watch_path(log_path)
    , _is_asked_to_quit(false)
    , _quit_check_interval(200) {
    _log_file = NULL;
}

/**
 * 监听当前日志文件，当发现日志为删除、移动、修改则新建一个日志文件继续写
 * @date: 2023-08-12 11:28:46
 * @author: meicorl
*/
void LogRotateWatcher::Run() {
    int watch_fd = inotify_init();
    if (watch_fd == -1) {
        LOG(WARNING) << "Failed to init LogRotateWatcher inotify instance with errno: " << errno;
        return;
    }
    int watch_wd = -1;
    while (!_is_asked_to_quit) {
        if (watch_wd == -1) {
            watch_wd = inotify_add_watch(watch_fd, _watch_path.c_str(),
                                         IN_MOVE_SELF | IN_ATTRIB | IN_DELETE_SELF);
            if (watch_wd == -1) {
                // 检查频率过高，所以要控制日志频率
                LOG_EVERY_N(WARNING, 100) << "LogRotateWatcher failed to watch '" << _watch_path
                                          << "' with errno: " << errno;
                bthread_usleep(100000); // 每隔 100 毫秒检查一下文件是否存在，尽快为文件添加上 watch
                continue;
            } else {
                LOG(INFO) << "LogRotateWatcher start watching '" << _watch_path << "'";
            }
        }
        timespec ts = butil::milliseconds_from_now(_quit_check_interval);
        int res     = bthread_fd_timedwait(watch_fd, EPOLLIN, &ts);
        if (res == 0) {
            unsigned char buf[4096] = {0};
            ssize_t total_len       = read(watch_fd, &buf, sizeof(buf));
            if (total_len > 0) {
                ssize_t offset = 0;
                while (offset < total_len) {
                    inotify_event* event = reinterpret_cast<inotify_event*>(buf + offset);
                    offset += sizeof(inotify_event) + event->len;
                    if (event->wd != watch_wd) {
                        // IGNORE WD
                        //LOG(INFO) << "LogRotateWatcher ignore wd: " << event->wd;
                        continue;
                    }
                    // 收到了需要处理的回调信号
                    if (event->mask & (IN_MOVE_SELF | IN_ATTRIB | IN_DELETE_SELF)) {
                        LOG(INFO) << "LogRotateWatcher watched log change, closing log file";
#ifdef USE_ASYNC_LOGSINK
                        AsyncLogSink::Close();
#else
                        ::logging::CloseLogFile();
#endif
                        if (_log_file != NULL) {
                            fclose(_log_file);
                            _log_file = NULL;
                        }
                        inotify_rm_watch(watch_fd, watch_wd);
                        watch_wd = -1;
                        // 这是为了下次 watch 一定能看到日志文件，否则需要依赖其它线程来打日志
                        _log_file = fopen(_watch_path.c_str(), "a");
                        if (_log_file == NULL) {
                            fprintf(stderr, "LogRotateWatcher fail to open new log file %s",
                                    _watch_path.c_str());
                        } else {
                            // std::string log = "LogRotateWatcher open new log file\r\n";
                            // fwrite(log.data(), log.size(), 1, _log_file);
                            // fflush(_log_file);
                            LOG(INFO) << "LogRotateWatcher open new log file.";
                        }
                    } else {
                        // IGNORE EVENT
                        LOG(INFO) << "LogRotateWatcher ignore unexpected event: " << event->mask;
                    }
                }
            } else {
                LOG(WARNING) << "LogRotateWatcher failed to read event";
            }
        }
    }
    if (watch_wd != -1) {
        inotify_rm_watch(watch_fd, watch_wd);
        watch_wd = -1;
    }
    LOG(INFO) << "LogRotateWatcher thread finished";
}

short epoll_to_poll_events(uint32_t epoll_events) {
    // Most POLL* and EPOLL* are same values.
    short poll_events = (epoll_events &
                         (EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLRDNORM | EPOLLRDBAND | EPOLLWRNORM |
                          EPOLLWRBAND | EPOLLMSG | EPOLLERR | EPOLLHUP));
    CHECK_EQ((uint32_t)poll_events, epoll_events);
    return poll_events;
}

int fd_wait_by_poll(int fd, unsigned epoll_events, const timespec* abstime) {
    int diff_ms = -1;
    if (abstime) {
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        int64_t now_us     = butil::timespec_to_microseconds(now);
        int64_t abstime_us = butil::timespec_to_microseconds(*abstime);
        if (abstime_us <= now_us) {
            errno = ETIMEDOUT;
            return -1;
        }
        diff_ms = (abstime_us - now_us + 999L) / 1000L;
    }
    const short poll_events = epoll_to_poll_events(epoll_events);
    if (poll_events == 0) {
        errno = EINVAL;
        return -1;
    }
    pollfd ufds  = {fd, poll_events, 0};
    const int rc = poll(&ufds, 1, diff_ms);
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (ufds.revents & POLLNVAL) {
        errno = EBADF;
        return -1;
    }
    return 0;
}