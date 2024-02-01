#include "fast_logsink.h"
#include "log_lock.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace server::logger;

const int PAGE_SIZE = 4 * 1024;             // 4KB
const int MAP_TRUNK_SIZE = 100 * 1024 * 1024;  // 4MB

int FastLogSink::log_fd = -1;
char* FastLogSink::begin_addr = nullptr;
char* FastLogSink::cur_addr = nullptr;
char* FastLogSink::end_addr = nullptr;

FastLogSink::FastLogSink(const logging::LoggingSettings& settings) :
        logging_dest(settings.logging_dest), log_file(settings.log_file) {
    LoggingLock::Init(settings.lock_log, settings.log_file);
    LoggingLock logging_lock;
    if (!Init(true)) {
        exit(-1);
    }
}

FastLogSink::~FastLogSink() {
    Close();
}

bool FastLogSink::OnLogMessage(int severity, const char* file, int line, const butil::StringPiece& log_content) {
    return OnLogMessage(severity, file, line, "", log_content);
}

bool FastLogSink::OnLogMessage(
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
        Init();
        while (cur_addr + log_content.length() >= end_addr) {
            AdjustFileMap();
        }
        memcpy(cur_addr, log_content.data(), log_content.length());
        cur_addr += log_content.length();
    }

    return true;
}

bool FastLogSink::Init(bool auto_create) {
    if (log_fd != -1) {
        return true;
    }

    // 打开文件
    if (auto_create) {
        log_fd = open(log_file.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);
        if (log_fd == -1) {
            if (EEXIST == errno) {
                // file already exist, just open
                log_fd = open(log_file.c_str(), O_RDWR);
            }
        }
    } else {
        log_fd = open(log_file.c_str(), O_RDWR);
    }

    if (log_fd == -1) {
        std::ostringstream os;
        os << "Fail to init log, file:" << log_file << std::endl;
        fwrite(os.str().c_str(), os.str().size(), 1, stderr);
        return false;
    }

    AdjustFileMap();

    return true;
}

void FastLogSink::AdjustFileMap() {
    size_t offset = 0;
    size_t file_size = 0;
    if (begin_addr != nullptr && end_addr != nullptr) {
        file_size = end_addr - begin_addr;
        offset = cur_addr - begin_addr;
        munmap(begin_addr, file_size);
    } else {
        struct stat fileStat;
        fstat(log_fd, &fileStat);
        file_size = fileStat.st_size;
        offset = file_size;
    }

    size_t new_file_size = file_size + MAP_TRUNK_SIZE;
    ftruncate(log_fd, new_file_size);
    begin_addr = (char*)mmap(NULL, new_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, 0);
    if (begin_addr == MAP_FAILED) {
        std::string err = "mmap failed, errno:" + std::to_string(errno);
        fwrite(err.c_str(), err.size(), 1, stderr);
        exit(1);
    }
    cur_addr = begin_addr + offset;
    end_addr = begin_addr + new_file_size;
}

void FastLogSink::Close() {
    LoggingLock logging_lock;

    if (log_fd != -1) {
        close(log_fd);
        munmap(begin_addr, end_addr - begin_addr);
        log_fd = -1;
        begin_addr = nullptr;
        cur_addr = nullptr;
        end_addr = nullptr;
    }
}