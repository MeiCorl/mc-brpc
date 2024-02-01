#include "async_logsink.h"
#include "log_lock.h"

namespace server {
namespace logger {

volatile int AsyncLogSink::log_fd = -1;

AsyncLogSink::AsyncLogSink(const logging::LoggingSettings& settings) :
        butil::SimpleThread("LogFlushThread"),
        logging_dest(settings.logging_dest),
        log_file(settings.log_file),
        is_asked_to_quit(false) {
    LoggingLock::Init(settings.lock_log, settings.log_file);
    LoggingLock logging_lock;
    if (!Init(true)) {
        exit(-1);
    }
    this->Start();
}

AsyncLogSink::~AsyncLogSink() {
    is_asked_to_quit = true;
    if (this->HasBeenStarted() && !this->HasBeenJoined()) {
        this->Join();
    }

    fsync(log_fd);
    Close();
}

void AsyncLogSink::Run() {
    LOG(INFO) << "LogFlushThread start...";
    while (!is_asked_to_quit) {
        bthread_usleep(1000000);

        std::string s;
        {
            LoggingLock logging_lock;
            if (log_fd == -1) {
                Init();
            }

            s = _stream.str();
            _stream.str("");
        }

        if (log_fd != -1 && !s.empty()) {
            write(log_fd, s.c_str(), s.length());
        }
    }
}

bool AsyncLogSink::OnLogMessage(int severity, const char* file, int line, const butil::StringPiece& log_content) {
    return OnLogMessage(severity, file, line, "", log_content);
}

bool AsyncLogSink::OnLogMessage(
    int severity,
    const char* file,
    int line,
    const char* func,
    const butil::StringPiece& log_content) {
    if ((logging_dest & logging::LoggingDestination::LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
        fwrite(log_content.data(), log_content.length(), 1, stderr);
        fflush(stderr);
    }

    if ((logging_dest & logging::LoggingDestination::LOG_TO_FILE) != 0) {
        LoggingLock logging_lock;
        _stream << log_content;
    }

    return true;
}

bool AsyncLogSink::Init(bool auto_create) {
    if (log_fd != -1) {
        return true;
    }
    if (auto_create) {
        log_fd = open(log_file.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0666);
        if (log_fd == -1) {
            if (EEXIST == errno) {
                // file already exist, just open
                log_fd = open(log_file.c_str(), O_WRONLY | O_APPEND);
            }
        }
    } else {
        log_fd = open(log_file.c_str(), O_WRONLY | O_APPEND);
    }

    if (log_fd == -1) {
        std::ostringstream os;
        os << "Fail to init log, file:" << log_file << std::endl;
        fwrite(os.str().c_str(), os.str().size(), 1, stderr);
        return false;
    }
    return true;
}

void AsyncLogSink::Close() {
    LoggingLock logging_lock;

    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}

}  // namespace logger
}  // namespace server