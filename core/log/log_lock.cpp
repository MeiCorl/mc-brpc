#include "log_lock.h"

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

LoggingLock::LoggingLock() {
    LockLogging();
}

LoggingLock::~LoggingLock() {
    UnlockLogging();
}

void LoggingLock::Init(logging::LogLockingState lock_log, const logging::LogChar* new_log_file) {
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

void LoggingLock::LockLogging() {
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

void LoggingLock::UnlockLogging() {
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