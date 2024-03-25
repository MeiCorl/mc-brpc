#include "fast_logsink.h"
#include "log_lock.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <gflags/gflags.h>

using namespace server::logger;

DEFINE_uint32(
    map_trunk_size,
    4 * 1024,
    "size of each mmap(default 4kb, set bigger size in production environment to get a better performance)");

volatile int FastLogSink::log_fd = -1;
char* FastLogSink::begin_addr = nullptr;
char* FastLogSink::cur_addr = nullptr;
char* FastLogSink::end_addr = nullptr;

/**
 * 获取系统页面大小
 **/
size_t GetPageSize() {
    return sysconf(_SC_PAGESIZE);
}

/**
 * 将 size 向下舍入到页面大小的整数倍
 **/
size_t RoundDownToPageSize(size_t size) {
    // 获取系统页面大小
    size_t page_size = GetPageSize();
    return size & ~(page_size - 1);
}

FastLogSink::FastLogSink(const logging::LoggingSettings& settings) :
        logging_dest(settings.logging_dest), log_file(settings.log_file) {
    LoggingLock::Init(settings.lock_log, settings.log_file);
    LoggingLock logging_lock;
    if (!Init()) {
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
        if (Init()) {
            while (cur_addr + log_content.length() > end_addr) {
                AdjustFileMap();
            }
            memcpy(cur_addr, log_content.data(), log_content.length());
            cur_addr += log_content.length();
        }
    }

    return true;
}

bool FastLogSink::Init() {
    if (log_fd != -1) {
        return true;
    }

    // 打开文件
    log_fd = open(log_file.c_str(), O_RDWR | O_CREAT, 0666);
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
    size_t file_size = 0;
    struct stat fileStat;
    fstat(log_fd, &fileStat);
    file_size = fileStat.st_size;

    size_t cur_pos = 0;  // 当前写入位置相对于文件头的offset
    if (begin_addr != nullptr && end_addr != nullptr) {
        cur_pos = file_size - (end_addr - cur_addr);
        munmap(begin_addr, FLAGS_map_trunk_size);
    } else {
        cur_pos = file_size;
    }

    // 下次映射从当前写入地址(cur_pos)的上一文件页末尾开始(会产生长度为file_size-offset的区域被重复映射)
    size_t offset = RoundDownToPageSize(cur_pos);
    size_t new_file_size = file_size + (FLAGS_map_trunk_size - (file_size - offset) + 1);
    ftruncate(log_fd, new_file_size);

    begin_addr = (char*)mmap(NULL, FLAGS_map_trunk_size, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, offset);
    if (begin_addr == MAP_FAILED) {
        std::ostringstream os;
        os << "mmap failed, errno:" << errno << ", log_fd:" << log_fd << std::endl;
        std::string&& err = os.str();
        fwrite(err.c_str(), err.size(), 1, stderr);
        exit(1);
    }
    cur_addr =
        begin_addr + (cur_pos - offset);  // 当前写入位置应为起始地址加上上次映射写入位置相对于当前映射起始地址的偏移量
    end_addr = begin_addr + FLAGS_map_trunk_size;
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