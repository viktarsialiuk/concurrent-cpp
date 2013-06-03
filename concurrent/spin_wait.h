#ifndef CONCURRENT_SPIN_WAIT_H_
#define CONCURRENT_SPIN_WAIT_H_
#include <thread>


#ifdef _WIN32
#include <intrin.h>     //for _mm_pause intrinsic
#endif

namespace concurrent
{

//spinning algorithm by Dmitry Vyukov
//please refere http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning for more details

#ifdef _WIN32

template<class T>
void spin_wait(T pred)
{
    int backoff = 0;
    while (!pred())
    {
        if (backoff < 10)
            _mm_pause();
        else if (backoff < 20)
            for (int i = 0; i != 50; ++i) _mm_pause();
        else if (backoff < 22)
            std::this_thread::yield();
        else if (backoff < 24)
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
        else if (backoff < 26)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ++backoff;
    }
}
#endif



}//namespace concurrent


#endif //CONCURRENT_SPIN_LOCK_H_