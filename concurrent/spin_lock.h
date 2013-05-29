#ifndef CONCURRENT_SPIN_LOCK_H_
#define CONCURRENT_SPIN_LOCK_H_
#include <atomic>
#include <thread>

namespace concurrent
{

template<class T>
void spin_wait(T pred)
{
    int step = 0;
    while (!pred())
        if (++step > 1000)
            std::this_thread::yield();
}

class spin_lock
{
public:
    spin_lock() : spin_(0) {}

    void lock()
    {
        spin_wait([this](){ return try_lock(); });
    }
    void unlock()
    {
        spin_.store(0, std::memory_order_release);
    }

private:
    bool try_lock()
    {
        return spin_.exchange(1, std::memory_order_acquire) == 0;
    }

private:
    std::atomic<int> spin_;

    //noncopyable
    spin_lock(spin_lock const&);
    spin_lock& operator=(spin_lock const&);
};

}//namespace concurrent


#endif //CONCURRENT_SPIN_LOCK_H_