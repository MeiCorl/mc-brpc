#ifndef ZERO_COPY_LOG_SINK
#define ZERO_COPY_LOG_SINK

#include <brpc/butil/logging.h>
#include <butil/threading/simple_thread.h>
#include <bthread/bthread.h>

namespace server {
namespace logger {

/**
 * 异步、批量写日志
 * @date: 2023-09-10 09:00:00
 * @author: meicorl
*/
class AsyncLogSink : public logging::LogSink, public butil::SimpleThread {
private:
    logging::LoggingDestination logging_dest;
    std::string log_file;
    static volatile int log_fd;

    bool is_asked_to_quit;
    std::ostringstream _stream;

public:
    AsyncLogSink(const logging::LoggingSettings& settings);
    virtual ~AsyncLogSink();

    void Run();

    bool OnLogMessage(int severity,
                      const char* file,
                      int line,
                      const butil::StringPiece& log_content) override;
    bool OnLogMessage(int severity,
                      const char* file,
                      int line,
                      const char* func,
                      const butil::StringPiece& log_content) override;

    bool Init(bool auto_create = false);
    static void Close();
};

} // namespace logger
} // namespace server
#endif