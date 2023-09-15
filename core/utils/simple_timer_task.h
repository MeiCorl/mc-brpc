#pragma once

#include <butil/threading/simple_thread.h>
#include <butil/class_name.h>
#include <condition_variable>
#include <mutex>
#include <chrono>

namespace server {
namespace utils {

template <typename T>
struct timer_callback_func {
    typedef void (T::*callback)();
};

/**
 * 简单定时任务类
 * @date: 2023-08-15 09:00:00
 * @author: meicorl
*/
template <typename T, typename timer_callback_func<T>::callback ptr>
class SimpleTimerTask : public butil::SimpleThread {
private:
    bool is_asked_to_quit;
    T* pobj;
    uint32_t time_interval_s;
    std::condition_variable cond;
    std::mutex mtx;

public:
    SimpleTimerTask() : butil::SimpleThread(butil::class_name<T>()), is_asked_to_quit(false) { }

    ~SimpleTimerTask() {
        is_asked_to_quit = true;
        cond.notify_all();
        if (this->HasBeenStarted() && !this->HasBeenJoined()) {
            this->Join();
        }
    }

    void Init(T* x, uint32_t ts) {
        pobj            = x;
        time_interval_s = ts;
    }

    void Run() {
        LOG(INFO) << "[+] SimpleTimerTask(" << butil::class_name<T>() << ") thread start";
        while (!is_asked_to_quit) {
            std::unique_lock<std::mutex> lck(mtx);
            cond.wait_for(lck, std::chrono::seconds(time_interval_s));
            if (is_asked_to_quit) {
                break;
            }
            (pobj->*ptr)();
        }
        LOG(INFO) << "[+] SimpleTimerTask(" << butil::class_name<T>() << ") thread finished";
    }
};


} // namespace utils
} // namespace server
