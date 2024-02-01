#pragma once

#include <fcntl.h>
#include <brpc/butil/file_util.h>
#include <brpc/butil/fd_guard.h>

/**
 * LoggingLock is copy from butil/logging.cc, since it is private
 * @note meicorl
 * @date 2023-09-01 17:00
 */
class LoggingLock {
public:
    LoggingLock();

    ~LoggingLock();

    static void Init(logging::LogLockingState lock_log, const logging::LogChar* new_log_file);

private:
    static void LockLogging();

    static void UnlockLogging();

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