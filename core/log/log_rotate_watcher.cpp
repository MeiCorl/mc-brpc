#include "log_rotate_watcher.h"

#include <bthread/unstable.h>
#include <bthread/bthread.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/inotify.h>


using namespace server::logger;

/*
int call_fctnl_flock(int fd, bool do_lock) {
    struct flock lock;
    lock.l_type   = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start  = 0;
    lock.l_len    = 0; // Lock entire file.
    return fcntl(fd, do_lock ? F_SETLK : F_UNLCK, &lock);
}

int get_sub_ins_index(const int port, const bool asyn_log, const std::string bin_name) {
    // no sub ins if port eq 0
    if (port <= 0) {
        return -1;
    }

    // port is regarded as sub_ins_id if yy log
    if (!asyn_log) {
        return port;
    }

    // exit when too many instances
    std::string cmd = "pidof " + bin_name + "|wc -w";
    FILE* fp        = popen(cmd.c_str(), "r");
    int count       = 0;
    if (!fp || fscanf(fp, "%d", &count) < 0 || count > 32) {
        //LOG(ERROR) << "Failed to count proc for " << bin_name << ". count:" << count;
        exit(-1);
    }
    pclose(fp);

    // get curr time
    timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    tm local_tm;
    localtime_r(&t, &local_tm);
    char local_buff[32];
    snprintf(local_buff, 32, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.6ld", local_tm.tm_year + 1900,
             local_tm.tm_mon + 1, local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min,
             local_tm.tm_sec, tv.tv_usec);

    // get sub_ins_id and update meta
    std::string meta_name =
        std::string(get_log_path(bin_name.c_str(), true) + "/" + bin_name + ".meta");
    fp = fopen(meta_name.c_str(), "a+");
    if (!fp || 0 != call_fctnl_flock(fileno(fp))) {
        //LOG(ERROR) << "Fail to open meta file";
        exit(-1);
    }

    char* line     = NULL;
    size_t len     = 0;
    int sub_ins_id = 0;
    if (getline(&line, &len, fp) < 0) {
        std::string title = "start_time\tlisten_port\tsub_ins_id\r\n";
        std::string msg   = std::string(local_buff) + " " + std::to_string(port) + " 0\r\n";
        if (fwrite(title.c_str(), title.size(), 1, fp) == 0 ||
            fwrite(msg.c_str(), msg.size(), 1, fp) == 0) {
            //LOG(ERROR) << "Failed to write data into meta";
            exit(-1);
        }
    } else {
        if (getline(&line, &len, fp) > 0) {
            try {
                sub_ins_id = (atoi(strrchr(line, ' ') + 1) + 1) % 32;
            } catch (std::exception& e) {
                //LOG(ERROR) << "Failed to get sub_ins_id. exception:" << e.what();
                exit(-1);
            } catch (...) {
                //LOG(ERROR) << "Failed to get sub_ins_id.";
                exit(-1);
            }
        }
        cmd = std::string("sed -i '1a ") + local_buff + " " + std::to_string(port) + " " +
            std::to_string(sub_ins_id) + "' " + meta_name;
        FILE* fp1 = popen(cmd.c_str(), "w"); // NOTE: auto release fcntl lock here
        if (!fp1) {
            //LOG(ERROR) << "Failed to popen meta";
            exit(-1);
        }
        pclose(fp1);
    }

    if (line) {
        free(line);
    }
    fclose(fp); //NOTE: auto release fcntl lock as well
    return sub_ins_id;
}
*/

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
                LOG_EVERY_N(WARNING, 100) << "[*] LogRotateWatcher failed to watch '" << _watch_path
                                          << "' with errno: " << errno;
                bthread_usleep(100000); // 每隔 100 毫秒检查一下文件是否存在，尽快为文件添加上 watch
                continue;
            } else {
                LOG(INFO) << "[+] LogRotateWatcher start watching '" << _watch_path << "'";
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
                        LOG(INFO) << "[+] LogRotateWatcher watched log change, closing log file";
                        ::logging::CloseLogFile();
                        if (_log_file != NULL) {
                            fclose(_log_file);
                            _log_file = NULL;
                        }
                        inotify_rm_watch(watch_fd, watch_wd);
                        watch_wd = -1;
                        // 这是为了下次 watch 一定能看到日志文件，否则需要依赖其它线程来打日志
                        LOG(INFO) << "[+] LogRotateWatcher open new log file";
                        _log_file = fopen(_watch_path.c_str(), "a");
                        if (_log_file == NULL) {
                            fprintf(stderr, "LogRotateWatcher fail to open new log file %s",
                                    _watch_path.c_str());
                        } else {
                            std::string log = "LogRotateWatcher open new log file\r\n";
                            fwrite(log.data(), log.size(), 1, _log_file);
                            fflush(_log_file);
                        }
                    } else {
                        // IGNORE EVENT
                        LOG(INFO) << "LogRotateWatcher ignore unexpected event: " << event->mask;
                    }
                }
            } else {
                LOG(WARNING) << "[*] LogRotateWatcher failed to read event";
            }
        }
    }
    if (watch_wd != -1) {
        inotify_rm_watch(watch_fd, watch_wd);
        watch_wd = -1;
    }
    LOG(INFO) << "[+] LogRotateWatcher thread finished";
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

LogRotateThreadWatcher::LogRotateThreadWatcher(const std::string& log_path)
    : _watch_path(log_path), _is_asked_to_quit(false), _quit_check_interval(200) {
    // TODO Auto-generated constructor stub
    _log_file = NULL;
}

void LogRotateThreadWatcher::Start() {
    int watch_fd = inotify_init();
    if (watch_fd == -1) {
        LOG(WARNING) << "[*] Failed to init LogRotateThreadWatcher inotify instance with errno: "
                     << errno;
        return;
    }
    int watch_wd = -1;
    while (!_is_asked_to_quit) {
        if (watch_wd == -1) {
            watch_wd = inotify_add_watch(watch_fd, _watch_path.c_str(),
                                         IN_MOVE_SELF | IN_ATTRIB | IN_DELETE_SELF);
            if (watch_wd == -1) {
                // 检查频率过高，所以要控制日志频率
                LOG_EVERY_N(WARNING, 100) << "[*] LogRotateThreadWatcher failed to watch '"
                                          << _watch_path << "' with errno: " << errno;
                bthread_usleep(10000); // 每隔 10 毫秒检查一下文件是否存在，尽快为文件添加上 watch
                continue;
            } else {
                LOG(INFO) << "[+] LogRotateThreadWatcher start watching '" << _watch_path << "'";
            }
        }
        bthread_usleep(10); //usleep tmp
        timespec ts = butil::milliseconds_from_now(_quit_check_interval);
        int res     = fd_wait_by_poll(watch_fd, EPOLLIN, &ts);
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
                        //LOG(INFO) << "LogRotateThreadWatcher ignore wd: " << event->wd;
                        continue;
                    }
                    // 收到了需要处理的回调信号
                    if (event->mask & (IN_MOVE_SELF | IN_ATTRIB | IN_DELETE_SELF)) {
                        LOG(INFO)
                            << "[+] LogRotateThreadWatcher watched log change, closing log file";
                        ::logging::CloseLogFile();
                        if (_log_file != NULL) {
                            fclose(_log_file);
                            _log_file = NULL;
                        }
                        inotify_rm_watch(watch_fd, watch_wd);
                        watch_wd = -1;
                        // 这是为了下次 watch 一定能看到日志文件，否则需要依赖其它线程来打日志
                        LOG(INFO) << "[+] LogRotateThreadWatcher open new log file";
                        _log_file = fopen(_watch_path.c_str(), "a");
                        if (_log_file == NULL) {
                            fprintf(stderr, "LogRotateThreadWatcher fail to open new log file %s",
                                    _watch_path.c_str());
                        } else {
                            std::string log = "LogRotateThreadWatcher open new log file\r\n";
                            fwrite(log.data(), log.size(), 1, _log_file);
                            fflush(_log_file);
                        }
                    } else {
                        // IGNORE EVENT
                        LOG(INFO) << "[+] LogRotateThreadWatcher ignore unexpected event: "
                                  << event->mask;
                    }
                }
            } else {
                LOG(WARNING) << "[*] LogRotateThreadWatcher failed to read event";
            }
        }
    }
    if (watch_wd != -1) {
        inotify_rm_watch(watch_fd, watch_wd);
        watch_wd = -1;
    }
    LOG(INFO) << "[+] LogRotateThreadWatcher thread finished";
}
