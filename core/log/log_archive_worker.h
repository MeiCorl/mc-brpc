#pragma once

#include "log_consts.h"
#include <butil/threading/simple_thread.h>

namespace server {
namespace logger {

class LogArchiveWorker : public butil::SimpleThread {
private:
    std::string _log_file;
    bool _is_asked_to_quit;

public:
    LogArchiveWorker(const std::string& log_file);
    ~LogArchiveWorker();

    void Run();
};

} // namespace logger
} // namespace server