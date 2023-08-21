#pragma once

#include "log_consts.h"
#include <butil/threading/simple_thread.h>

namespace server {
namespace logger {

class LogArchiveWorker : public butil::SimpleThread {
private:
    std::string _log_file;
    bool _is_asked_to_quit;
    uint32_t _remain_days;

public:
    LogArchiveWorker(const std::string& log_file, uint32_t remain_days = 30);
    ~LogArchiveWorker();

    void Run();
};

} // namespace logger
} // namespace server