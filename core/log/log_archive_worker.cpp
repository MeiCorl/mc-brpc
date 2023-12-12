#include "log_archive_worker.h"
#include <bthread/bthread.h>
#include <stdlib.h>

using namespace server::logger;

LogArchiveWorker::LogArchiveWorker(const std::string& log_file, uint32_t remain_days)
    : butil::SimpleThread("LogArchiveWorker")
    , _log_file(log_file)
    , _is_asked_to_quit(false)
    , _remain_days(remain_days) { }

LogArchiveWorker::~LogArchiveWorker() {
    _is_asked_to_quit = true;
    // if (this->HasBeenStarted() && !this->HasBeenJoined()) {
    // this->Join();
    // }
    LOG(INFO) << "LogArchiveWorker thread finished";
}

/**
 * 定时整点对日志文件进行压缩归档(配合日志监听线程完成)
 * @date: 2023-08-14 10:22:00
 * @author: meicorl
*/
void LogArchiveWorker::Run() {
    int64_t BENCHMARK_TIME = 1691978400000000; // UTC-8 2023-08-14 10:00:00
    LOG(INFO) << "LogArchiveWorker start...";
    while (!_is_asked_to_quit) {
        int64_t now = butil::gettimeofday_us();

        uint32_t curHourPass = (now - BENCHMARK_TIME) % 3600000000;
        uint32_t sleepUs     = 3600000000 - curHourPass;
        bthread_usleep(sleepUs);

        char tmp[32] = {0};
        time_t ts    = now / 1000000;
        strftime(tmp, sizeof(tmp), "%Y-%m-%d_%H", localtime(&ts));
        std::string date(tmp);

        memset(tmp, 0, sizeof(tmp));
        time_t one_month_ago_ts = ts - _remain_days * 86400;
        strftime(tmp, sizeof(tmp), "%Y-%m-%d_%H", localtime(&one_month_ago_ts));
        std::string one_month_ago(tmp);

        auto const pos = _log_file.find_last_of('.');
        std::string path_name;
        if (pos != std::string::npos) {
            path_name = _log_file.substr(0, pos);
        }

        // cmd example: gzip /data/log/brpc_test_d/brpc_test_d.log 
        //              && mv /data/log/brpc_test_d/brpc_test_d.log.gz /data/log/brpc_test_d/brpc_test_d_2023-12-08_09.log.gz 
        //              && rm -rf /data/log/brpc_test_d/brpc_test_d_2023-12-05_09.log.gz 
        std::ostringstream cmd;
        cmd << "gzip " << _log_file << " && mv " << _log_file << ".gz " << path_name << "_" << date
            << ".log.gz && rm -rf " << path_name << "_" << one_month_ago << ".log.gz";
        int ret = system(cmd.str().c_str());
        if (ret == -1) {
            LOG(ERROR) << "[!] system call failed.";
        }
        LOG(INFO) << "LogArchive cmd:" << cmd.str();

        bthread_usleep(5000000);
    }
}