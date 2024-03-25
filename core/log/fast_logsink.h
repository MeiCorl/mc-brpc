#pragma once


#include <brpc/butil/logging.h>

namespace server {
namespace logger {

/**
 * mmap文件内存映射写日志
 * @date: 2024-01-30 15:00:00
 * @author: meicorl
*/
class FastLogSink : public logging::LogSink {
private:
    logging::LoggingDestination logging_dest;
    std::string log_file;

    static volatile int log_fd;
    static char* begin_addr;
    static char* cur_addr;
    static char* end_addr;

    bool Init();
    void AdjustFileMap();
public:
    FastLogSink(const logging::LoggingSettings& settings);
    virtual ~FastLogSink();

    bool OnLogMessage(int severity,
                      const char* file,
                      int line,
                      const butil::StringPiece& log_content) override;
    bool OnLogMessage(int severity,
                      const char* file,
                      int line,
                      const char* func,
                      const butil::StringPiece& log_content) override;

    static void Close();
};

} // namespace logger
} // namespace server
