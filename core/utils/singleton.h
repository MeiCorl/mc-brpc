#pragma once
#include <pthread.h>

namespace server {
namespace utils {

template <typename T>
class Singleton {
private:
    static pthread_once_t _pOnce;
    static T* pInstance;

    Singleton();
    ~Singleton();
    static void _new() { pInstance = new T(); }

public:
    static T* get() {
        pthread_once(&_pOnce, &Singleton::_new);
        return pInstance;
    }
};

template <typename T>
pthread_once_t Singleton<T>::_pOnce = PTHREAD_ONCE_INIT;

template <typename T>
T* Singleton<T>::pInstance = nullptr;

} // namespace utils
} // namespace server