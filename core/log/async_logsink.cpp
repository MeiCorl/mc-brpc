#include "async_logsink.h"
#include <fcntl.h>
#include <brpc/butil/file_util.h>
#include <brpc/butil/fd_guard.h>

#if defined(OS_WIN)
#include <io.h>
#include <windows.h>
typedef HANDLE MutexHandle;
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2
#elif defined(OS_MACOSX)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#elif defined(OS_POSIX)
#if defined(OS_NACL) || defined(OS_LINUX)
#include <sys/time.h>  // timespec doesn't seem to be in <time.h>
#else
#include <sys/syscall.h>
#endif
#include <time.h>
#endif
#if defined(OS_POSIX)
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX
typedef pthread_mutex_t* MutexHandle;
#endif

namespace server {
namespace logger {

namespace {
#if defined(OS_WIN)
typedef std::wstring PathString;
#else
typedef std::string PathString;
#endif

#if defined(OS_WIN)
PathString GetDefaultLogFile() {
    // On Windows we use the same path as the exe.
    wchar_t module_name[MAX_PATH];
    GetModuleFileName(NULL, module_name, MAX_PATH);

    PathString log_file = module_name;
    PathString::size_type last_backslash = log_file.rfind('\\', log_file.size());
    if (last_backslash != PathString::npos)
        log_file.erase(last_backslash + 1);
    log_file += L"debug.log";
    return log_file;
}
#endif

/**
 * LoggingLock is copy from butil/logging.cc, since it is private
 * @note meicorl
 * @date 2023-09-01 17:00
 */
class LoggingLock {
public:
    LoggingLock() {
        LockLogging();
    }

    ~LoggingLock() {
        UnlockLogging();
    }

    static void Init(logging::LogLockingState lock_log, const logging::LogChar* new_log_file) {
        if (initialized)
            return;
        lock_log_file = lock_log;
        if (lock_log_file == logging::LogLockingState::LOCK_LOG_FILE) {
#if defined(OS_WIN)
            if (!log_mutex) {
                std::wstring safe_name;
                if (new_log_file)
                    safe_name = new_log_file;
                else
                    safe_name = GetDefaultLogFile();
                // \ is not a legal character in mutex names so we replace \ with /
                std::replace(safe_name.begin(), safe_name.end(), '\\', '/');
                std::wstring t(L"Global\\");
                t.append(safe_name);
                log_mutex = ::CreateMutex(NULL, FALSE, t.c_str());

                if (log_mutex == NULL) {
#if DEBUG
                    // Keep the error code for debugging
                    int error = GetLastError();  // NOLINT
                    butil::debug::BreakDebugger();
#endif
                    // Return nicely without putting initialized to true.
                    return;
                }
            }
#endif
        } else {
            log_lock = new butil::Mutex;
        }
        initialized = true;
    }

private:
    static void LockLogging() {
        if (lock_log_file == logging::LogLockingState::LOCK_LOG_FILE) {
#if defined(OS_WIN)
            ::WaitForSingleObject(log_mutex, INFINITE);
            // WaitForSingleObject could have returned WAIT_ABANDONED. We don't
            // abort the process here. UI tests might be crashy sometimes,
            // and aborting the test binary only makes the problem worse.
            // We also don't use LOG macros because that might lead to an infinite
            // loop. For more info see http://crbug.com/18028.
#elif defined(OS_POSIX)
            pthread_mutex_lock(&log_mutex);
#endif
        } else {
            // use the lock
            log_lock->lock();
        }
    }

    static void UnlockLogging() {
        if (lock_log_file == logging::LogLockingState::LOCK_LOG_FILE) {
#if defined(OS_WIN)
            ReleaseMutex(log_mutex);
#elif defined(OS_POSIX)
            pthread_mutex_unlock(&log_mutex);
#endif
        } else {
            log_lock->unlock();
        }
    }

    // The lock is used if log file locking is false. It helps us avoid problems
    // with multiple threads writing to the log file at the same time.
    static butil::Mutex* log_lock;

    // When we don't use a lock, we are using a global mutex. We need to do this
    // because LockFileEx is not thread safe.
#if defined(OS_WIN)
    static MutexHandle log_mutex;
#elif defined(OS_POSIX)
    static pthread_mutex_t log_mutex;
#endif

    static bool initialized;
    static logging::LogLockingState lock_log_file;
};

// static
bool LoggingLock::initialized = false;
// static
butil::Mutex* LoggingLock::log_lock = NULL;
// static
logging::LogLockingState LoggingLock::lock_log_file = logging::LogLockingState::LOCK_LOG_FILE;

#if defined(OS_WIN)
// static
MutexHandle LoggingLock::log_mutex = NULL;
#elif defined(OS_POSIX)
pthread_mutex_t LoggingLock::log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

}  // namespace

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