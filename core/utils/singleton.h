#pragma once
#include <pthread.h>

// 取消使用，请使用butil/memory/singleton.h
namespace {
template <typename T>
class UnUseSingleton {
private:
    static pthread_once_t _pOnce;
    static T* pInstance;

    static void _new() {
        pInstance = new T();
    }

public:
    static T* get() {
        pthread_once(&_pOnce, &UnUseSingleton::_new);
        return pInstance;
    }
};

template <typename T>
pthread_once_t UnUseSingleton<T>::_pOnce = PTHREAD_ONCE_INIT;

template <typename T>
T* UnUseSingleton<T>::pInstance = nullptr;
}  // namespace